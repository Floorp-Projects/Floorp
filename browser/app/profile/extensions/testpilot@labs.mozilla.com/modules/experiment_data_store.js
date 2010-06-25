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
 * The Original Code is Test Pilot.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jono X <jono@mozilla.com>
 *   Raymond Lee <raymond@appcoast.com>
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

EXPORTED_SYMBOLS = ["ExperimentDataStore", "TYPE_INT_32", "TYPE_DOUBLE",
                   "TYPE_STRING"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://testpilot/modules/dbutils.js");
Cu.import("resource://testpilot/modules/log4moz.js");
Cu.import("resource://testpilot/modules/string_sanitizer.js");
var _dirSvc = Cc["@mozilla.org/file/directory_service;1"]
                .getService(Ci.nsIProperties);
var _storSvc = Cc["@mozilla.org/storage/service;1"]
                 .getService(Ci.mozIStorageService);

const TYPE_INT_32 = 0;
const TYPE_DOUBLE = 1;
const TYPE_STRING = 2;

function ExperimentDataStore(fileName, tableName, columns) {
  this._init(fileName, tableName, columns);
}
ExperimentDataStore.prototype = {
  _init: function EDS__init(fileName, tableName, columns) {
    this._fileName = fileName;
    this._tableName = tableName;
    this._columns = columns;
    let logger = Log4Moz.repository.getLogger("TestPilot.Database");
    let file = _dirSvc.get("ProfD", Ci.nsIFile);
    file.append(this._fileName);
    // openDatabase creates the file if it's not there yet:
    this._connection = DbUtils.openDatabase(file);
    // Create schema based on columns:
    let schemaClauses = [];
    for (let i = 0; i < this._columns.length; i++) {
      let colName = this._columns[i].property;
      let colType;
      switch( this._columns[i].type) {
      case TYPE_INT_32: case TYPE_DOUBLE:
        colType = "INTEGER";
        break;
      case TYPE_STRING:
        colType = "TEXT";
        break;

      }
      schemaClauses.push( colName + " " + colType );
    }
    let schema = "CREATE TABLE " + this._tableName + "("
                  + schemaClauses.join(", ") + ");";
    // CreateTable creates the table only if it does not already exist:
    try {
      this._connection = DbUtils.createTable(this._connection,
                                             this._tableName,
                                             schema);
    } catch(e) {
      logger.warn("Error in createTable: " + e + "\n");
    }
  },

  _createStatement: function _createStatement(selectSql) {
    try {
      var selStmt = this._connection.createStatement(selectSql);
      return selStmt;
    } catch (e) {
      throw new Error(this._connection.lastErrorString);
    }
  },

  storeEvent: function EDS_storeEvent(uiEvent, callback) {
    // Stores event asynchronously; callback will be called back with
    // true or false on success or failure.
    let columnNumbers = [];
    for (let i = 1; i <= this._columns.length; i++) {
      // the i = 1 is so that we'll start with 1 instead of 0... we want
      // a string like "...VALUES (?1, ?2, ?3)"
      columnNumbers.push( "?" + i);
    }
    let insertSql = "INSERT INTO " + this._tableName + " VALUES (";
    insertSql += columnNumbers.join(", ") + ")";
    let insStmt = this._createStatement(insertSql);
    for (i = 0; i < this._columns.length; i++) {
      let datum =  uiEvent[this._columns[i].property];
      switch (this._columns[i].type) {
        case TYPE_INT_32:
          insStmt.bindInt32Parameter(i, datum);
        break;
        case TYPE_DOUBLE:
          insStmt.bindDoubleParameter(i, datum);
        break;
        case TYPE_STRING:
          insStmt.bindUTF8StringParameter(i, sanitizeString(datum));
        break;
      }
    }
    insStmt.executeAsync({
      handleResult: function(aResultSet) {
      },
      handleError: function(aError) {
        if (callback) {
          callback(false);
        }
      },
      handleCompletion: function(aReason) {
        if (aReason == Ci.mozIStorageStatementCallback.REASON_FINISHED) {
          if (callback) {
            callback(true);
          }
        } else {
          if (callback) {
            callback(false);
          }
        }
      }
    });
    insStmt.finalize();
  },

  getJSONRows: function EDS_getJSONRows(callback) {
        let selectSql = "SELECT * FROM " + this._tableName;
    let selStmt = this._createStatement(selectSql);
    let records = [];
    let self = this;
    let numCols = selStmt.columnCount;

    selStmt.executeAsync({
      handleResult: function(aResultSet) {
        for (let row = aResultSet.getNextRow(); row;
             row = aResultSet.getNextRow()) {
          let newRecord = [];
          for (let i = 0; i < numCols; i++) {
            let column = self._columns[i];
            //let value = row.getResultByIndex(i);
            let value = 0;
            switch (column.type) {
              case TYPE_INT_32:
                value = row.getInt32(i);
              break;
              case TYPE_DOUBLE:
                value = row.getDouble(i);
              break;
              case TYPE_STRING:
                value = sanitizeString(row.getUTF8String(i));
              break;
            }
            newRecord.push(value);
          }
          records.push(newRecord);
        }
      },
      handleError: function(aError) {
        callback(records);
      },

      handleCompletion: function(aReason) {
        callback(records);
      }
    });
    selStmt.finalize();
  },

  getAllDataAsJSON: function EDS_getAllDataAsJSON( useDisplayValues, callback ) {
    /* if useDisplayValues is true, the values in the returned JSON are translated to
     * their human-readable equivalents, using the mechanism provided in the columns
     * set.  If it's false, the values in the returned JSON are straight numerical
     * values. */

    // Note this works without knowing what the schema is
    let selectSql = "SELECT * FROM " + this._tableName;
    let selStmt = this._createStatement(selectSql);
    let records = [];
    let self = this;
    let numCols = selStmt.columnCount;

    selStmt.executeAsync({
      handleResult: function(aResultSet) {
        for (let row = aResultSet.getNextRow(); row;
             row = aResultSet.getNextRow()) {
          let newRecord = {};
          for (let i = 0; i < numCols; i++) {
            let column = self._columns[i];
            //let value = row.getResultByIndex(i);
            let value = 0;
            switch (column.type) {
              case TYPE_INT_32:
                value = row.getInt32(i);
              break;
              case TYPE_DOUBLE:
                value = row.getDouble(i);
              break;
              case TYPE_STRING:
                value = sanitizeString(row.getUTF8String(i));
              break;
            }
            /* The column may have a property called displayValue, which can be either
             * a function returning a string or an array of strings.  If we're called
             * with useDisplayValues, then take the raw numeric value and either use it as
             * an index to the array of strings or use it as input to the function in order
             * to get the human-readable display name of the value. */
            if (useDisplayValues && column.displayValue != undefined) {
              if (typeof( column.displayValue) == "function") {
                newRecord[column.property] = column.displayValue(value);
              } else {
                newRecord[column.property] = column.displayValue[value];
              }
            } else {
              newRecord[column.property] = value;
            }
          }
          records.push(newRecord);
        }
      },
      handleError: function(aError) {
        callback(records);
      },

      handleCompletion: function(aReason) {
        callback(records);
      }
    });
    selStmt.finalize();
  },

  wipeAllData: function EDS_wipeAllData(callback) {
    let logger = Log4Moz.repository.getLogger("TestPilot.Database");
    logger.trace("ExperimentDataStore.wipeAllData called.\n");
    let wipeSql = "DELETE FROM " + this._tableName;
    let wipeStmt = this._createStatement(wipeSql);
    wipeStmt.executeAsync({
      handleResult: function(aResultSet) {
      },
      handleError: function(aError) {
        if (callback) {
          callback(false);
        }
      },
      handleCompletion: function(aReason) {
        if (aReason == Ci.mozIStorageStatementCallback.REASON_FINISHED) {
          logger.trace("ExperimentDataStore.wipeAllData complete.\n");
          if (callback) {
            callback(true);
          }
        } else {
          if (callback) {
            callback(false);
          }
        }
      }
    });
    wipeStmt.finalize();
  },

  nukeTable: function EDS_nukeTable() {
    // Should never be called, except if schema needs to be changed
    // during debugging/development.
    let nuke = this._createStatement("DROP TABLE " + this._tableName);
    nuke.executeAsync();
    nuke.finalize();
  },

  haveData: function EDS_haveData(callback) {
    let countSql = "SELECT * FROM " + this._tableName;
    let countStmt = this._createStatement(countSql);
    let hasData = false;
    countStmt.executeAsync({
      handleResult: function(aResultSet) {
        if (aResultSet.getNextRow()) {
          hasData = true;
        }
      },

      handleError: function(aError) {
        callback(false);
      },

      handleCompletion: function(aReason) {
        if (aReason == Ci.mozIStorageStatementCallback.REASON_FINISHED &&
            hasData) {
          callback(true);
        } else {
          callback(false);
        }
      }
    });
    countStmt.finalize();
  },

  getHumanReadableColumnNames: function EDS_getHumanReadableColumnNames() {
    let i;
    return [ this._columns[i].displayName for (i in this._columns) ];
  },

  getPropertyNames: function EDS_getPropertyNames() {
    let i;
    return [ this._columns[i].property for (i in this._columns) ];
  }
};
