#include "mysql_storage.h"
#include <mysql/mysql.h>
#include <wsjcpp_core.h>
#include <sstream>

MySqlStorageConnection::MySqlStorageConnection(MYSQL *pConn) {
    m_pConnection = pConn;
    TAG = "MySqlStorageConenction";
    m_nCreated = WsjcppCore::getCurrentTimeInMilliseconds();
}

// ----------------------------------------------------------------------

MySqlStorageConnection::~MySqlStorageConnection() {
    mysql_close(m_pConnection);
    // delete m_pConnection;
}

// ----------------------------------------------------------------------

bool MySqlStorageConnection::executeQuery(const std::string &sQuery) {
    // TODO statistics time
    std::lock_guard<std::mutex> lock(m_mtxConn);
    // WsjcppLog::info(TAG, "Try " + sQuery);
    if (mysql_query(m_pConnection, sQuery.c_str())) {
        WsjcppLog::err(TAG, "Problem on executeQuery \r\nQuery: " + sQuery);
        std::string sError(mysql_error(m_pConnection));
        WsjcppLog::err(TAG, "executeQuery error " + sError);
        return false;
    } else {
        // WsjcppLog::ok(TAG, "" + sQuery);
    }
    return true;
}

// ----------------------------------------------------------------------

std::string MySqlStorageConnection::lastDatabaseVersion() {
    std::lock_guard<std::mutex> lock(m_mtxConn);

    std::string sLastVersion = "";
    std::string sQuery = "SELECT version FROM updates ORDER BY id DESC LIMIT 0,1";

    if (mysql_query(m_pConnection, sQuery.c_str())) {
        std::string sError(mysql_error(m_pConnection));
        if (sError.find("updates' doesn't exist") != std::string::npos) {
            WsjcppLog::warn(TAG, "Creating table updates .... ");
            std::string sTableDbUpdates = 
                "CREATE TABLE IF NOT EXISTS updates ("
                "  id INT NOT NULL AUTO_INCREMENT,"
                "  version varchar(255) DEFAULT NULL,"
                "  description text,"
                "  datetime_update datetime DEFAULT NULL,"
                "  PRIMARY KEY (`id`)"
                ") ENGINE=InnoDB  DEFAULT CHARSET=utf8 AUTO_INCREMENT=1;";
            if (mysql_query(m_pConnection, sTableDbUpdates.c_str())) {
                std::string sError2(mysql_error(m_pConnection));
                WsjcppLog::err(TAG, "Problem on create table updates " + sError2);
                return "error";
            } else {
                WsjcppLog::ok(TAG, "Table updates success created");
                sLastVersion = "";
                return "";
            }
        } else {
            WsjcppLog::err(TAG, "Problem with database " + sError);
            return "error";
        }
    } else {
        MYSQL_RES *pRes = mysql_use_result(m_pConnection);
        MYSQL_ROW row;
        // output table name
        if ((row = mysql_fetch_row(pRes)) != NULL) {
            sLastVersion = std::string(row[0]);
        }
        mysql_free_result(pRes);
    }
    return sLastVersion;
}

// ----------------------------------------------------------------------

std::vector<std::string> MySqlStorageConnection::getInstalledVersions() {
    std::lock_guard<std::mutex> lock(m_mtxConn);
    std::vector<std::string> vVersions;

    std::string sQuery = "SELECT version FROM updates ORDER BY id";

    if (mysql_query(m_pConnection, sQuery.c_str())) {
        std::string sError(mysql_error(m_pConnection));
        if (sError.find("updates' doesn't exist") != std::string::npos) {
            WsjcppLog::warn(TAG, "Creating table updates .... ");
            std::string sTableDbUpdates = 
                "CREATE TABLE IF NOT EXISTS updates ("
                "  id INT NOT NULL AUTO_INCREMENT,"
                "  version varchar(255) DEFAULT NULL,"
                "  description text,"
                "  datetime_update datetime DEFAULT NULL,"
                "  PRIMARY KEY (`id`)"
                ") ENGINE=InnoDB  DEFAULT CHARSET=utf8 AUTO_INCREMENT=1;";
            if (mysql_query(m_pConnection, sTableDbUpdates.c_str())) {
                std::string sError2(mysql_error(m_pConnection));
                WsjcppLog::throw_err(TAG, "Problem on create table updates " + sError2);
            } else {
                WsjcppLog::ok(TAG, "Table updates success created");
                return vVersions;
            }
        } else {
            WsjcppLog::throw_err(TAG, "Problem with database " + sError);
        }
    } else {
        MYSQL_RES *pRes = mysql_use_result(m_pConnection);
        MYSQL_ROW row;
        // output table name
        while ((row = mysql_fetch_row(pRes)) != NULL) {
            vVersions.push_back(std::string(row[0]));
        }
        mysql_free_result(pRes);
    }
    return vVersions;
}

