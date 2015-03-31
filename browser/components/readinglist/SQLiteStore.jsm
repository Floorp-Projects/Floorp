/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "SQLiteStore",
];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ReadingList",
  "resource:///modules/readinglist/ReadingList.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Sqlite",
  "resource://gre/modules/Sqlite.jsm");

/**
 * A SQLite Reading List store backed by a database on disk.  The database is
 * created if it doesn't exist.
 *
 * @param pathRelativeToProfileDir The path of the database file relative to
 *        the profile directory.
 */
this.SQLiteStore = function SQLiteStore(pathRelativeToProfileDir) {
  this.pathRelativeToProfileDir = pathRelativeToProfileDir;
};

this.SQLiteStore.prototype = {

  /**
   * Yields the number of items in the store that match the given options.
   *
   * @param userOptsList A variable number of options objects that control the
   *        items that are matched.  See Options Objects in ReadingList.jsm.
   * @param controlOpts A single options object.  Use this to filter out items
   *        that don't match it -- in other words, to override the user options.
   *        See Options Objects in ReadingList.jsm.
   * @return Promise<number> The number of matching items in the store.
   *         Rejected with an Error on error.
   */
  count: Task.async(function* (userOptsList=[], controlOpts={}) {
    let [sql, args] = sqlWhereFromOptions(userOptsList, controlOpts);
    let count = 0;
    let conn = yield this._connectionPromise;
    yield conn.executeCached(`
      SELECT COUNT(*) AS count FROM items ${sql};
    `, args, row => count = row.getResultByName("count"));
    return count;
  }),

  /**
   * Enumerates the items in the store that match the given options.
   *
   * @param callback Called for each item in the enumeration.  It's passed a
   *        single object, an item.
   * @param userOptsList A variable number of options objects that control the
   *        items that are matched.  See Options Objects in ReadingList.jsm.
   * @param controlOpts A single options object.  Use this to filter out items
   *        that don't match it -- in other words, to override the user options.
   *        See Options Objects in ReadingList.jsm.
   * @return Promise<null> Resolved when the enumeration completes.  Rejected
   *         with an Error on error.
   */
  forEachItem: Task.async(function* (callback, userOptsList=[], controlOpts={}) {
    let [sql, args] = sqlWhereFromOptions(userOptsList, controlOpts);
    let colNames = ReadingList.ItemRecordProperties;
    let conn = yield this._connectionPromise;
    yield conn.executeCached(`
      SELECT ${colNames} FROM items ${sql};
    `, args, row => callback(itemFromRow(row)));
  }),

  /**
   * Adds an item to the store that isn't already present.  See
   * ReadingList.prototype.addItem.
   *
   * @param items A simple object representing an item.
   * @return Promise<null> Resolved when the store is updated.  Rejected with an
   *         Error on error.
   */
  addItem: Task.async(function* (item) {
    let colNames = [];
    let paramNames = [];
    for (let propName in item) {
      colNames.push(propName);
      paramNames.push(`:${propName}`);
    }
    let conn = yield this._connectionPromise;
    try {
      yield conn.executeCached(`
        INSERT INTO items (${colNames}) VALUES (${paramNames});
      `, item);
    }
    catch (err) {
      throwExistsError(err);
    }
  }),

  /**
   * Updates the properties of an item that's already present in the store.  See
   * ReadingList.prototype.updateItem.
   *
   * @param item The item to update.  It must have a `url`.
   * @return Promise<null> Resolved when the store is updated.  Rejected with an
   *         Error on error.
   */
  updateItem: Task.async(function* (item) {
    yield this._updateItem(item, "url");
  }),

  /**
   * Same as updateItem, but the item is keyed off of its `guid` instead of its
   * `url`.
   *
   * @param item The item to update.  It must have a `guid`.
   * @return Promise<null> Resolved when the store is updated.  Rejected with an
   *         Error on error.
   */
  updateItemByGUID: Task.async(function* (item) {
    yield this._updateItem(item, "guid");
  }),

  /**
   * Deletes an item from the store by its URL.
   *
   * @param url The URL string of the item to delete.
   * @return Promise<null> Resolved when the store is updated.  Rejected with an
   *         Error on error.
   */
  deleteItemByURL: Task.async(function* (url) {
    let conn = yield this._connectionPromise;
    yield conn.executeCached(`
      DELETE FROM items WHERE url = :url;
    `, { url: url });
  }),

  /**
   * Deletes an item from the store by its GUID.
   *
   * @param guid The GUID string of the item to delete.
   * @return Promise<null> Resolved when the store is updated.  Rejected with an
   *         Error on error.
   */
  deleteItemByGUID: Task.async(function* (guid) {
    let conn = yield this._connectionPromise;
    yield conn.executeCached(`
      DELETE FROM items WHERE guid = :guid;
    `, { guid: guid });
  }),

  /**
   * Call this when you're done with the store.  Don't use it afterward.
   */
  destroy() {
    if (!this._destroyPromise) {
      this._destroyPromise = Task.spawn(function* () {
        let conn = yield this._connectionPromise;
        yield conn.close();
        this.__connectionPromise = Promise.reject("Store destroyed");
      }.bind(this));
    }
    return this._destroyPromise;
  },

  /**
   * Promise<Sqlite.OpenedConnection>
   */
  get _connectionPromise() {
    if (!this.__connectionPromise) {
      this.__connectionPromise = this._createConnection();
    }
    return this.__connectionPromise;
  },

  /**
   * Creates the database connection.
   *
   * @return Promise<Sqlite.OpenedConnection>
   */
  _createConnection: Task.async(function* () {
    let conn = yield Sqlite.openConnection({
      path: this.pathRelativeToProfileDir,
      sharedMemoryCache: false,
    });
    Sqlite.shutdown.addBlocker("readinglist/SQLiteStore: Destroy",
                               this.destroy.bind(this));
    yield conn.execute(`
      PRAGMA locking_mode = EXCLUSIVE;
    `);
    yield this._checkSchema(conn);
    return conn;
  }),

  /**
   * Updates the properties of an item that's already present in the store.  See
   * ReadingList.prototype.updateItem.
   *
   * @param item The item to update.  It must have the property named by
   *        keyProp.
   * @param keyProp The item is keyed off of this property.
   * @return Promise<null> Resolved when the store is updated.  Rejected with an
   *         Error on error.
   */
  _updateItem: Task.async(function* (item, keyProp) {
    let assignments = [];
    for (let propName in item) {
      assignments.push(`${propName} = :${propName}`);
    }
    let conn = yield this._connectionPromise;
    if (!item[keyProp]) {
      throw new ReadingList.Error.Error("Item must have " + keyProp);
    }
    try {
      yield conn.executeCached(`
        UPDATE items SET ${assignments} WHERE ${keyProp} = :${keyProp};
      `, item);
    }
    catch (err) {
      throwExistsError(err);
    }
  }),

  // The current schema version.
  _schemaVersion: 1,

  _checkSchema: Task.async(function* (conn) {
    let version = parseInt(yield conn.getSchemaVersion());
    for (; version < this._schemaVersion; version++) {
      let meth = `_migrateSchema${version}To${version + 1}`;
      yield this[meth](conn);
    }
    yield conn.setSchemaVersion(this._schemaVersion);
  }),

  _migrateSchema0To1: Task.async(function* (conn) {
    yield conn.execute(`
      PRAGMA journal_mode = wal;
    `);
    // 524288 bytes = 512 KiB
    yield conn.execute(`
      PRAGMA journal_size_limit = 524288;
    `);
    // Not important, but FYI: The order that these columns are listed in
    // follows the order that the server doc lists the fields in the article
    // data model, more or less:
    // http://readinglist.readthedocs.org/en/latest/model.html
    yield conn.execute(`
      CREATE TABLE items (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        guid TEXT UNIQUE,
        serverLastModified INTEGER,
        url TEXT UNIQUE,
        preview TEXT,
        title TEXT,
        resolvedURL TEXT UNIQUE,
        resolvedTitle TEXT,
        excerpt TEXT,
        archived BOOLEAN,
        deleted BOOLEAN,
        favorite BOOLEAN,
        isArticle BOOLEAN,
        wordCount INTEGER,
        unread BOOLEAN,
        addedBy TEXT,
        addedOn INTEGER,
        storedOn INTEGER,
        markedReadBy TEXT,
        markedReadOn INTEGER,
        readPosition INTEGER,
        syncStatus INTEGER
      );
    `);
    yield conn.execute(`
      CREATE INDEX items_addedOn ON items (addedOn);
    `);
    yield conn.execute(`
      CREATE INDEX items_unread ON items (unread);
    `);
  }),
};

