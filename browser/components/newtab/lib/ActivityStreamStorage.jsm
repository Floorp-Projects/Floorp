/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "IndexedDB",
  "resource://gre/modules/IndexedDB.jsm"
);

class ActivityStreamStorage {
  /**
   * @param storeNames Array of strings used to create all the required stores
   */
  constructor({ storeNames, telemetry }) {
    if (!storeNames) {
      throw new Error("storeNames required");
    }

    this.dbName = "ActivityStream";
    this.dbVersion = 3;
    this.storeNames = storeNames;
    this.telemetry = telemetry;
  }

  get db() {
    return this._db || (this._db = this.createOrOpenDb());
  }

  /**
   * Public method that binds the store required by the consumer and exposes
   * the private db getters and setters.
   *
   * @param storeName String name of desired store
   */
  getDbTable(storeName) {
    if (this.storeNames.includes(storeName)) {
      return {
        get: this._get.bind(this, storeName),
        getAll: this._getAll.bind(this, storeName),
        set: this._set.bind(this, storeName),
      };
    }

    throw new Error(`Store name ${storeName} does not exist.`);
  }

  async _getStore(storeName) {
    return (await this.db).objectStore(storeName, "readwrite");
  }

  _get(storeName, key) {
    return this._requestWrapper(async () =>
      (await this._getStore(storeName)).get(key)
    );
  }

  _getAll(storeName) {
    return this._requestWrapper(async () =>
      (await this._getStore(storeName)).getAll()
    );
  }

  _set(storeName, key, value) {
    return this._requestWrapper(async () =>
      (await this._getStore(storeName)).put(value, key)
    );
  }

  _openDatabase() {
    return lazy.IndexedDB.open(this.dbName, { version: this.dbVersion }, db => {
      // If provided with array of objectStore names we need to create all the
      // individual stores
      this.storeNames.forEach(store => {
        if (!db.objectStoreNames.contains(store)) {
          this._requestWrapper(() => db.createObjectStore(store));
        }
      });
    });
  }

  /**
   * createOrOpenDb - Open a db (with this.dbName) if it exists.
   *                  If it does not exist, create it.
   *                  If an error occurs, deleted the db and attempt to
   *                  re-create it.
   * @returns Promise that resolves with a db instance
   */
  async createOrOpenDb() {
    try {
      const db = await this._openDatabase();
      return db;
    } catch (e) {
      if (this.telemetry) {
        this.telemetry.handleUndesiredEvent({ event: "INDEXEDDB_OPEN_FAILED" });
      }
      await lazy.IndexedDB.deleteDatabase(this.dbName);
      return this._openDatabase();
    }
  }

  async _requestWrapper(request) {
    let result = null;
    try {
      result = await request();
    } catch (e) {
      if (this.telemetry) {
        this.telemetry.handleUndesiredEvent({ event: "TRANSACTION_FAILED" });
      }
      throw e;
    }

    return result;
  }
}

function getDefaultOptions(options) {
  return { collapsed: !!options.collapsed };
}

const EXPORTED_SYMBOLS = ["ActivityStreamStorage", "getDefaultOptions"];
