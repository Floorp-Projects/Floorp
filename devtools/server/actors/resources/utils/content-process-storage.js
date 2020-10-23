/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { storageTypePool } = require("devtools/server/actors/storage");

// ms of delay to throttle updates
const BATCH_DELAY = 200;

class ContentProcessStorage {
  constructor(storageKey, storageType) {
    this.storageKey = storageKey;
    this.storageType = storageType;
  }

  async watch(targetActor, { onAvailable, onUpdated, onDestroyed }) {
    const ActorConstructor = storageTypePool.get(this.storageKey);
    this.actor = new ActorConstructor({
      get conn() {
        return targetActor.conn;
      },
      get windows() {
        return targetActor.windows;
      },
      get window() {
        return targetActor.window;
      },
      get document() {
        return this.window.document;
      },
      get originAttributes() {
        return this.document.effectiveStoragePrincipal.originAttributes;
      },

      update(action, storeType, data) {
        if (!this.boundUpdate) {
          this.boundUpdate = {};
        }

        if (action === "cleared") {
          const response = {};
          response[this.storageKey] = data;

          onDestroyed([
            {
              // needs this so the resource gets passed as an actor
              // ...storages[storageKey],
              ...storage,
              clearedHostsOrPaths: data,
            },
          ]);
        }

        if (this.batchTimer) {
          clearTimeout(this.batchTimer);
        }

        if (!this.boundUpdate[action]) {
          this.boundUpdate[action] = {};
        }
        if (!this.boundUpdate[action][storeType]) {
          this.boundUpdate[action][storeType] = {};
        }
        for (const host in data) {
          if (!this.boundUpdate[action][storeType][host]) {
            this.boundUpdate[action][storeType][host] = [];
          }
          for (const name of data[host]) {
            if (!this.boundUpdate[action][storeType][host].includes(name)) {
              this.boundUpdate[action][storeType][host].push(name);
            }
          }
        }

        if (action === "added") {
          // If the same store name was previously deleted or changed, but now
          // is added somehow, don't send the deleted or changed update
          this._removeNamesFromUpdateList("deleted", storeType, data);
          this._removeNamesFromUpdateList("changed", storeType, data);
        } else if (
          action === "changed" &&
          this.boundUpdate?.added?.[storeType]
        ) {
          // If something got added and changed at the same time, then remove
          // those items from changed instead.
          this._removeNamesFromUpdateList(
            "changed",
            storeType,
            this.boundUpdate.added[storeType]
          );
        } else if (action === "deleted") {
          // If any item got deleted, or a host got deleted, there's no point
          // in sending added or changed upate, so we remove them.
          this._removeNamesFromUpdateList("added", storeType, data);
          this._removeNamesFromUpdateList("changed", storeType, data);

          for (const host in data) {
            if (
              data[host].length === 0 &&
              this.boundUpdate?.added?.[storeType]?.[host]
            ) {
              delete this.boundUpdate.added[storeType][host];
            }

            if (
              data[host].length === 0 &&
              this.boundUpdate?.changed?.[storeType]?.[host]
            ) {
              delete this.boundUpdate.changed[storeType][host];
            }
          }
        }

        this.batchTimer = setTimeout(() => {
          clearTimeout(this.batchTimer);
          onUpdated([
            {
              // needs this so the resource gets passed as an actor
              // ...storages[storageKey],
              ...storage,
              added: this.boundUpdate.added,
              changed: this.boundUpdate.changed,
              deleted: this.boundUpdate.deleted,
            },
          ]);
          this.boundUpdate = {};
        }, BATCH_DELAY);

        return null;
      },
      /**
       * This method removes data from the this.boundUpdate object in the same
       * manner like this.update() adds data to it.
       *
       * @param {string} action
       *        The type of change. One of "added", "changed" or "deleted"
       * @param {string} storeType
       *        The storage actor for which you want to remove the updates data.
       * @param {object} data
       *        The update object. This object is of the following format:
       *         - {
       *             <host1>: [<store_names1>, <store_name2>...],
       *             <host2>: [<store_names34>...],
       *           }
       *           Where host1, host2 are the hosts which you want to remove and
       *           [<store_namesX] is an array of the names of the store objects.
       **/
      _removeNamesFromUpdateList(action, storeType, data) {
        for (const host in data) {
          if (this.boundUpdate?.[action]?.[storeType]?.[host]) {
            for (const name in data[host]) {
              const index = this.boundUpdate[action][storeType][host].indexOf(
                name
              );
              if (index > -1) {
                this.boundUpdate[action][storeType][host].splice(index, 1);
              }
            }
            if (!this.boundUpdate[action][storeType][host].length) {
              delete this.boundUpdate[action][storeType][host];
            }
          }
        }
        return null;
      },

      on() {
        targetActor.on.apply(this, arguments);
      },
      off() {
        targetActor.off.apply(this, arguments);
      },
      once() {
        targetActor.once.apply(this, arguments);
      },
    });

    // We have to manage the actor manually, because ResourceWatcher doesn't
    // use the protocol.js specification.
    // resource-available-form is typed as "json"
    // So that we have to manually handle stuff that would normally be
    // automagically done by procotol.js
    // 1) Manage the actor in order to have an actorID on it
    targetActor.manage(this.actor);
    // 2) Convert to JSON "form"
    const form = this.actor.form();

    // NOTE: this is hoisted, so the `update` method above may use it.
    const storage = form;

    // All resources should have a resourceType, resourceId and resourceKey
    // attributes, so available/updated/destroyed callbacks work properly.
    storage.resourceType = this.storageType;
    storage.resourceId = this.storageType;
    storage.resourceKey = this.storageKey;

    onAvailable([storage]);
  }

  destroy() {
    this.actor?.destroy();
    this.actor = null;
  }
}

module.exports = ContentProcessStorage;
