/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const { childSpecs, storageSpec } = require("devtools/shared/specs/storage");

for (const childSpec of Object.values(childSpecs)) {
  class ChildStorageFront extends FrontClassWithSpec(childSpec) {
    constructor(client, targetFront, parentFront) {
      super(client, targetFront, parentFront);

      this.on("single-store-update", this._onStoreUpdate.bind(this));
    }

    form(form) {
      this.actorID = form.actor;
      this.hosts = form.hosts;
      this.traits = form.traits || {};
      return null;
    }

    // Update the storage fronts `hosts` properties with potential new hosts and remove the deleted ones
    async _onStoreUpdate({ changed, added, deleted }) {
      // `resourceKey` comes from the storage resource and is set by the legacy listener
      // -or- the resource transformer.
      const { resourceKey } = this;
      if (added) {
        for (const host in added[resourceKey]) {
          if (!this.hosts[host]) {
            this.hosts[host] = added[resourceKey][host];
          }
        }
      }
      if (deleted) {
        // While addition have to be added immediately, before ui.js receive single-store-update event
        // Deletions have to be removed after ui.js processed single-store-update.
        //
        // Unfortunately it makes some tests to fail, for ex: browser_storage_cookies_delete_all.js
        //
        //setTimeout(()=> {
        //  for (const host in deleted[resourceKey]) {
        //    delete this.hosts[host];
        //  }
        //}, 2000);
      }
    }
  }
  registerFront(ChildStorageFront);
}

class StorageFront extends FrontClassWithSpec(storageSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    // Attribute name from which to retrieve the actorID out of the target actor's form
    this.formAttributeName = "storageActor";
  }

  // listStores actor method doesn't support being called many times in a row,
  // so memoize its value to call it only once.
  // This function fetches all the storage actor's for each store type.
  // This is called many times by each legacy listener, but its returned content
  // is always the same as the actors are instantiated once for the whole target's lifecycle.
  listStores() {
    if (this.stores) {
      return this.stores;
    }
    this.stores = super.listStores();
    return this.stores;
  }
}

exports.StorageFront = StorageFront;
registerFront(StorageFront);
