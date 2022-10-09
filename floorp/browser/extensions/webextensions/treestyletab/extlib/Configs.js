/*
 license: The MIT License, Copyright (c) 2016-2018 YUKI "Piro" Hiroshi
 original:
   http://github.com/piroor/webextensions-lib-configs
*/

'use strict';

// eslint-disable-next-line no-unused-vars
class Configs {
  constructor(
    defaults,
    { logging, logger, localKeys, syncKeys, sync } = { syncKeys: [], logger: null }
  ) {
    this.$defaultLockedKeys = [];

    this._defaultValues = {
      ...this._clone(defaults),
      __ConfigsMigration__userValeusSameToDefaultAreCleared: false,
    };
    this.$default = {};
    for (const key of Object.keys(this._defaultValues)) {
      Object.defineProperty(this.$default, key, {
        get: () => this._defaultValues[key],
        set: (value) => this._setDefaultValue(key, value),
        enumerable: true,
      });
    }

    for (const key of Object.keys(defaults)) {
      if (!key.endsWith(':locked'))
        continue;
      if (defaults[key])
        this.$defaultLockedKeys.push(key.replace(/:locked$/, ''));
      delete defaults[key];
    }
    this.$logging = logging || false;
    this.$logs = [];
    this.$logger = logger;
    this.sync = sync === undefined ? true : !!sync;
    this._locked = new Set();
    this._lastValues = {};
    this._fetchedValues = {};
    this._updating = new Map();
    this._observers = new Set();
    this._changedObservers = new Set();
    this._localLoadedObservers = new Set();
    this._syncKeys = [
      ...(localKeys ?
        Object.keys(defaults).filter(x => !localKeys.includes(x)) :
        (syncKeys || [])),
      '__ConfigsMigration__userValeusSameToDefaultAreCleared',
    ];
    this.$loaded = this._load();
  }

  $reset(key) {
    if (!key) {
      for (const key of Object.keys(this._defaultValues)) {
        this.$reset(key);
      }
      return;
    }

    if (!(key in this._defaultValues))
      throw new Error(`failed to reset unknown key: ${key}`);

    this._setValue(key, this._defaultValues[key], true);
  }

  $cleanUp() {
    for (const [key, defaultValue] of Object.entries(this._defaultValues)) {
      if (!(key in this._fetchedValues))
        continue;
      if (JSON.stringify(this._fetchedValues[key]) == JSON.stringify(defaultValue))
        this.$reset(key);
    }
  }

  _setDefaultValue(key, value) {
    if (!key)
      throw new Error(`missing key for default value ${value}`);

    if (!(key in this._defaultValues))
      throw new Error(`failed to set default value for unknown key: ${key}`);

    this._defaultValues[key] = this._clone(value);
    if (!(key in this._fetchedValues) ||
        JSON.stringify(this._defaultValues[key]) == JSON.stringify(this._fetchedValues[key]))
      this.$reset(key);
  }

  $addLocalLoadedObserver(observer) {
    if (!this._localLoadedObservers.has(observer))
      this._localLoadedObservers.add(observer);
  }
  $removeLocalLoadedObserver(observer) {
    this._localLoadedObservers.delete(observer);
  }

  $addChangedObserver(observer) {
    if (!this._changedObservers.has(observer))
      this._changedObservers.add(observer);
  }
  $removeChangedObserver(observer) {
    this._changedObservers.delete(observer);
  }

  $addObserver(observer) {
    // for backward compatibility
    if (typeof observer == 'function')
      this.$addChangedObserver(observer);
    else if (!this._observers.has(observer))
      this._observers.add(observer);
  }
  $removeObserver(observer) {
    // for backward compatibility
    if (typeof observer == 'function')
      this.$removeChangedObserver(observer);
    else
      this._observers.delete(observer);
  }

  _log(message, ...args) {
    message = `Configs[${location.href}] ${message}`;
    this.$logs = this.$logs.slice(-1000);

    if (!this.$logging)
      return;

    if (typeof this.$logger === 'function')
      this.$logger(message, ...args);
    else
      console.log(message, ...args);
  }

  _load() {
    return this.$_promisedLoad ||
             (this.$_promisedLoad = this._tryLoad());
  }

