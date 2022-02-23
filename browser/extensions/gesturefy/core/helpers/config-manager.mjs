import { isObject, cloneObject } from "/core/utils/commons.mjs";

/**
 * This class is a wrapper of the native storage API in order to allow synchronous config calls.
 * It also allows loading an optional default configuration which serves as a fallback if the property isn't stored in the user configuration.
 * The config manager should only be used after the config has been loaded.
 * This can be checked via the Promise returned by ConfigManagerInstance.loaded property.
 **/
export default class ConfigManager {

  /**
   * The constructor of the class requires a storage area as a string.
   * For the first parameter either "local" or "sync" is allowed.
   * An URL to a JSON formatted file can be passed optionally. The containing properties will be treated as the defaults.
   **/
  constructor (storageArea, defaultsURL) {
    if (storageArea !== "local" && storageArea !== "sync") {
      throw "The first argument must be a storage area in form of a string containing either local or sync.";
    }
    if (typeof defaultsURL !== "string" && defaultsURL !== undefined) {
      throw "The second argument must be an URL to a JSON file.";
    }

    this._storageArea = storageArea;
    // empty object as default value so the config doesn't have to be loaded
    this._storage = {};
    this._defaults = {};

    const fetchResources = [ browser.storage[this._storageArea].get() ];
    if (typeof defaultsURL === "string") {
      const defaultsObject = new Promise((resolve, reject) => {
        fetch(defaultsURL, {mode:'same-origin'})
          .then(res => res.json())
          .then(obj => resolve(obj), err => reject(err));
       });
      fetchResources.push( defaultsObject );
    }
    // load resources
    this._loaded = Promise.all(fetchResources);
    // store resources when loaded
    this._loaded.then((values) => {
      if (values[0]) this._storage = values[0];
      if (values[1]) this._defaults = values[1];
    });

    // holds all custom event callbacks
    this._events = {
      'change': new Set()
    };
    // defines if the storage should be automatically loaded und updated on storage changes
    this._autoUpdate = false;
    // setup on storage change handler
    browser.storage.onChanged.addListener((changes, areaName) => {
      if (areaName === this._storageArea) {
        // automatically update config if defined
        if (this._autoUpdate === true) {
          for (let property in changes) {
            this._storage[property] = changes[property].newValue;
          }
        }
        // execute event callbacks
        this._events["change"].forEach((callback) => callback(changes));
      }
    });
  }


  /**
   * Expose the "is loaded" Promise
   * This enables the programmer to check if the config has been loaded and run code on load
   * get, set, remove calls should generally called after the config has been loaded otherwise they'll have no effect or return undefined
   **/
  get loaded () {
    return this._loaded;
  }


  /**
   * Returns the value of the given storage path
   * A Storage path is constructed of one or more nested JSON keys concatenated with dots or an array of nested JSON keys
   * If the storage path is left the current storage object is returned
   * If the storage path does not exist in the config or the function is called before the config has been loaded it will return undefined
   **/
  get (storagePath = []) {
    if (typeof storagePath === "string") storagePath = storagePath.split('.');
    else if (!Array.isArray(storagePath)) {
      throw "The first argument must be a storage path either in the form of an array or a string concatenated with dots.";
    }

    const pathWalker = (obj, key) => isObject(obj) ? obj[key] : undefined;
    let entry = storagePath.reduce(pathWalker, this._storage);
    // try to get the default value
    if (entry === undefined) entry = storagePath.reduce(pathWalker, this._defaults);
    if (entry !== undefined) return cloneObject(entry);

    return undefined;
  }


  /**
   * Returns true if the the given storage path exists else false
   * If the storage path is left the current storage object will be used
   * If is called before the config has been loaded it will return false
   **/
   has (storagePath = []) {
    return typeof this.get(storagePath) !== "undefined";
  }


