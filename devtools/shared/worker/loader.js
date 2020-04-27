/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global worker, DebuggerNotificationObserver */

// A CommonJS module loader that is designed to run inside a worker debugger.
// We can't simply use the SDK module loader, because it relies heavily on
// Components, which isn't available in workers.
//
// In principle, the standard instance of the worker loader should provide the
// same built-in modules as its devtools counterpart, so that both loaders are
// interchangable on the main thread, making them easier to test.
//
// On the worker thread, some of these modules, in particular those that rely on
// the use of Components, and for which the worker debugger doesn't provide an
// alternative API, will be replaced by vacuous objects. Consequently, they can
// still be required, but any attempts to use them will lead to an exception.
//
// Note: to see dump output when running inside the worker thread, you might
// need to enable the browser.dom.window.dump.enabled pref.

this.EXPORTED_SYMBOLS = ["WorkerDebuggerLoader", "worker"];

// Some notes on module ids and URLs:
//
// An id is either a relative id or an absolute id. An id is relative if and
// only if it starts with a dot. An absolute id is a normalized id if and only
// if it contains no redundant components.
//
// Every normalized id is a URL. A URL is either an absolute URL or a relative
// URL. A URL is absolute if and only if it starts with a scheme name followed
// by a colon and 2 or 3 slashes.

/**
 * Convert the given relative id to an absolute id.
 *
 * @param String id
 *        The relative id to be resolved.
 * @param String baseId
 *        The absolute base id to resolve the relative id against.
 *
 * @return String
 *         An absolute id
 */
function resolveId(id, baseId) {
  return baseId + "/../" + id;
}

/**
 * Convert the given absolute id to a normalized id.
 *
 * @param String id
 *        The absolute id to be normalized.
 *
 * @return String
 *         A normalized id.
 */
function normalizeId(id) {
  // An id consists of an optional root and a path. A root consists of either
  // a scheme name followed by 2 or 3 slashes, or a single slash. Slashes in the
  // root are not used as separators, so only normalize the path.
  const [, root, path] = id.match(/^(\w+:\/\/\/?|\/)?(.*)/);

  const stack = [];
  path.split("/").forEach(function(component) {
    switch (component) {
      case "":
      case ".":
        break;
      case "..":
        if (stack.length === 0) {
          if (root !== undefined) {
            throw new Error("Can't normalize absolute id '" + id + "'!");
          } else {
            stack.push("..");
          }
        } else if (stack[stack.length - 1] == "..") {
          stack.push("..");
        } else {
          stack.pop();
        }
        break;
      default:
        stack.push(component);
        break;
    }
  });

  return (root ? root : "") + stack.join("/");
}

/**
 * Create a module object with the given normalized id.
 *
 * @param String
 *        The normalized id of the module to be created.
 *
 * @return Object
 *         A module with the given id.
 */
function createModule(id) {
  return Object.create(null, {
    // CommonJS specifies the id property to be non-configurable and
    // non-writable.
    id: {
      configurable: false,
      enumerable: true,
      value: id,
      writable: false,
    },

    // CommonJS does not specify an exports property, so follow the NodeJS
    // convention, which is to make it non-configurable and writable.
    exports: {
      configurable: false,
      enumerable: true,
      value: Object.create(null),
      writable: true,
    },
  });
}

/**
 * Create a CommonJS loader with the following options:
 * - createSandbox:
 *     A function that will be used to create sandboxes. It should take the name
 *     and prototype of the sandbox to be created, and return the newly created
 *     sandbox as result. This option is required.
 * - globals:
 *     A map of names to built-in globals that will be exposed to every module.
 *     Defaults to the empty map.
 * - loadSubScript:
 *     A function that will be used to load scripts in sandboxes. It should take
 *     the URL from and the sandbox in which the script is to be loaded, and not
 *     return a result. This option is required.
 * - modules:
 *     A map from normalized ids to built-in modules that will be added to the
 *     module cache. Defaults to the empty map.
 * - paths:
 *     A map of paths to base URLs that will be used to resolve relative URLs to
 *     absolute URLS. Defaults to the empty map.
 * - resolve:
 *     A function that will be used to resolve relative ids to absolute ids. It
 *     should take the relative id of a module to be required and the absolute
 *     id of the requiring module as arguments, and return the absolute id of
 *     the module to be required as result. Defaults to resolveId above.
 */
