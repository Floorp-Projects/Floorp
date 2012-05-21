/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

const EXCEPTION_TABLE_NAME = "exceptions";

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
      DbUtils.createTable(this._connection, this._tableName, schema);
    } catch(e) {
      logger.warn("Error in createTable: " + e + "\n");
    }

    // Create a second table for storing exceptions for this study.  It has a fixed
    // name and schema:
    let exceptionTableSchema = "CREATE TABLE " + EXCEPTION_TABLE_NAME +
      " (time INTEGER, trace TEXT);";
    try {
      DbUtils.createTable(this._connection, "exceptions", exceptionTableSchema);
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
        case TYPE_INT_32: case TYPE_DOUBLE:
          insStmt.params[i] = datum;
        break;
        case TYPE_STRING:
          insStmt.params[i] = sanitizeString(datum);
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

  logException: function EDS_logException(exception) {
    let insertSql = "INSERT INTO " + EXCEPTION_TABLE_NAME + " VALUES (?1, ?2);";
    let insStmt =  this._createStatement(insertSql);
    // Even though the SQL says ?1 and ?2, the param indices count from 0.
    insStmt.params[0] = Date.now();
    let txt = exception.message ? exception.message : exception.toString();
    insStmt.params[1] = txt;
    insStmt.executeAsync();
    insStmt.finalize(); // TODO Is this the right thing to do when calling asynchronously?
  },

  getJSONRows: function EDS_getJSONRows(callback) {
    // TODO why do both this function and getAllDataAsJSON exist when they both do
    // the same thing?
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

  getExceptionsAsJson: function(callback) {
    let selectSql = "SELECT * FROM " + EXCEPTION_TABLE_NAME;
    let selStmt = this._createStatement(selectSql);
    let records = [];
    let self = this;

    selStmt.executeAsync({
      handleResult: function(aResultSet) {
        for (let row = aResultSet.getNextRow(); row;
             row = aResultSet.getNextRow()) {
          records.push({ time: row.getDouble(0),
                         exception: row.getString(1)});
        }
      },
      handleError: function(aError) {
        callback(records);
      },
      handleCompletion: function(aReason) {
        callback(records);
      }
    });
  },

  wipeAllData: function EDS_wipeAllData(callback) {
    // Wipe both the data table and the exception table; call callback when both
    // are wiped.
    let logger = Log4Moz.repository.getLogger("TestPilot.Database");
    logger.trace("ExperimentDataStore.wipeAllData called.\n");
    let wipeDataStmt = this._createStatement("DELETE FROM " + this._tableName);
    let wipeExcpStmt = this._createStatement("DELETE FROM " + EXCEPTION_TABLE_NAME);

    let numberWiped = 0;
    let onComplete = function() {
      numberWiped ++;
      if (numberWiped == 2 && callback) {
        callback();
      }
    };
    wipeDataStmt.executeAsync({
      handleResult: function(aResultSet) {},
      handleError: function(aError) { onComplete(); },
      handleCompletion: function(aReason) {
        if (aReason == Ci.mozIStorageStatementCallback.REASON_FINISHED) {
          logger.trace("ExperimentDataStore.wipeAllData complete.\n");
        }
        onComplete();
      }
    });
    wipeExcpStmt.executeAsync({
      handleResult: function(aResultSet) {},
      handleError: function(aError) {  onComplete(); },
      handleCompletion: function(aReason) { onComplete(); }
    });
    wipeDataStmt.finalize();
    wipeExcpStmt.finalize();
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
