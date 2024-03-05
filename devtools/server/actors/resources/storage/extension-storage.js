/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  BaseStorageActor,
} = require("resource://devtools/server/actors/resources/storage/index.js");
const {
  parseItemValue,
} = require("resource://devtools/shared/storage/utils.js");
const {
  LongStringActor,
} = require("resource://devtools/server/actors/string.js");
// Use global: "shared" for these extension modules, because these
// are singletons with shared state, and we must not create a new instance if a
// dedicated loader was used to load this module.
loader.lazyGetter(this, "ExtensionParent", () => {
  return ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
    { global: "shared" }
  ).ExtensionParent;
});
loader.lazyGetter(this, "ExtensionProcessScript", () => {
  return ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionProcessScript.sys.mjs",
    { global: "shared" }
  ).ExtensionProcessScript;
});
loader.lazyGetter(this, "ExtensionStorageIDB", () => {
  return ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionStorageIDB.sys.mjs",
    { global: "shared" }
  ).ExtensionStorageIDB;
});

/**
 * The Extension Storage actor.
 */
class ExtensionStorageActor extends BaseStorageActor {
  constructor(storageActor) {
    super(storageActor, "extensionStorage");

    this.addonId = this.storageActor.parentActor.addonId;

    // Retrieve the base moz-extension url for the extension
    // (and also remove the final '/' from it).
    this.extensionHostURL = this.getExtensionPolicy().getURL().slice(0, -1);

    // Map<host, ExtensionStorageIDB db connection>
    // Bug 1542038, 1542039: Each storage area will need its own
    // dbConnectionForHost, as they each have different storage backends.
    // Anywhere dbConnectionForHost is used, we need to know the storage
    // area to access the correct database.
    this.dbConnectionForHost = new Map();

    this.onExtensionStartup = this.onExtensionStartup.bind(this);

    this.onStorageChange = this.onStorageChange.bind(this);
  }

  getExtensionPolicy() {
    return WebExtensionPolicy.getByID(this.addonId);
  }

  destroy() {
    ExtensionStorageIDB.removeOnChangedListener(
      this.addonId,
      this.onStorageChange
    );
    ExtensionParent.apiManager.off("startup", this.onExtensionStartup);

    super.destroy();
  }

  /**
   * We need to override this method as we ignore BaseStorageActor's hosts
   * and only care about the extension host.
   */
  async populateStoresForHosts() {
    // Ensure the actor's target is an extension and it is enabled
    if (!this.addonId || !this.getExtensionPolicy()) {
      return;
    }

    // Subscribe a listener for event notifications from the WE storage API when
    // storage local data has been changed by the extension, and keep track of the
    // listener to remove it when the debugger is being disconnected.
    ExtensionStorageIDB.addOnChangedListener(
      this.addonId,
      this.onStorageChange
    );

    try {
      // Make sure the extension storage APIs have been loaded,
      // otherwise the DevTools storage panel would not be updated
      // automatically when the extension storage data is being changed
      // if the parent ext-storage.js module wasn't already loaded
      // (See Bug 1802929).
      const { extension } = WebExtensionPolicy.getByID(this.addonId);
      await extension.apiManager.asyncGetAPI("storage", extension);
      // Also watch for addon reload in order to also do that
      // on next addon startup, otherwise we may also miss updates
      ExtensionParent.apiManager.on("startup", this.onExtensionStartup);
    } catch (e) {
      console.error(
        "Exception while trying to initialize webext storage API",
        e
      );
    }

    await this.populateStoresForHost(this.extensionHostURL);
  }

  /**
   * AddonManager listener used to force instantiating storage API
   * implementation in the parent process so that it forward content process
   * messages to ExtensionStorageIDB.
   *
   * Without this, we may miss storage updated after the addon reload.
   */
  async onExtensionStartup(_evtName, extension) {
    if (extension.id != this.addonId) {
      return;
    }
    await extension.apiManager.asyncGetAPI("storage", extension);
  }

