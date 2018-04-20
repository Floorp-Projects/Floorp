ChromeUtils.defineModuleGetter(this, "IndexedDB", "resource://gre/modules/IndexedDB.jsm");

this.ActivityStreamStorage = class ActivityStreamStorage {
  /**
   * @param storeNames Array of strings used to create all the required stores
   */
  constructor(options = {}) {
    if (!options.storeNames || !options.telemetry) {
      throw new Error(`storeNames and telemetry are required, called only with ${Object.keys(options)}`);
    }

    this.dbName = "ActivityStream";
    this.dbVersion = 3;
    this.storeNames = options.storeNames;
    this.telemetry = options.telemetry;
  }

  get db() {
    return this._db || (this._db = this._openDatabase());
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
        set: this._set.bind(this, storeName)
      };
    }

    throw new Error(`Store name ${storeName} does not exist.`);
  }

  async _getStore(storeName) {
    return (await this.db).objectStore(storeName, "readwrite");
  }

  _get(storeName, key) {
    return this._requestWrapper(async () => (await this._getStore(storeName)).get(key));
  }

  _getAll(storeName) {
    return this._requestWrapper(async () => (await this._getStore(storeName)).getAll());
  }

  _set(storeName, key, value) {
    return this._requestWrapper(async () => (await this._getStore(storeName)).put(value, key));
  }

  _openDatabase() {
    return IndexedDB.open(this.dbName, {version: this.dbVersion}, db => {
      // If provided with array of objectStore names we need to create all the
      // individual stores
      this.storeNames.forEach(store => {
        if (!db.objectStoreNames.contains(store)) {
          this._requestWrapper(() => db.createObjectStore(store));
        }
      });
    });
  }

  async _requestWrapper(request) {
    let result = null;
    try {
      result = await request();
    } catch (e) {
      if (this.telemetry) {
        this.telemetry.handleUndesiredEvent({data: {event: "TRANSACTION_FAILED"}});
      }
      throw e;
    }

    return result;
  }
};

function getDefaultOptions(options) {
  return {collapsed: !!options.collapsed};
}

const EXPORTED_SYMBOLS = ["ActivityStreamStorage", "getDefaultOptions"];