bool MySqlStorageConnection::insertUpdateInfo(const std::string &sVersion, const std::string &sDescription) {
    std::lock_guard<std::mutex> lock(m_mtxConn);
    std::string sInsertNewVersion = "INSERT INTO updates(version, description, datetime_update) "
        " VALUES(" + this->prepareStringValue(sVersion) + ", " + this->prepareStringValue(sDescription) + ",NOW());";
    if (mysql_query(m_pConnection, sInsertNewVersion.c_str())) {
        WsjcppLog::err(TAG, "Could not insert row to updates: " + std::string(mysql_error(m_pConnection)));
        return false;
    }
    return true;
}

long MySqlStorageConnection::getConnectionDurationInSeconds() {
    long nRet = WsjcppCore::getCurrentTimeInMilliseconds() - m_nCreated;
    nRet = nRet / 1000;
    return nRet;
}

std::string MySqlStorageConnection::prepareStringValue(const std::string &sValue) {
    // escaping simbols  NUL (ASCII 0), \n, \r, \, ', ", и Control-Z.
    std::string sResult;
    sResult.reserve(sValue.size()*2);
    sResult.push_back('"');
    for (int i = 0; i < sValue.size(); i++) {
        char c = sValue[i];
        if (c == '\n') {
            sResult.push_back('\\');
            sResult.push_back('n');
        } else if (c == '\r') {
            sResult.push_back('\\');
            sResult.push_back('r');
        } else if (c == '\\' || c == '"' || c == '\'') {
            sResult.push_back('\\');
            sResult.push_back(c);
        } else if (c == 0) {
            sResult.push_back('\\');
            sResult.push_back('0');
        } else {
            sResult.push_back(c);
        }
    }
    sResult.push_back('"');
    return sResult;
}

int MySqlStorageConnection::paramtoInt(const char *p) {
    std::stringstream intValue(p);
    int number = 0;
    intValue >> number;
    return number;
}

std::vector<ModelVersion> MySqlStorageConnection::getApiVersionsAll() {
    std::lock_guard<std::mutex> lock(m_mtxConn);
    std::vector<ModelVersion> vRet;

    std::string sQuery = "SELECT id, `name` FROM webdiff_versions ORDER BY `name`";

    if (mysql_query(m_pConnection, sQuery.c_str())) {
        std::string sError(mysql_error(m_pConnection));
        WsjcppLog::throw_err(TAG, "Problem with database " + sError);
    } else {
        MYSQL_RES *pRes = mysql_use_result(m_pConnection);
        MYSQL_ROW row;
        // output table name
        while ((row = mysql_fetch_row(pRes)) != NULL) {
            ModelVersion model;
            model.setId(paramtoInt(row[0]));
            model.setName(std::string(row[1]));
            vRet.push_back(model);
        }
        mysql_free_result(pRes);
    }
    return vRet;
}

std::vector<ModelGroup> MySqlStorageConnection::getGroupsAll() {
    std::lock_guard<std::mutex> lock(m_mtxConn);
    std::vector<ModelGroup> vRet;

    std::string sQuery = "SELECT id, `name` FROM webdiff_file_groups ORDER BY `name`";

    if (mysql_query(m_pConnection, sQuery.c_str())) {
        std::string sError(mysql_error(m_pConnection));
        WsjcppLog::throw_err(TAG, "Problem with database " + sError);
    } else {
        MYSQL_RES *pRes = mysql_use_result(m_pConnection);
        MYSQL_ROW row;
        // output table name
        while ((row = mysql_fetch_row(pRes)) != NULL) {
            ModelGroup model;
            model.setId(paramtoInt(row[0]));
            model.setName(std::string(row[1]));
            vRet.push_back(model);
        }
        mysql_free_result(pRes);
    }
    return vRet;
}