  /**
   * Sets the value of a given storage path and creates the JSON keys if not available
   * If only one value of type object is passed the object keys will be stored in the config and existing keys will be overridden
   * Returns the storage set promise which resolves when the storage has been written successfully
   **/
  set (storagePath, value) {
    if (typeof storagePath === "string") storagePath = storagePath.split('.');
    // if only one argument is given and it is an object use this as the new config and override the old one
    else if (arguments.length === 1 && isObject(arguments[0])) {
      this._storage = cloneObject(arguments[0]);
      return browser.storage[this._storageArea].set(this._storage);
    }
    else if (!Array.isArray(storagePath)) {
      throw "The first argument must be a storage path either in the form of an array or a string concatenated with dots.";
    }

    if (storagePath.length > 0) {
      let entry = this._storage;
      const lastIndex = storagePath.length - 1;

      for (let i = 0; i < lastIndex; i++) {
        const key = storagePath[i];
        if (!entry.hasOwnProperty(key) || !isObject(entry[key])) {
          entry[key] = {};
        }
        entry = entry[key];
      }
      entry[ storagePath[lastIndex] ] = cloneObject(value);
      // save to storage
      return browser.storage[this._storageArea].set(this._storage);
    }
  }


  /**
   * Removes the key and value of a given storage path
   * Default values will not be removed, so get() may still return a default value even if removed was called before
   * Returns the storage set promise which resolves when the storage has been written successfully
   **/
  remove (storagePath) {
    if (typeof storagePath === "string") storagePath = storagePath.split('.');
    else if (!Array.isArray(storagePath)) {
      throw "The first argument must be a storage path either in the form of an array or a string concatenated with dots.";
    }

    if (storagePath.length > 0) {
      let entry = this._storage;
      const lastIndex = storagePath.length - 1;

      for (let i = 0; i < lastIndex; i++) {
        const key = storagePath[i];
        if (entry.hasOwnProperty(key) && isObject(entry[key])) {
          entry = entry[key];
        }
        else return;
      }
      delete entry[ storagePath[lastIndex] ];
      // remove single config item
      if (storagePath.length === 1) {
        return browser.storage[this._storageArea].remove(storagePath[0]);
      }
      // overwrite entire config
      return browser.storage[this._storageArea].set(this._storage);
    }
  }


  /**
   * Clears the entire config
   * If a default config is specified this is equal to resetting the config
   * Returns the storage clear promise which resolves when the storage has been written successfully
   **/
  clear () {
    this._storage = {};
    return browser.storage[this._storageArea].clear();
  }


  /**
   * Adds an event listener
   * Requires an event specifier as a string and a callback method
   * Current events are:
   * "change" - Fires when the storage has been changed
   **/
  addEventListener (event, callback) {
    if (!this._events.hasOwnProperty(event)) {
      throw "The first argument is not a valid event.";
    }
    if (typeof callback !== "function") {
      throw "The second argument must be a function.";
    }
    this._events[event].add(callback);
  }


  /**
   * Checks if an event listener exists
   * Requires an event specifier as a string and a callback method
   **/
  hasEventListener (event, callback) {
    if (!this._events.hasOwnProperty(event)) {
      throw "The first argument is not a valid event.";
    }
    if (typeof callback !== "function") {
      throw "The second argument must be a function.";
    }
    this._events[event].has(callback);
  }


  /**
   * Removes an event listener
   * Requires an event specifier as a string and a callback method
   **/
  removeEventListener (event, callback) {
    if (!this._events.hasOwnProperty(event)) {
      throw "The first argument is not a valid event.";
    }
    if (typeof callback !== "function") {
      throw "The second argument must be a function.";
    }
    this._events[event].delete(callback);
  }


  /**
   * Setter for the autoUpdate value
   * If autoUpdate is set to true the cached config will automatically update itself on storage changes
   **/
  set autoUpdate (value) {
    this._autoUpdate = Boolean(value);
  }


  /**
   * Getter for the autoUpdate value
   * If autoUpdate is set to true the cached config will automatically update itself on storage changes
   **/
  get autoUpdate () {
    return this._autoUpdate;
  }
}