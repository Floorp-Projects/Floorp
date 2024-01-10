/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global global */

/**
 * Adds mocks for browser-environment global variables/methods to Node global.
 */
function setMocksInGlobal() {
  global.Cc = new Proxy(
    {},
    {
      get(target, prop, receiver) {
        if (prop.startsWith("@mozilla.org")) {
          return { getService: () => ({}) };
        }
        return null;
      },
    }
  );
  global.Ci = {
    // sw states from
    // mozilla-central/source/dom/interfaces/base/nsIServiceWorkerManager.idl
    nsIServiceWorkerInfo: {
      STATE_PARSED: 0,
      STATE_INSTALLING: 1,
      STATE_INSTALLED: 2,
      STATE_ACTIVATING: 3,
      STATE_ACTIVATED: 4,
      STATE_REDUNDANT: 5,
      STATE_UNKNOWN: 6,
    },
  };
  global.Cu = {
    isInAutomation: true,
    now: () => {},
  };

  global.Services = require("Services-mock");
  global.ChromeUtils = require("ChromeUtils-mock");

  global.isWorker = false;

  global.loader = {
    lazyGetter: (context, name, fn) => {
      Object.defineProperty(global, name, {
        get() {
          delete global[name];
          global[name] = fn.apply(global);
          return global[name];
        },
        configurable: true,
        enumerable: true,
      });
    },
    lazyRequireGetter: (context, names, module, destructure) => {
      if (!Array.isArray(names)) {
        names = [names];
      }

      for (const name of names) {
        global.loader.lazyGetter(context, name, () => {
          return destructure ? require(module)[name] : require(module || name);
        });
      }
    },
    lazyServiceGetter: () => {},
  };

  global.define = function () {};

  // Used for the HTMLTooltip component.
  // And set "isSystemPrincipal: false" because can't support XUL element in node.
  global.document.nodePrincipal = {
    isSystemPrincipal: false,
  };

  global.requestIdleCallback = function () {};

  global.requestAnimationFrame = function (cb) {
    cb();
    return null;
  };

  // Mock getSelection
  let selection;
  global.getSelection = function () {
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
    Array.prototype.flatMap = function (cb) {
      return this.reduce((acc, x, i, arr) => {
        return acc.concat(cb(x, i, arr));
      }, []);
    };
  }

  if (typeof global.TextEncoder === "undefined") {
    const { TextEncoder } = require("util");
    global.TextEncoder = TextEncoder;
  }

  if (typeof global.TextDecoder === "undefined") {
    const { TextDecoder } = require("util");
    global.TextDecoder = TextDecoder;
  }

  if (!Promise.withResolvers) {
    Promise.withResolvers = function () {
      let resolve, reject;
      const promise = new Promise(function (res, rej) {
        resolve = res;
        reject = rej;
      });
      return { resolve, reject, promise };
    };
  }
}

module.exports = {
  setMocksInGlobal,
};