std::vector<ModelGroupForVersion> MySqlStorageConnection::getGroups(int nVersionId) {
    std::lock_guard<std::mutex> lock(m_mtxConn);
    std::vector<ModelGroupForVersion> vRet;

    std::string sQuery =
        " SELECT"
        "   version_id,"
        "   file_group_id,"
        "   `name` as title,"
        "   COUNT(*) as amount_of_files"
        " FROM webdiff_files"
        " INNER JOIN webdiff_file_groups t1 ON t1.id = file_group_id"
        " WHERE version_id = " + std::to_string(nVersionId) + 
        " GROUP BY version_id, file_group_id"
    ;
    if (mysql_query(m_pConnection, sQuery.c_str())) {
        std::string sError(mysql_error(m_pConnection));
        WsjcppLog::throw_err(TAG, "Problem with database " + sError);
    } else {
        MYSQL_RES *pRes = mysql_use_result(m_pConnection);
        MYSQL_ROW row;
        // output table name
        while ((row = mysql_fetch_row(pRes)) != NULL) {
            ModelGroupForVersion model;
            model.setVersionId(paramtoInt(row[0]));
            model.setId(paramtoInt(row[1]));
            model.setTitle(std::string(row[2]));
            model.setAmountOfFiles(paramtoInt(row[3]));
            vRet.push_back(model);
        }
        mysql_free_result(pRes);
    }
    return vRet;
}

std::vector<ModelFile> MySqlStorageConnection::getFiles(int nVersionId, int nGroupId, int nParentId) {
    std::lock_guard<std::mutex> lock(m_mtxConn);
    std::vector<ModelFile> vRet;
    std::string sQuery =
        " SELECT"
        "   t0.version_id,"
        "   t0.file_group_id,"
        "   t0.id as file_id,"
        "   t0.amount_of_children,"
//        "   t1.filepath,"
        "   t1.filename as title"
        " FROM webdiff_files t0"
        " INNER JOIN webdiff_define_files t1 ON t1.id = define_file_id"
        " WHERE t0.version_id = " + std::to_string(nVersionId) +
            " AND file_group_id = " + std::to_string(nGroupId) + 
            " AND t0.parent_id = " + std::to_string(nParentId) +
        " ORDER BY t0.amount_of_children <> 0 DESC, title"
    ;
    // WsjcppLog::info(TAG, sQuery);
    if (mysql_query(m_pConnection, sQuery.c_str())) {
        std::string sError(mysql_error(m_pConnection));
        WsjcppLog::throw_err(TAG, "Problem with database " + sError);
    } else {
        MYSQL_RES *pRes = mysql_use_result(m_pConnection);
        MYSQL_ROW row;
        // output table name
        while ((row = mysql_fetch_row(pRes)) != NULL) {
            ModelFile model;
            model.setVersionId(paramtoInt(row[0]));
            model.setGroupId(paramtoInt(row[1]));
            model.setId(paramtoInt(row[2]));
            model.setAmountOfChildren(paramtoInt(row[3]));
            model.setTitle(std::string(row[4]));
            vRet.push_back(model);
        }
        mysql_free_result(pRes);
    }
    return vRet;
}


// ----------------------------------------------------------------------
// MySqlStorage