  async _tryLoad() {
    this._log('load');
    this._applyValues(this._defaultValues);
    let values;
    try {
      this._log(`load: try load from storage on ${location.href}`);
      const [localValues, managedValues, lockedKeys] = await Promise.all([
        (async () => {
          try {
            const localValues = await browser.storage.local.get(null); // keys must be "null" to get only stored values
            this._log('load: successfully loaded local storage');
            const observers = [...this._observers, ...this._localLoadedObservers];
            for (const [key, value] of Object.entries(localValues)) {
              this.$notifyToObservers(key, value, observers, 'onLocalLoaded');
            }
            return localValues;
          }
          catch(e) {
            this._log('load: failed to load local storage: ', String(e));
          }
          return {};
        })(),
        (async () => {
          if (!browser.storage.managed) {
            this._log('load: skip managed storage');
            return null;
          }
          return new Promise(async (resolve, _reject) => {
            const loadManagedStorage = () => {
              let resolved = false;
              return new Promise((resolve, reject) => {
                browser.storage.managed.get().then(managedValues => {
                  if (resolved)
                    return;
                  resolved = true;
                  this._log('load: successfully loaded managed storage');
                  resolve(managedValues || null);
                }).catch(error => {
                  if (resolved)
                    return;
                  resolved = true;
                  this._log('load: failed to load managed storage: ', String(error));
                  reject(error);
                });

                // storage.managed.get() fails on options page in Thunderbird.
                // The problem should be fixed by Thunderbird side.
                setTimeout(() => {
                  if (resolved)
                    return;
                  resolved = true;
                  this._log('load: failed to load managed storage: timeout');
                  reject(new Error('timeout'));
                }, 250);
              });
            };

            for (let i = 0, maxi = 10; i < maxi; i++) {
              try {
                const result = await loadManagedStorage();
                resolve(result);
                return;
              }
              catch(error) {
                if (error.message != 'timeout') {
                  console.log('managed storage is not provided');
                  resolve(null);
                  return;
                }
                console.log('failed to load managed storage ', error);
              }
              await new Promise(resolve => setTimeout(resolve, 250));
            }
            console.log('failed to load managed storage with 10 times retly');
            resolve(null);
          });
        })(),
        (async () => {
          try {
            const lockedKeys = await browser.runtime.sendMessage({
              type: 'Configs:getLockedKeys'
            });
            this._log('load: successfully synchronized locked state');
            return lockedKeys || [];
          }
          catch(e) {
            this._log('load: failed to synchronize locked state: ', String(e));
          }
          return [];
        })()
      ]);
      this._log(`load: loaded:`, { localValues, managedValues, lockedKeys });

      lockedKeys.push(...this.$defaultLockedKeys);

      const lockedValues = {};
      for (const key of this.$defaultLockedKeys) {
        lockedValues[key] = this._defaultValues[key];
      }

      if (managedValues) {
        const unlockedKeys = new Set();
        for (const key of Object.keys(managedValues)) {
          if (!key.endsWith(':locked'))
            continue;
          if (!managedValues[key])
            unlockedKeys.add(key.replace(/:locked$/, ''));
          delete managedValues[key];
        }
        const lockedManagedKeys = Object.keys(managedValues).filter(key => !unlockedKeys.has(key));
        lockedKeys.push(...lockedManagedKeys);
        for (const key of lockedManagedKeys) {
          lockedValues[key] = managedValues[key];
        }
      }

      values = { ...(managedValues || {}), ...(localValues || {}), ...lockedValues };
      this._fetchedValues = this._clone(values);
      this._applyValues(values);
      this._log('load: values are applied');

      for (const key of new Set(lockedKeys)) {
        this._updateLocked(key, true);
      }
      this._log('load: locked state is applied');
      browser.storage.onChanged.addListener(this._onChanged.bind(this));
      if (this.sync &&
          (this._syncKeys ||
           this._syncKeys.length > 0)) {
        try {
          browser.storage.sync.get(this._syncKeys).then(syncedValues => {
            this._log('load: successfully loaded sync storage');
            if (!syncedValues)
              return;
            for (const key of Object.keys(syncedValues)) {
              this[key] = syncedValues[key];
            }
          });
        }
        catch(e) {
          this._log('load: failed to read sync storage: ', String(e));
          return null;
        }
      }
      browser.runtime.onMessage.addListener(this._onMessage.bind(this));

      if (!this.__ConfigsMigration__userValeusSameToDefaultAreCleared) {
        this.$cleanUp();
        this.__ConfigsMigration__userValeusSameToDefaultAreCleared = true;
      }

      return values;
    }
    catch(e) {
      this._log('load: fatal error: ', e, e.stack);
      throw e;
    }
  }
  _applyValues(values) {
    for (const [key, value] of Object.entries(values)) {
      if (this._locked.has(key))
        continue;
      this._lastValues[key] = value;
      if (key in this)
        continue;
      Object.defineProperty(this, key, {
        get: () => this._lastValues[key],
        set: (value) => this._setValue(key, value),
        enumerable: true,
      });
    }
  }

