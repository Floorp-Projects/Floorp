/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Ubiquity.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brandon Pung <brandonpung@gmail.com>
 *   Jono X <jono@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
