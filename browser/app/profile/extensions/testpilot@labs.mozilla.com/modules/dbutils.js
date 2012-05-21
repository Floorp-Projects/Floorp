/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Ci = Components.interfaces;
var Cc = Components.classes;
var Cu = Components.utils;
var EXPORTED_SYMBOLS = ["DbUtils"];

Cu.import("resource://testpilot/modules/log4moz.js");

/* Make a namespace object called DbUtils, to export,
 * which contains each function in this file.*/
var DbUtils = ([f for each (f in this) if (typeof f === "function")]
                 .reduce(function(o, f)(o[f.name] = f, o), {}));

var _dirSvc = Cc["@mozilla.org/file/directory_service;1"]
                .getService(Ci.nsIProperties);
var _storSvc = Cc["@mozilla.org/storage/service;1"]
                 .getService(Ci.mozIStorageService);

DbUtils.openDatabase = function openDatabase(file) {
  /* If the pointed-at file doesn't already exist, it means the database
   * has never been initialized */
  let logger = Log4Moz.repository.getLogger("TestPilot.Database");
  let connection = null;
  try {
    logger.debug("Trying to open file...\n");
    connection = _storSvc.openDatabase(file);
    logger.debug("Opening file done...\n");
  } catch(e) {
    logger.debug("Opening file failed...\n");
    Components.utils.reportError(
      "Opening database failed, database may not have been initialized");
  }
  return connection;
};

DbUtils.createTable = function createTable(connection, tableName, schema){
  let logger = Log4Moz.repository.getLogger("TestPilot.Database");
  let file = connection.databaseFile;
  logger.debug("File is " + file + "\n");
  try{
    if(!connection.tableExists(tableName)){
      // TODO why don't we use connection.createTable here??
      connection.executeSimpleSQL(schema);
    } else{
      logger.debug("database table: " + tableName + " already exists\n");
    }
  }
  catch(e) {
    logger.warn("Error creating database: " + e + "\n");
    Cu.reportError("Test Pilot's " + tableName +
        " database table appears to be corrupt, resetting it.");
    if(file.exists()){
      //remove corrupt database table
      file.remove(false);
    }
    connection = _storSvc.openDatabase(file);
    connection.executeSimpleSQL(schema);
  }
  return connection;
};