  _setValue(key, value, force = false) {
    if (this._locked.has(key)) {
      this._log(`warning: ${key} is locked and not updated`);
      return value;
    }

    const stringified = JSON.stringify(value);
    if (stringified == JSON.stringify(this._lastValues[key]) && !force)
      return value;

    const shouldReset = stringified == JSON.stringify(this._defaultValues[key]);
    this._log(`set: ${key} = ${value}${shouldReset ? ' (reset to default)' : ''}`);
    this._lastValues[key] = value;

    const update = {};
    update[key] = value;
    try {
      this._updating.set(key, value);
      const updated = shouldReset ?
        browser.storage.local.remove([key]).then(() => {
          delete this._fetchedValues[key];
          this._log('local: successfully removed ', key);
        }) :
        browser.storage.local.set(update).then(() => {
          this._fetchedValues[key] = this._clone(value);
          this._log('local: successfully saved ', update);
        });
      updated
        .then(() => {
          setTimeout(() => {
            if (!this._updating.has(key))
              return;
            // failsafe: on Thunderbird updates sometimes won't be notified to the page itself.
            const changes = {};
            changes[key] = {
              oldValue: this[key],
              newValue: value
            };
            this._onChanged(changes);
          }, 250);
        });
    }
    catch(e) {
      this._log('save: failed', e);
    }
    try {
      if (this.sync && this._syncKeys.includes(key)) {
        if (shouldReset)
          browser.storage.sync.remove([key]).then(() => {
            this._log('sync: successfully removed', update);
          });
        else
          browser.storage.sync.set(update).then(() => {
            this._log('sync: successfully synced', update);
          });
      }
    }
    catch(e) {
      this._log('sync: failed', e);
    }
    return value;
  }

  $lock(key) {
    this._log('locking: ' + key);
    this._updateLocked(key, true);
  }

  $unlock(key) {
    this._log('unlocking: ' + key);
    this._updateLocked(key, false);
  }

  $isLocked(key) {
    return this._locked.has(key);
  }

  _updateLocked(key, locked, { broadcast } = {}) {
    if (locked) {
      this._locked.add(key);
    }
    else {
      this._locked.delete(key);
    }
    if (browser.runtime &&
        broadcast !== false) {
      try {
        browser.runtime.sendMessage({
          type:   'Configs:updateLocked',
          key:    key,
          locked: this._locked.has(key)
        }).catch(_error => {});
      }
      catch(_error) {
      }
    }
  }

  _onMessage(message, sender) {
    if (!message ||
        typeof message.type != 'string')
      return;

    this._log(`onMessage: ${message.type}`, message, sender);
    switch (message.type) {
      case 'Configs:getLockedKeys':
        return Promise.resolve(Array.from(this._locked.values()));

      case 'Configs:updateLocked':
        this._updateLocked(message.key, message.locked, { broadcast: false });
        break;
    }
  }

  _onChanged(changes) {
    this._log('_onChanged', changes);
    const observers = [...this._observers, ...this._changedObservers];
    for (const [key, change] of Object.entries(changes)) {
      if ('newValue' in change) {
        this._fetchedValues[key] = this._clone(change.newValue);
        this._lastValues[key] = change.newValue;
      }
      else {
        delete this._fetchedValues[key];
        this._lastValues[key] = this._clone(this._defaultValues[key]);
      }
      this._updating.delete(key);
      this.$notifyToObservers(key, change.newValue, observers, 'onChangeConfig');
    }
  }

  $notifyToObservers(key, value, observers, observerMethod) {
    for (const observer of observers) {
      if (typeof observer === 'function')
        observer(key, value);
      else if (observer && typeof observer[observerMethod] === 'function')
        observer[observerMethod](key, value);
    }
  }

  _clone(value) {
    return JSON.parse(JSON.stringify(value));
  }
};
export default Configs;
