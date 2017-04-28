/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://shield-recipe-client/lib/LogManager.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "JSONFile", "resource://gre/modules/JSONFile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task", "resource://gre/modules/Task.jsm");

this.EXPORTED_SYMBOLS = ["Storage"];

const log = LogManager.getLogger("storage");
let storePromise;

function loadStorage() {
  if (storePromise === undefined) {
    const path = OS.Path.join(OS.Constants.Path.profileDir, "shield-recipe-client.json");
    const storage = new JSONFile({path});
    storePromise = Task.spawn(function* () {
      yield storage.load();
      return storage;
    });
  }
  return storePromise;
}

this.Storage = {
  makeStorage(prefix, sandbox) {
    if (!sandbox) {
      throw new Error("No sandbox passed");
    }

    const storageInterface = {
      /**
       * Sets an item in the prefixed storage.
       * @returns {Promise}
       * @resolves With the stored value, or null.
       * @rejects Javascript exception.
       */
      getItem(keySuffix) {
        return new sandbox.Promise((resolve, reject) => {
          loadStorage()
            .then(store => {
              const namespace = store.data[prefix] || {};
              const value = namespace[keySuffix] || null;
              resolve(Cu.cloneInto(value, sandbox));
            })
            .catch(err => {
              log.error(err);
              reject(new sandbox.Error());
            });
        });
      },

      /**
       * Sets an item in the prefixed storage.
       * @returns {Promise}
       * @resolves When the operation is completed succesfully
       * @rejects Javascript exception.
       */
      setItem(keySuffix, value) {
        return new sandbox.Promise((resolve, reject) => {
          loadStorage()
            .then(store => {
              if (!(prefix in store.data)) {
                store.data[prefix] = {};
              }
              store.data[prefix][keySuffix] = value;
              store.saveSoon();
              resolve();
            })
            .catch(err => {
              log.error(err);
              reject(new sandbox.Error());
            });
        });
      },

      /**
       * Removes a single item from the prefixed storage.
       * @returns {Promise}
       * @resolves When the operation is completed succesfully
       * @rejects Javascript exception.
       */
      removeItem(keySuffix) {
        return new sandbox.Promise((resolve, reject) => {
          loadStorage()
            .then(store => {
              if (!(prefix in store.data)) {
                return;
              }
              delete store.data[prefix][keySuffix];
              store.saveSoon();
              resolve();
            })
            .catch(err => {
              log.error(err);
              reject(new sandbox.Error());
            });
        });
      },

      /**
       * Clears all storage for the prefix.
       * @returns {Promise}
       * @resolves When the operation is completed succesfully
       * @rejects Javascript exception.
       */
      clear() {
        return new sandbox.Promise((resolve, reject) => {
          return loadStorage()
            .then(store => {
              store.data[prefix] = {};
              store.saveSoon();
              resolve();
            })
            .catch(err => {
              log.error(err);
              reject(new sandbox.Error());
            });
        });
      },
    };

    return Cu.cloneInto(storageInterface, sandbox, {
      cloneFunctions: true,
    });
  },

  /**
   * Clear ALL storage data and save to the disk.
   */
  clearAllStorage() {
    return loadStorage()
      .then(store => {
        store.data = {};
        store.saveSoon();
      })
      .catch(err => {
        log.error(err);
      });
  },
};