MySqlStorage::MySqlStorage() {
    TAG = "MySqlStorage";
    m_nConnectionOutdatedAfterSeconds = 60*60; // 1 hour

    if (!WsjcppCore::getEnv("WEBDIFF_DB_HOST", m_sDatabaseHost)) {
        m_sDatabaseHost = "localhost";
    }

    if (!WsjcppCore::getEnv("WEBDIFF_DB_NAME", m_sDatabaseName)) {
        m_sDatabaseName = "webdiff";
    }

    if (!WsjcppCore::getEnv("WEBDIFF_DB_USER", m_sDatabaseUser)) {
        m_sDatabaseUser = "webdiffu";
    }

    if (!WsjcppCore::getEnv("WEBDIFF_DB_PASS", m_sDatabasePass)) {
        m_sDatabasePass = "jET3E4W9vm";
    }

    std::string sDatabasePort;
    if (WsjcppCore::getEnv("WEBDIFF_DB_PORT", sDatabasePort)) {
        std::stringstream intValue(sDatabasePort);
        int number = 0;
        intValue >> number;
    } else {
        m_nDatabasePort = 3306;
    }
    
    WsjcppLog::info(TAG, "Database host: " + m_sDatabaseHost);
    WsjcppLog::info(TAG, "Database port: " + std::to_string(m_nDatabasePort));
    WsjcppLog::info(TAG, "Database name: " + m_sDatabaseName);
    WsjcppLog::info(TAG, "Database user: " + m_sDatabaseUser);
    WsjcppLog::info(TAG, "Database password: (hided)");
}

MySqlStorageConnection * MySqlStorage::connect() {
    MySqlStorageConnection *pConn = nullptr;
    MYSQL *pDatabase = mysql_init(NULL);
    if (!mysql_real_connect(pDatabase, 
            m_sDatabaseHost.c_str(),
            m_sDatabaseUser.c_str(),
            m_sDatabasePass.c_str(),
            m_sDatabaseName.c_str(), 
            m_nDatabasePort, NULL, 0)) {
        WsjcppLog::err(TAG, "Connect error: " + std::string(mysql_error(pDatabase)));
        WsjcppLog::err(TAG, "Failed to connect.");
    } else {
        pConn = new MySqlStorageConnection(pDatabase);
    }
    return pConn;
}

MySqlStorageConnection * MySqlStorage::getConnection() {
    std::lock_guard<std::mutex> lock(m_mtxStorageConnections);
    
    if (m_vDoRemoveStorageConnections.size() > 0) {
        WsjcppLog::warn(TAG, "TODO cleanup m_vDoRemoveStorageConnections, size = " + std::to_string(m_vDoRemoveStorageConnections.size()));
    }

    std::string sThreadId = WsjcppCore::getThreadId();
    MySqlStorageConnection *pStorageConnection = nullptr;
    std::map<std::string, MySqlStorageConnection *>::iterator it;
    it = m_mapStorageConnections.find(sThreadId);
    if (it == m_mapStorageConnections.end()) {
        pStorageConnection = this->connect();
        if (pStorageConnection == nullptr) {
            return nullptr;
        }
        m_mapStorageConnections[sThreadId] = pStorageConnection;
    } else {
        pStorageConnection = it->second;
        // if connection outdated just reconnect this also maybe need keep several time last connection
        if (pStorageConnection->getConnectionDurationInSeconds() > m_nConnectionOutdatedAfterSeconds) {
            m_vDoRemoveStorageConnections.push_back(pStorageConnection);
            pStorageConnection = this->connect();
            if (pStorageConnection == nullptr) {
                return nullptr;
            }
            m_mapStorageConnections[sThreadId] = pStorageConnection;
        }
    }
    return pStorageConnection;
}

bool MySqlStorage::loadCache() {
    MySqlStorageConnection *pConn = getConnection();
    m_vVersions = pConn->getApiVersionsAll();
    m_vGroups = pConn->getGroupsAll();
    return true;
}

const std::vector<ModelVersion> &MySqlStorage::getVersionsAll() {
    // from cache
    return m_vVersions;
}

const std::vector<ModelGroup> &MySqlStorage::getGroupsAll() {
    // from cache
    return m_vGroups;
}

std::vector<ModelGroupForVersion> MySqlStorage::getGroups(int nVersionId) {
    MySqlStorageConnection *pConn = getConnection();
    return pConn->getGroups(nVersionId);
}

std::vector<ModelFile> MySqlStorage::getFiles(int nVersionId, int nGroupId, int nParentId) {
    MySqlStorageConnection *pConn = getConnection();
    return pConn->getFiles(nVersionId, nGroupId, nParentId);
}