/**
 * Returns a simple object whose properties are the
 * ReadingList.ItemRecordProperties lifted from the given row.
 *
 * @param row A mozIStorageRow.
 * @return The item.
 */
function itemFromRow(row) {
  let item = {};
  for (let name of ReadingList.ItemRecordProperties) {
    item[name] = row.getResultByName(name);
  }
  return item;
}

/**
 * If the given Error indicates that a unique constraint failed, then wraps that
 * error in a ReadingList.Error.Exists and throws it.  Otherwise throws the
 * given error.
 *
 * @param err An Error object.
 */
function throwExistsError(err) {
  let match =
    /UNIQUE constraint failed: items\.([a-zA-Z0-9_]+)/.exec(err.message);
  if (match) {
    let newErr = new ReadingList.Error.Exists(
      "An item with the following property already exists: " + match[1]
    );
    newErr.originalError = err;
    err = newErr;
  }
  throw err;
}

/**
 * Returns the back part of a SELECT statement generated from the given list of
 * options.
 *
 * @param userOptsList A variable number of options objects that control the
 *        items that are matched.  See Options Objects in ReadingList.jsm.
 * @param controlOpts A single options object.  Use this to filter out items
 *        that don't match it -- in other words, to override the user options.
 *        See Options Objects in ReadingList.jsm.
 * @return An array [sql, args].  sql is a string of SQL.  args is an object
 *         that contains arguments for all the parameters in sql.
 */