function WorkerDebuggerLoader(options) {
  /**
   * Convert the given relative URL to an absolute URL, using the map of paths
   * given below.
   *
   * @param String url
   *        The relative URL to be resolved.
   *
   * @return String
   *         An absolute URL.
   */
  function resolveURL(url) {
    let found = false;
    for (const [path, baseURL] of paths) {
      if (url.startsWith(path)) {
        found = true;
        url = url.replace(path, baseURL);
        break;
      }
    }
    if (!found) {
      throw new Error("Can't resolve relative URL '" + url + "'!");
    }

    // If the url has no extension, use ".js" by default.
    return url.endsWith(".js") ? url : url + ".js";
  }

  /**
   * Load the given module with the given url.
   *
   * @param Object module
   *        The module object to be loaded.
   * @param String url
   *        The URL to load the module from.
   */
  function loadModule(module, url) {
    // CommonJS specifies 3 free variables: require, exports, and module. These
    // must be exposed to every module, so define these as properties on the
    // sandbox prototype. Additional built-in globals are exposed by making
    // the map of built-in globals the prototype of the sandbox prototype.
    const prototype = Object.create(globals);
    prototype.Components = {};
    prototype.require = createRequire(module);
    prototype.exports = module.exports;
    prototype.module = module;

    const sandbox = createSandbox(url, prototype);
    try {
      loadSubScript(url, sandbox);
    } catch (error) {
      if (/^Error opening input stream/.test(String(error))) {
        throw new Error(
          "Can't load module '" + module.id + "' with url '" + url + "'!"
        );
      }
      throw error;
    }

    // The value of exports may have been changed by the module script, so
    // freeze it if and only if it is still an object.
    if (typeof module.exports === "object" && module.exports !== null) {
      Object.freeze(module.exports);
    }
  }

  /**
   * Create a require function for the given module. If no module is given,
   * create a require function for the top-level module instead.
   *
   * @param Object requirer
   *        The module for which the require function is to be created.
   *
   * @return Function
   *         A require function for the given module.
   */
  function createRequire(requirer) {
    return function require(id) {
      // Make sure an id was passed.
      if (id === undefined) {
        throw new Error("Can't require module without id!");
      }

      // Built-in modules are cached by id rather than URL, so try to find the
      // module to be required by id first.
      let module = modules[id];
      if (module === undefined) {
        // Failed to find the module to be required by id, so convert the id to
        // a URL and try again.

        // If the id is relative, convert it to an absolute id.
        if (id.startsWith(".")) {
          if (requirer === undefined) {
            throw new Error(
              "Can't require top-level module with relative id " +
                "'" +
                id +
                "'!"
            );
          }
          id = resolve(id, requirer.id);
        }

        // Convert the absolute id to a normalized id.
        id = normalizeId(id);

        // Convert the normalized id to a URL.
        let url = id;

        // If the URL is relative, resolve it to an absolute URL.
        if (url.match(/^\w+:\/\//) === null) {
          url = resolveURL(id);
        }

        // Try to find the module to be required by URL.
        module = modules[url];
        if (module === undefined) {
          // Failed to find the module to be required in the cache, so create
          // a new module, load it from the given URL, and add it to the cache.

          // Add modules to the cache early so that any recursive calls to
          // require for the same module will return the partially-loaded module
          // from the cache instead of triggering a new load.
          module = modules[url] = createModule(id);

          try {
            loadModule(module, url);
          } catch (error) {
            // If the module failed to load, remove it from the cache so that
            // subsequent calls to require for the same module will trigger a
            // new load, instead of returning a partially-loaded module from
            // the cache.
            delete modules[url];
            throw error;
          }

          Object.freeze(module);
        }
      }

      return module.exports;
    };
  }

  const createSandbox = options.createSandbox;
  const globals = options.globals || Object.create(null);
  const loadSubScript = options.loadSubScript;

  // Create the module cache, by converting each entry in the map from
  // normalized ids to built-in modules to a module object, with the exports
  // property of each module set to a frozen version of the original entry.
  const modules = options.modules || {};
  for (const id in modules) {
    const module = createModule(id);
    module.exports = Object.freeze(modules[id]);
    modules[id] = module;
  }

  // Convert the map of paths to base URLs into an array for use by resolveURL.
  // The array is sorted from longest to shortest path to ensure that the
  // longest path is always the first to be found.
  let paths = options.paths || Object.create(null);
  paths = Object.keys(paths)
    .sort((a, b) => b.length - a.length)
    .map(path => [path, paths[path]]);

  const resolve = options.resolve || resolveId;

  this.require = createRequire();
}

this.WorkerDebuggerLoader = WorkerDebuggerLoader;

// The following APIs rely on the use of Components, and the worker debugger
// does not provide alternative definitions for them. Consequently, they are
// stubbed out both on the main thread and worker threads.

var chrome = {
  CC: undefined,
  Cc: undefined,
  ChromeWorker: undefined,
  Cm: undefined,
  Ci: undefined,
  Cu: undefined,
  Cr: undefined,
  components: undefined,
};

var loader = {
  lazyGetter: function(object, name, lambda) {
    Object.defineProperty(object, name, {
      get: function() {
        delete object[name];
        object[name] = lambda.apply(object);
        return object[name];
      },
      configurable: true,
      enumerable: true,
    });
  },
  lazyImporter: function() {
    throw new Error("Can't import JSM from worker thread!");
  },
  lazyServiceGetter: function() {
    throw new Error("Can't import XPCOM service from worker thread!");
  },
  lazyRequireGetter: function(obj, property, module, destructure) {
    Object.defineProperty(obj, property, {
      get: () =>
        destructure
          ? worker.require(module)[property]
          : worker.require(module || property),
    });
  },
};

// The following APIs are defined differently depending on whether we are on the
// main thread or a worker thread. On the main thread, we use the Components
// object to implement them. On worker threads, we use the APIs provided by
// the worker debugger.

/* eslint-disable no-shadow */
var {
  Debugger,
  URL,
  createSandbox,
  dump,
  rpc,
  loadSubScript,
  reportError,
  setImmediate,
  xpcInspector,
} = function() {
  // Main thread
  if (typeof Components === "object") {
    const { Constructor: CC } = Components;

    const principal = CC("@mozilla.org/systemprincipal;1", "nsIPrincipal")();

    // To ensure that the this passed to addDebuggerToGlobal is a global, the
    // Debugger object needs to be defined in a sandbox.
    const sandbox = Cu.Sandbox(principal, {});
    Cu.evalInSandbox(
      "Components.utils.import('resource://gre/modules/jsdebugger.jsm');" +
        "addDebuggerToGlobal(this);",
      sandbox
    );
    const Debugger = sandbox.Debugger;

    const createSandbox = function(name, prototype) {
      return Cu.Sandbox(principal, {
        invisibleToDebugger: true,
        sandboxName: name,
        sandboxPrototype: prototype,
        wantComponents: false,
        wantXrays: false,
      });
    };

    const rpc = undefined;

    // eslint-disable-next-line mozilla/use-services
    const subScriptLoader = Cc[
      "@mozilla.org/moz/jssubscript-loader;1"
    ].getService(Ci.mozIJSSubScriptLoader);

    const loadSubScript = function(url, sandbox) {
      subScriptLoader.loadSubScript(url, sandbox);
    };

    const reportError = Cu.reportError;

    const Timer = ChromeUtils.import("resource://gre/modules/Timer.jsm");

    const setImmediate = function(callback) {
      Timer.setTimeout(callback, 0);
    };

    const xpcInspector = Cc["@mozilla.org/jsinspector;1"].getService(
      Ci.nsIJSInspector
    );

    const { URL } = Cu.Sandbox(principal, {
      wantGlobalProperties: ["URL"],
    });

    return {
      Debugger,
      URL: URL,
      createSandbox,
      dump: this.dump,
      rpc,
      loadSubScript,
      reportError,
      setImmediate,
      xpcInspector,
    };
  }
  // Worker thread
  const requestors = [];

  const scope = this;

  const xpcInspector = {
    get eventLoopNestLevel() {
      return requestors.length;
    },

    get lastNestRequestor() {
      return requestors.length === 0 ? null : requestors[requestors.length - 1];
    },

    enterNestedEventLoop: function(requestor) {
      requestors.push(requestor);
      scope.enterEventLoop();
      return requestors.length;
    },

    exitNestedEventLoop: function() {
      requestors.pop();
      scope.leaveEventLoop();
      return requestors.length;
    },
  };

  return {
    Debugger: this.Debugger,
    URL: this.URL,
    createSandbox: this.createSandbox,
    dump: this.dump,
    rpc: this.rpc,
    loadSubScript: this.loadSubScript,
    reportError: this.reportError,
    setImmediate: this.setImmediate,
    xpcInspector: xpcInspector,
  };
}.call(this);
/* eslint-enable no-shadow */

// Create the default instance of the worker loader, using the APIs we defined
// above.

this.worker = new WorkerDebuggerLoader({
  createSandbox: createSandbox,
  globals: {
    isWorker: true,
    dump: dump,
    loader: loader,
    reportError: reportError,
    rpc: rpc,
    URL: URL,
    setImmediate: setImmediate,
    retrieveConsoleEvents: this.retrieveConsoleEvents,
    setConsoleEventHandler: this.setConsoleEventHandler,
    console: console,
    btoa: this.btoa,
    atob: this.atob,
  },
  loadSubScript: loadSubScript,
  modules: {
    Debugger: Debugger,
    Services: Object.create(null),
    chrome: chrome,
    xpcInspector: xpcInspector,
    ChromeUtils: ChromeUtils,
    DebuggerNotificationObserver: DebuggerNotificationObserver,
  },
  paths: {
    // ⚠ DISCUSSION ON DEV-DEVELOPER-TOOLS REQUIRED BEFORE MODIFYING ⚠
    devtools: "resource://devtools",
    // ⚠ DISCUSSION ON DEV-DEVELOPER-TOOLS REQUIRED BEFORE MODIFYING ⚠
    promise: "resource://gre/modules/Promise-backend.js",
    // ⚠ DISCUSSION ON DEV-DEVELOPER-TOOLS REQUIRED BEFORE MODIFYING ⚠
    "xpcshell-test": "resource://test",
    // ⚠ DISCUSSION ON DEV-DEVELOPER-TOOLS REQUIRED BEFORE MODIFYING ⚠
  },
});
