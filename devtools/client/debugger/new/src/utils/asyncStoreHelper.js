"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.asyncStoreHelper = asyncStoreHelper;

var _asyncStorage = require("devtools/shared/async-storage");

var _asyncStorage2 = _interopRequireDefault(_asyncStorage);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/*
 * asyncStoreHelper wraps asyncStorage so that it is easy to define project
 * specific properties. It is similar to PrefsHelper.
 *
 * e.g.
 *   const asyncStore = asyncStoreHelper("r", {a: "_a"})
 *   asyncStore.a         // => asyncStorage.getItem("r._a")
 *   asyncStore.a = 2     // => asyncStorage.setItem("r._a", 2)
 */
function asyncStoreHelper(root, mappings) {
  let store = {};

  function getMappingKey(key) {
    return Array.isArray(mappings[key]) ? mappings[key][0] : mappings[key];
  }

  function getMappingDefaultValue(key) {
    return Array.isArray(mappings[key]) ? mappings[key][1] : null;
  }

  Object.keys(mappings).map(key => Object.defineProperty(store, key, {
    async get() {
      const value = await _asyncStorage2.default.getItem(`${root}.${getMappingKey(key)}`);
      return value || getMappingDefaultValue(key);
    },

    set(value) {
      return _asyncStorage2.default.setItem(`${root}.${getMappingKey(key)}`, value);
    }

  }));
  store = new Proxy(store, {
    set: function (target, property, value, receiver) {
      if (!mappings.hasOwnProperty(property)) {
        throw new Error(`AsyncStore: ${property} is not defined in mappings`);
      }

      Reflect.set(...arguments);
      return true;
    }
  });
  return store;
}