function sqlWhereFromOptions(userOptsList, controlOpts) {
  // We modify the options objects in userOptsList, which were passed in by the
  // store client, so clone them first.
  userOptsList = Cu.cloneInto(userOptsList, {}, { cloneFunctions: false });

  let sort;
  let sortDir;
  let limit;
  let offset;
  for (let opts of userOptsList) {
    if ("sort" in opts) {
      sort = opts.sort;
      delete opts.sort;
    }
    if ("descending" in opts) {
      if (opts.descending) {
        sortDir = "DESC";
      }
      delete opts.descending;
    }
    if ("limit" in opts) {
      limit = opts.limit;
      delete opts.limit;
    }
    if ("offset" in opts) {
      offset = opts.offset;
      delete opts.offset;
    }
  }

  let fragments = [];

  if (sort) {
    sortDir = sortDir || "ASC";
    fragments.push(`ORDER BY ${sort} ${sortDir}`);
  }
  if (limit) {
    fragments.push(`LIMIT ${limit}`);
    if (offset) {
      fragments.push(`OFFSET ${offset}`);
    }
  }

  let args = {};
  let mainExprs = [];

  let controlSQLExpr = sqlExpressionFromOptions([controlOpts], args);
  if (controlSQLExpr) {
    mainExprs.push(`(${controlSQLExpr})`);
  }

  let userSQLExpr = sqlExpressionFromOptions(userOptsList, args);
  if (userSQLExpr) {
    mainExprs.push(`(${userSQLExpr})`);
  }

  if (mainExprs.length) {
    let conjunction = mainExprs.join(" AND ");
    fragments.unshift(`WHERE ${conjunction}`);
  }

  let sql = fragments.join(" ");
  return [sql, args];
}

/**
 * Returns a SQL expression generated from the given options list.  Each options
 * object in the list generates a subexpression, and all the subexpressions are
 * OR'ed together to produce the final top-level expression.  (e.g., an optsList
 * with three options objects would generate an expression like "(guid = :guid
 * OR (title = :title AND unread = :unread) OR resolvedURL = :resolvedURL)".)
 *
 * All the properties of the options objects are assumed to refer to columns in
 * the database.  If they don't, your SQL query will fail.
 *
 * @param optsList See Options Objects in ReadingList.jsm.
 * @param args An object that will hold the SQL parameters.  It will be
 *        modified.
 * @return A string of SQL.  Also, args will contain arguments for all the
 *         parameters in the SQL.
 */
function sqlExpressionFromOptions(optsList, args) {
  let disjunctions = [];
  for (let opts of optsList) {
    let conjunctions = [];
    for (let key in opts) {
      if (Array.isArray(opts[key])) {
        // Convert arrays to IN expressions.  e.g., { guid: ['a', 'b', 'c'] }
        // becomes "guid IN (:guid, :guid_1, :guid_2)".  The guid_i arguments
        // are added to opts.
        let array = opts[key];
        let params = [];
        for (let i = 0; i < array.length; i++) {
          let paramName = uniqueParamName(args, key);
          params.push(`:${paramName}`);
          args[paramName] = array[i];
        }
        conjunctions.push(`${key} IN (${params})`);
      }
      else {
        let paramName = uniqueParamName(args, key);
        conjunctions.push(`${key} = :${paramName}`);
        args[paramName] = opts[key];
      }
    }
    let conjunction = conjunctions.join(" AND ");
    if (conjunction) {
      disjunctions.push(`(${conjunction})`);
    }
  }
  let disjunction = disjunctions.join(" OR ");
  return disjunction;
}

/**
 * Returns a version of the given name such that it doesn't conflict with the
 * name of any property in args.  e.g., if name is "foo" but args already has
 * properties named "foo", "foo1", and "foo2", then "foo3" is returned.
 *
 * @param args An object.
 * @param name The name you want to use.
 * @return A unique version of the given name.
 */
function uniqueParamName(args, name) {
  if (name in args) {
    for (let i = 1; ; i++) {
      let newName = `${name}_${i}`;
      if (!(newName in args)) {
        return newName;
      }
    }
  }
  return name;
}
