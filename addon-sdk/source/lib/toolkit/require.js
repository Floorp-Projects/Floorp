/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const make = (exports, rootURI, components) => {
  const { Loader: { Loader, Require, Module, main } } =
        components.utils.import(rootURI + "toolkit/loader.js", {});

  const loader = Loader({
    id: "toolkit/require",
    rootURI: rootURI,
    isNative: true,
    paths: {
     "": rootURI,
     "devtools/": "resource://devtools/"
    }
  });

  // Implement require.unload(uri) that can be used to unload
  // already loaded module which is convinient during development phase.
  const unload = uri => {
    delete loader.sandboxes[uri];
    delete loader.modules[uri];
  };

  const builtins = new Set(Object.keys(loader.modules));

  // Below we define `require` & `require.resolve` that resolve passed
  // module id relative to the caller URI. This is not perfect but good
  // enough for common case & there is always an option to pass absolute
  // id when that
  // but presumably well enough to cover

  const require = (id, options={}) => {
    const { reload, all } = options;
    const requirerURI = components.stack.caller.filename;
    const requirer = Module(requirerURI, requirerURI);
    const require = Require(loader, requirer);
    if (reload) {
      // To load JS code into modules, loader uses `mozIJSSubScriptLoader`
      // which uses startup cache to avoid reading source from the same URI
      // more than once. Unless we invalidate statup cache changes to a module
      // won't be reflected even after reload. Therefor we must dispatch an
      // nsIObserverService notification that causes cache invalidation.
      // Note: This is not ideal since it destroys whole cache, but since there
      // is no way to invalidate individual entries, we assume performance hit
      // during development is acceptable.
      components.classes["@mozilla.org/observer-service;1"].
        getService(components.interfaces.nsIObserverService).
        notifyObservers({}, "startupcache-invalidate", null);

      if (all) {
        for (let uri of Object.keys(loader.sandboxes)) {
          unload(uri);
        }
      }
      else {
        unload(require.resolve(id));
      }
    }
    return require(id);
  };

  require.resolve = id => {
    const requirerURI = components.stack.caller.filename;
    const requirer = Module(requirerURI, requirerURI);
    return Require(loader, requirer).resolve(id);
  };

  exports.require = require;
}

// If loaded in the context of commonjs module, reload as JSM into an
// exports object.
if (typeof(require) === "function" && typeof(module) === "object") {
  require("chrome").Cu.import(module.uri, module.exports);
}
// If loaded in the context of JSM make a loader & require and define
// new symbols as exported ones.
else if (typeof(__URI__) === "string" && this["Components"]) {
  const builtin = Object.keys(this);
  const uri = __URI__.replace("toolkit/require.js", "");
  make(this, uri, this["Components"]);

  this.EXPORTED_SYMBOLS = Object.
                            keys(this).
                            filter($ => builtin.indexOf($) < 0);
}
else {
  throw Error("Loading require.js in this environment isn't supported")
}