  /**
   * This method asynchronously reads the storage data for the target extension
   * and caches this data into this.hostVsStores.
   * @param {String} host - the hostname for the extension
   */
  async populateStoresForHost(host) {
    if (host !== this.extensionHostURL) {
      return;
    }

    const extension = ExtensionProcessScript.getExtensionChild(this.addonId);
    if (!extension || !extension.hasPermission("storage")) {
      return;
    }

    // Make sure storeMap is defined and set in this.hostVsStores before subscribing
    // a storage onChanged listener in the parent process
    const storeMap = new Map();
    this.hostVsStores.set(host, storeMap);

    const storagePrincipal = await this.getStoragePrincipal();

    if (!storagePrincipal) {
      // This could happen if the extension fails to be migrated to the
      // IndexedDB backend
      return;
    }

    const db = await ExtensionStorageIDB.open(storagePrincipal);
    this.dbConnectionForHost.set(host, db);
    const data = await db.get();

    for (const [key, value] of Object.entries(data)) {
      storeMap.set(key, value);
    }

    if (this.storageActor.parentActor.fallbackWindow) {
      // Show the storage actor in the add-on storage inspector even when there
      // is no extension page currently open
      // This strategy may need to change depending on the outcome of Bug 1597900
      const storageData = {};
      storageData[host] = this.getNamesForHost(host);
      this.storageActor.update("added", this.typeName, storageData);
    }
  }
  /**
   * This fires when the extension changes storage data while the storage
   * inspector is open. Ensures this.hostVsStores stays up-to-date and
   * passes the changes on to update the client.
   */
  onStorageChange(changes) {
    const host = this.extensionHostURL;
    const storeMap = this.hostVsStores.get(host);

    function isStructuredCloneHolder(value) {
      return (
        value &&
        typeof value === "object" &&
        Cu.getClassName(value, true) === "StructuredCloneHolder"
      );
    }

    for (const key in changes) {
      const storageChange = changes[key];
      let { newValue, oldValue } = storageChange;
      if (isStructuredCloneHolder(newValue)) {
        newValue = newValue.deserialize(this);
      }
      if (isStructuredCloneHolder(oldValue)) {
        oldValue = oldValue.deserialize(this);
      }

      let action;
      if (typeof newValue === "undefined") {
        action = "deleted";
        storeMap.delete(key);
      } else if (typeof oldValue === "undefined") {
        action = "added";
        storeMap.set(key, newValue);
      } else {
        action = "changed";
        storeMap.set(key, newValue);
      }

      this.storageActor.update(action, this.typeName, { [host]: [key] });
    }
  }

  async getStoragePrincipal() {
    const { extension } = this.getExtensionPolicy();
    const { backendEnabled, storagePrincipal } =
      await ExtensionStorageIDB.selectBackend({ extension });

    if (!backendEnabled) {
      // IDB backend disabled; give up.
      return null;
    }

    // Received as a StructuredCloneHolder, so we need to deserialize
    return storagePrincipal.deserialize(this, true);
  }

  getValuesForHost(host, name) {
    const result = [];

    if (!this.hostVsStores.has(host)) {
      return result;
    }

    if (name) {
      return [{ name, value: this.hostVsStores.get(host).get(name) }];
    }

    for (const [key, value] of Array.from(
      this.hostVsStores.get(host).entries()
    )) {
      result.push({ name: key, value });
    }
    return result;
  }

  /**
   * Converts a storage item to an "extensionobject" as defined in
   * devtools/shared/specs/storage.js. Behavior largely mirrors the "indexedDB" storage actor,
   * except where it would throw an unhandled error (i.e. for a `BigInt` or `undefined`
   * `item.value`).
   * @param {Object} item - The storage item to convert
   * @param {String} item.name - The storage item key
   * @param {*} item.value - The storage item value
   * @return {extensionobject}
   */
  toStoreObject(item) {
    if (!item) {
      return null;
    }

    let { name, value } = item;
    const isValueEditable = extensionStorageHelpers.isEditable(value);

    // `JSON.stringify()` throws for `BigInt`, adds extra quotes to strings and `Date` strings,
    // and doesn't modify `undefined`.
    switch (typeof value) {
      case "bigint":
        value = `${value.toString()}n`;
        break;
      case "string":
        break;
      case "undefined":
        value = "undefined";
        break;
      default:
        value = JSON.stringify(value);
        if (
          // can't use `instanceof` across frame boundaries
          Object.prototype.toString.call(item.value) === "[object Date]"
        ) {
          value = JSON.parse(value);
        }
    }

    return {
      name,
      value: new LongStringActor(this.conn, value),
      area: "local", // Bug 1542038, 1542039: set the correct storage area
      isValueEditable,
    };
  }

  getFields() {
    return [
      { name: "name", editable: false },
      { name: "value", editable: true },
      { name: "area", editable: false },
      { name: "isValueEditable", editable: false, private: true },
    ];
  }

  onItemUpdated(action, host, names) {
    this.storageActor.update(action, this.typeName, {
      [host]: names,
    });
  }

