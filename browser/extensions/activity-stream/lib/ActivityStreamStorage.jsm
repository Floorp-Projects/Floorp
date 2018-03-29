ChromeUtils.defineModuleGetter(this, "IndexedDB", "resource://gre/modules/IndexedDB.jsm");

this.ActivityStreamStorage = class ActivityStreamStorage {
  /**
   * @param storeName String with the store name to access or array of strings
   *                  to create all the required stores
   */
  constructor(storeName) {
    this.dbName = "ActivityStream";
    this.dbVersion = 3;
    this.storeName = storeName;
  }

  get db() {
    return this._db || (this._db = this._openDatabase());
  }

  async getStore() {
    return (await this.db).objectStore(this.storeName, "readwrite");
  }

  async get(key) {
    return (await this.getStore()).get(key);
  }

  async getAll() {
    return (await this.getStore()).getAll();
  }

  async set(key, value) {
    return (await this.getStore()).put(value, key);
  }

  _openDatabase() {
    return IndexedDB.open(this.dbName, {version: this.dbVersion}, db => {
      // If provided with array of objectStore names we need to create all the
      // individual stores
      if (Array.isArray(this.storeName)) {
        this.storeName.forEach(store => {
          if (!db.objectStoreNames.contains(store)) {
            db.createObjectStore(store);
          }
        });
      } else if (!db.objectStoreNames.contains(this.storeName)) {
        db.createObjectStore(this.storeName);
      }
    });
  }
};

function getDefaultOptions(options) {
  return {collapsed: !!options.collapsed};
}

const EXPORTED_SYMBOLS = ["ActivityStreamStorage", "getDefaultOptions"];
