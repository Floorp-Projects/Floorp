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
  this._ensureConnection(pathRelativeToProfileDir);
};

this.SQLiteStore.prototype = {

  /**
   * Yields the number of items in the store that match the given options.
   *
   * @param optsList A variable number of options objects that control the
   *        items that are matched.  See Options Objects in ReadingList.jsm.
   * @return Promise<number> The number of matching items in the store.
   *         Rejected with an Error on error.
   */
  count: Task.async(function* (...optsList) {
    let [sql, args] = sqlFromOptions(optsList);
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
   * @param optsList A variable number of options objects that control the
   *        items that are matched.  See Options Objects in ReadingList.jsm.
   * @return Promise<null> Resolved when the enumeration completes.  Rejected
   *         with an Error on error.
   */
  forEachItem: Task.async(function* (callback, ...optsList) {
    let [sql, args] = sqlFromOptions(optsList);
    let colNames = ReadingList.ItemBasicPropertyNames;
    let conn = yield this._connectionPromise;
    yield conn.executeCached(`
      SELECT ${colNames} FROM items ${sql};
    `, args, row => callback(itemFromRow(row)));
  }),

  /**
   * Adds an item to the store that isn't already present.  See
   * ReadingList.prototype.addItems.
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
    yield conn.executeCached(`
      INSERT INTO items (${colNames}) VALUES (${paramNames});
    `, item);
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
    let assignments = [];
    for (let propName in item) {
      assignments.push(`${propName} = :${propName}`);
    }
    let conn = yield this._connectionPromise;
    yield conn.executeCached(`
      UPDATE items SET ${assignments} WHERE url = :url;
    `, item);
  }),

  /**
   * Deletes an item from the store.
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
   * Call this when you're done with the store.  Don't use it afterward.
   */
  destroy() {
    if (!this._destroyPromise) {
      this._destroyPromise = Task.spawn(function* () {
        let conn = yield this._connectionPromise;
        yield conn.close();
        this._connectionPromise = Promise.reject("Store destroyed");
      }.bind(this));
    }
    return this._destroyPromise;
  },

  /**
   * Creates the database connection if it hasn't been created already.
   *
   * @param pathRelativeToProfileDir The path of the database file relative to
   *        the profile directory.
   */
  _ensureConnection: Task.async(function* (pathRelativeToProfileDir) {
    if (!this._connectionPromise) {
      this._connectionPromise = Task.spawn(function* () {
        let conn = yield Sqlite.openConnection({
          path: pathRelativeToProfileDir,
          sharedMemoryCache: false,
        });
        Sqlite.shutdown.addBlocker("readinglist/SQLiteStore: Destroy",
                                   this.destroy.bind(this));
        yield conn.execute(`
          PRAGMA locking_mode = EXCLUSIVE;
        `);
        yield this._checkSchema(conn);
        return conn;
      }.bind(this));
    }
  }),

  // Promise<Sqlite.OpenedConnection>
  _connectionPromise: null,

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
    yield conn.execute(`
      CREATE TABLE items (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        guid TEXT UNIQUE,
        url TEXT NOT NULL UNIQUE,
        resolvedURL TEXT UNIQUE,
        lastModified INTEGER,
        title TEXT,
        resolvedTitle TEXT,
        excerpt TEXT,
        status INTEGER,
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
        preview TEXT
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
 * ReadingList.ItemBasicPropertyNames properties lifted from the given row.
 *
 * @param row A mozIStorageRow.
 * @return The item.
 */
function itemFromRow(row) {
  let item = {};
  for (let name of ReadingList.ItemBasicPropertyNames) {
    item[name] = row.getResultByName(name);
  }
  return item;
}

/**
 * Returns the back part of a SELECT statement generated from the given list of
 * options.
 *
 * @param optsList See Options Objects in ReadingList.jsm.
 * @return An array [sql, args].  sql is a string of SQL.  args is an object
 *         that contains arguments for all the parameters in sql.
 */
function sqlFromOptions(optsList) {
  // We modify the options objects, which were passed in by the store client, so
  // clone them first.
  optsList = Cu.cloneInto(optsList, {}, { cloneFunctions: false });

  let sort;
  let sortDir;
  let limit;
  let offset;
  for (let opts of optsList) {
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

  function uniqueParamName(name) {
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

  // Build a WHERE clause for the remaining properties.  Assume they all refer
  // to columns.  (If they don't, the SQL query will fail.)
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
          let paramName = uniqueParamName(key);
          params.push(`:${paramName}`);
          args[paramName] = array[i];
        }
        conjunctions.push(`${key} IN (${params})`);
      }
      else {
        let paramName = uniqueParamName(key);
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
  if (disjunction) {
    let where = `WHERE ${disjunction}`;
    fragments = [where].concat(fragments);
  }

  let sql = fragments.join(" ");
  return [sql, args];
}
