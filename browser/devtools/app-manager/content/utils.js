/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 *
 * Some helpers for common operations in the App Manager:
 *
 *  . mergeStores: merge several store into one.
 *  . l10n: resolves strings from app-manager.properties.
 *
 */

let Utils = (function() {
  const Cu = Components.utils;
  const {Services} = Cu.import("resource://gre/modules/Services.jsm", {});
  const {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
  const {require} = devtools;
  const EventEmitter = require("devtools/toolkit/event-emitter");


  function _createSetEventForwarder(key, finalStore) {
    return function(event, path, value) {
      finalStore.emit("set", [key].concat(path), value);
    };
  }

  function mergeStores(stores) {
    let finalStore = {object:{}};

    EventEmitter.decorate(finalStore);

    let setEventForwarders = {};

    for (let key in stores) {
      let store = stores[key];
      finalStore.object[key] = store.object;
      setEventForwarders[key] = _createSetEventForwarder(key, finalStore);
      store.on("set", setEventForwarders[key]);
    }

    finalStore.destroy = () => {
      for (let key in stores) {
        let store = stores[key];
        store.off("set", setEventForwarders[key]);
        if (store.destroy) {
          store.destroy();
        }
      }
    };

    return finalStore;
  }


  let strings = Services.strings.createBundle("chrome://browser/locale/devtools/app-manager.properties");

  function l10n (property, args = []) {
    if (args && args.length > 0) {
      return strings.formatStringFromName(property, args, args.length);
    } else {
      return strings.GetStringFromName(property);
    }
  }

  return {
    mergeStores: mergeStores,
    l10n: l10n
  }
})();
