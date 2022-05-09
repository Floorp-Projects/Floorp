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
    for (const key of Object.keys(defaults)) {
      if (!key.endsWith(':locked'))
        continue;
      if (defaults[key])
        this.$defaultLockedKeys.push(key.replace(/:locked$/, ''));
      delete defaults[key];
    }
    this.$default = defaults;
    this.$logging = logging || false;
    this.$logs = [];
    this.$logger = logger;
    this.sync = sync === undefined ? true : !!sync;
    this._locked = new Set();
    this._lastValues = {};
    this._updating = new Map();
    this._observers = new Set();
    this._changedObservers = new Set();
    this._localLoadedObservers = new Set();
    this._syncKeys = localKeys ?
      Object.keys(defaults).filter(x => !localKeys.includes(x)) :
      (syncKeys || []);
    this.$loaded = this._load();
  }

  async $reset() {
    this._applyValues(this.$default);
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
    this._applyValues(this.$default);
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
          let resolved = false;
          return new Promise((resolve, _reject) => {
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
              resolve(null);
            });

            // storage.managed.get() fails on options page in Thunderbird.
            // The problem should be fixed by Thunderbird side.
            if (window.messenger) {
              setTimeout(() => {
                if (resolved)
                  return;
                resolved = true;
                this._log('load: failed to load managed storage: timeout');
                resolve(null);
              }, 250);
            }
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
        lockedValues[key] = this.$default[key];
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
        set: (value) => this._setValue(key, value)
      });
    }
  }

  _setValue(key, value) {
    if (this._locked.has(key)) {
      this._log(`warning: ${key} is locked and not updated`);
      return value;
    }
    if (JSON.stringify(value) == JSON.stringify(this._lastValues[key]))
      return value;
    this._log(`set: ${key} = ${value}`);
    this._lastValues[key] = value;

    const update = {};
    update[key] = value;
    try {
      this._updating.set(key, value);
      browser.storage.local.set(update).then(() => {
        this._log('successfully saved', update);
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
      if (this.sync && this._syncKeys.includes(key))
        browser.storage.sync.set(update).then(() => {
          this._log('successfully synced', update);
        });
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
      this._lastValues[key] = change.newValue;
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
};
export default Configs;
