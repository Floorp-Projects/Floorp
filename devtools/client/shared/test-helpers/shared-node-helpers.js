/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global global */

/**
 * Adds mocks for browser-environment global variables/methods to Node global.
 */
function setMocksInGlobal() {
  global.Services = require("Services-mock");
  global.ChromeUtils = require("ChromeUtils-mock");

  global.isWorker = false;

  global.loader = {
    lazyGetter: (context, name, fn) => {
      const module = fn();
      global[name] = module;
    },
    lazyRequireGetter: (context, names, module, destructure) => {
      if (!Array.isArray(names)) {
        names = [names];
      }

      for (const name of names) {
        Object.defineProperty(global, name, {
          get() {
            const value = destructure
              ? require(module)[name]
              : require(module || name);
            return value;
          },
          configurable: true,
        });
      }
    },
    lazyImporter: () => {},
  };

  global.define = function() {};

  // Used for the HTMLTooltip component.
  // And set "isSystemPrincipal: false" because can't support XUL element in node.
  global.document.nodePrincipal = {
    isSystemPrincipal: false,
  };

  global.requestIdleCallback = function() {};

  global.requestAnimationFrame = function(cb) {
    cb();
    return null;
  };

  // Mock getSelection
  let selection;
  global.getSelection = function() {
    return {
      toString: () => selection,
      get type() {
        if (selection === undefined) {
          return "None";
        }
        if (selection === "") {
          return "Caret";
        }
        return "Range";
      },
      setMockSelection: str => {
        selection = str;
      },
    };
  };

  // Array#flatMap is only supported in Node 11+
  if (!Array.prototype.flatMap) {
    // eslint-disable-next-line no-extend-native
    Array.prototype.flatMap = function(cb) {
      return this.reduce((acc, x, i, arr) => {
        return acc.concat(cb(x, i, arr));
      }, []);
    };
  }

  global.indexedDB = function() {
    const store = {};
    return {
      open: () => ({}),
      getItem: async key => store[key],
      setItem: async (key, value) => {
        store[key] = value;
      },
    };
  };

  if (typeof global.TextEncoder === "undefined") {
    const { TextEncoder } = require("util");
    global.TextEncoder = TextEncoder;
  }

  if (typeof global.TextDecoder === "undefined") {
    const { TextDecoder } = require("util");
    global.TextDecoder = TextDecoder;
  }
}

module.exports = {
  setMocksInGlobal,
};