  async editItem({ host, items }) {
    const db = this.dbConnectionForHost.get(host);
    if (!db) {
      return;
    }

    const { name, value } = items;

    let parsedValue = parseItemValue(value);
    if (parsedValue === value) {
      const { typesFromString } = extensionStorageHelpers;
      for (const { test, parse } of Object.values(typesFromString)) {
        if (test(value)) {
          parsedValue = parse(value);
          break;
        }
      }
    }
    const changes = await db.set({ [name]: parsedValue });
    this.fireOnChangedExtensionEvent(host, changes);

    this.onItemUpdated("changed", host, [name]);
  }

  async removeItem(host, name) {
    const db = this.dbConnectionForHost.get(host);
    if (!db) {
      return;
    }

    const changes = await db.remove(name);
    this.fireOnChangedExtensionEvent(host, changes);

    this.onItemUpdated("deleted", host, [name]);
  }

  async removeAll(host) {
    const db = this.dbConnectionForHost.get(host);
    if (!db) {
      return;
    }

    const changes = await db.clear();
    this.fireOnChangedExtensionEvent(host, changes);

    this.onItemUpdated("cleared", host, []);
  }

  /**
   * Let the extension know that storage data has been changed by the user from
   * the storage inspector.
   */
  fireOnChangedExtensionEvent(host, changes) {
    // Bug 1542038, 1542039: Which message to send depends on the storage area
    const uuid = new URL(host).host;
    Services.cpmm.sendAsyncMessage(
      `Extension:StorageLocalOnChanged:${uuid}`,
      changes
    );
  }
}
exports.ExtensionStorageActor = ExtensionStorageActor;

const extensionStorageHelpers = {
  /**
   * Editing is supported only for serializable types. Examples of unserializable
   * types include Map, Set and ArrayBuffer.
   */
  isEditable(value) {
    // Bug 1542038: the managed storage area is never editable
    for (const { test } of Object.values(this.supportedTypes)) {
      if (test(value)) {
        return true;
      }
    }
    return false;
  },
  isPrimitive(value) {
    const primitiveValueTypes = ["string", "number", "boolean"];
    return primitiveValueTypes.includes(typeof value) || value === null;
  },
  isObjectLiteral(value) {
    return (
      value &&
      typeof value === "object" &&
      Cu.getClassName(value, true) === "Object"
    );
  },
  // Nested arrays or object literals are only editable 2 levels deep
  isArrayOrObjectLiteralEditable(obj) {
    const topLevelValuesArr = Array.isArray(obj) ? obj : Object.values(obj);
    if (
      topLevelValuesArr.some(
        value =>
          !this.isPrimitive(value) &&
          !Array.isArray(value) &&
          !this.isObjectLiteral(value)
      )
    ) {
      // At least one value is too complex to parse
      return false;
    }
    const arrayOrObjects = topLevelValuesArr.filter(
      value => Array.isArray(value) || this.isObjectLiteral(value)
    );
    if (arrayOrObjects.length === 0) {
      // All top level values are primitives
      return true;
    }

    // One or more top level values was an array or object literal.
    // All of these top level values must themselves have only primitive values
    // for the object to be editable
    for (const nestedObj of arrayOrObjects) {
      const secondLevelValuesArr = Array.isArray(nestedObj)
        ? nestedObj
        : Object.values(nestedObj);
      if (secondLevelValuesArr.some(value => !this.isPrimitive(value))) {
        return false;
      }
    }
    return true;
  },
  typesFromString: {
    // Helper methods to parse string values in editItem
    jsonifiable: {
      test(str) {
        try {
          JSON.parse(str);
        } catch (e) {
          return false;
        }
        return true;
      },
      parse(str) {
        return JSON.parse(str);
      },
    },
  },
  supportedTypes: {
    // Helper methods to determine the value type of an item in isEditable
    array: {
      test(value) {
        if (Array.isArray(value)) {
          return extensionStorageHelpers.isArrayOrObjectLiteralEditable(value);
        }
        return false;
      },
    },
    boolean: {
      test(value) {
        return typeof value === "boolean";
      },
    },
    null: {
      test(value) {
        return value === null;
      },
    },
    number: {
      test(value) {
        return typeof value === "number";
      },
    },
    object: {
      test(value) {
        if (extensionStorageHelpers.isObjectLiteral(value)) {
          return extensionStorageHelpers.isArrayOrObjectLiteralEditable(value);
        }
        return false;
      },
    },
    string: {
      test(value) {
        return typeof value === "string";
      },
    },
  },
};
