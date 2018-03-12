ChromeUtils.defineModuleGetter(this, "IndexedDB", "resource://gre/modules/IndexedDB.jsm");

this.ActivityStreamStorage = class ActivityStreamStorage {
  constructor(storeName) {
    this.dbName = "ActivityStream";
    this.dbVersion = 2;
    this.storeName = storeName;

    this._db = null;
  }

  get db() {
    if (!this._db) {
      throw new Error("It looks like the db connection has not initialized yet. Are you use .init was called?");
    }
    return this._db;
  }

  getStore() {
    return this.db.objectStore(this.storeName, "readwrite");
  }

  get(key) {
    return this.getStore().get(key);
  }

  set(key, value) {
    return this.getStore().put(value, key);
  }

  _openDatabase() {
    return IndexedDB.open(this.dbName, {version: this.dbVersion}, db => {
      db.createObjectStore(this.storeName);
    });
  }

  async init() {
    this._db = await this._openDatabase();
  }
};

const EXPORTED_SYMBOLS = ["ActivityStreamStorage"];
