/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module defines custom globals injected in all our modules and also
 * pseudo modules that aren't separate files but just dynamically set values.
 *
 * As it does so, the module itself doesn't have access to these globals,
 * nor the pseudo modules. Be careful to avoid loading any other js module as
 * they would also miss them.
 */

const { Cu, CC, Cc, Ci } = require("chrome");
const { Loader } = Cu.import("resource://gre/modules/commonjs/toolkit/loader.js", {});
const promise = Cu.import("resource://gre/modules/Promise.jsm", {}).Promise;
const jsmScope = Cu.import("resource://gre/modules/Services.jsm", {});
const { Services } = jsmScope;
// Steal various globals only available in JSM scope (and not Sandbox one)
const { PromiseDebugging, ChromeUtils, ThreadSafeChromeUtils, HeapSnapshot,
        atob, btoa, TextEncoder, TextDecoder } = jsmScope;
const { URL } = Cu.Sandbox(CC("@mozilla.org/systemprincipal;1", "nsIPrincipal")(),
                           {wantGlobalProperties: ["URL"]});

/**
 * Defines a getter on a specified object that will be created upon first use.
 *
 * @param object
 *        The object to define the lazy getter on.
 * @param name
 *        The name of the getter to define on object.
 * @param lambda
 *        A function that returns what the getter should return.  This will
 *        only ever be called once.
 */
function defineLazyGetter(object, name, lambda) {
  Object.defineProperty(object, name, {
    get: function () {
      // Redefine this accessor property as a data property.
      // Delete it first, to rule out "too much recursion" in case object is
      // a proxy whose defineProperty handler might unwittingly trigger this
      // getter again.
      delete object[name];
      let value = lambda.apply(object);
      Object.defineProperty(object, name, {
        value,
        writable: true,
        configurable: true,
        enumerable: true
      });
      return value;
    },
    configurable: true,
    enumerable: true
  });
}

/**
 * Defines a getter on a specified object for a service.  The service will not
 * be obtained until first use.
 *
 * @param object
 *        The object to define the lazy getter on.
 * @param name
 *        The name of the getter to define on object for the service.
 * @param contract
 *        The contract used to obtain the service.
 * @param interfaceName
 *        The name of the interface to query the service to.
 */
function defineLazyServiceGetter(object, name, contract, interfaceName) {
  defineLazyGetter(object, name, function () {
    return Cc[contract].getService(Ci[interfaceName]);
  });
}

/**
 * Defines a getter on a specified object for a module.  The module will not
 * be imported until first use. The getter allows to execute setup and
 * teardown code (e.g.  to register/unregister to services) and accepts
 * a proxy object which acts on behalf of the module until it is imported.
 *
 * @param object
 *        The object to define the lazy getter on.
 * @param name
 *        The name of the getter to define on object for the module.
 * @param resource
 *        The URL used to obtain the module.
 * @param symbol
 *        The name of the symbol exported by the module.
 *        This parameter is optional and defaults to name.
 * @param preLambda
 *        A function that is executed when the proxy is set up.
 *        This will only ever be called once.
 * @param postLambda
 *        A function that is executed when the module has been imported to
 *        run optional teardown procedures on the proxy object.
 *        This will only ever be called once.
 * @param proxy
 *        An object which acts on behalf of the module to be imported until
 *        the module has been imported.
 */
function defineLazyModuleGetter(object, name, resource, symbol,
                                preLambda, postLambda, proxy) {
  proxy = proxy || {};

  if (typeof (preLambda) === "function") {
    preLambda.apply(proxy);
  }

  defineLazyGetter(object, name, function () {
    let temp = {};
    try {
      Cu.import(resource, temp);

      if (typeof (postLambda) === "function") {
        postLambda.apply(proxy);
      }
    } catch (ex) {
      Cu.reportError("Failed to load module " + resource + ".");
      throw ex;
    }
    return temp[symbol || name];
  });
}

/**
 * Define a getter property on the given object that requires the given
 * module. This enables delaying importing modules until the module is
 * actually used.
 *
 * @param Object obj
 *    The object to define the property on.
 * @param String property
 *    The property name.
 * @param String module
 *    The module path.
 * @param Boolean destructure
 *    Pass true if the property name is a member of the module's exports.
 */
function lazyRequireGetter(obj, property, module, destructure) {
  Object.defineProperty(obj, property, {
    get: () => {
      // Redefine this accessor property as a data property.
      // Delete it first, to rule out "too much recursion" in case obj is
      // a proxy whose defineProperty handler might unwittingly trigger this
      // getter again.
      delete obj[property];
      let value = destructure
        ? require(module)[property]
        : require(module || property);
      Object.defineProperty(obj, property, {
        value,
        writable: true,
        configurable: true,
        enumerable: true
      });
      return value;
    },
    configurable: true,
    enumerable: true
  });
}

// List of pseudo modules exposed to all devtools modules.
exports.modules = {
  "Services": Object.create(Services),
  "toolkit/loader": Loader,
  promise,
  PromiseDebugging,
  ChromeUtils,
  ThreadSafeChromeUtils,
  HeapSnapshot,
};

defineLazyGetter(exports.modules, "Debugger", () => {
  // addDebuggerToGlobal only allows adding the Debugger object to a global. The
  // this object is not guaranteed to be a global (in particular on B2G, due to
  // compartment sharing), so add the Debugger object to a sandbox instead.
  let sandbox = Cu.Sandbox(CC("@mozilla.org/systemprincipal;1", "nsIPrincipal")());
  Cu.evalInSandbox(
    "Components.utils.import('resource://gre/modules/jsdebugger.jsm');" +
    "addDebuggerToGlobal(this);",
    sandbox
  );
  return sandbox.Debugger;
});

defineLazyGetter(exports.modules, "Timer", () => {
  let {setTimeout, clearTimeout} = Cu.import("resource://gre/modules/Timer.jsm", {});
  // Do not return Cu.import result, as SDK loader would freeze Timer.jsm globals...
  return {
    setTimeout,
    clearTimeout
  };
});

defineLazyGetter(exports.modules, "xpcInspector", () => {
  return Cc["@mozilla.org/jsinspector;1"].getService(Ci.nsIJSInspector);
});

defineLazyGetter(exports.modules, "FileReader", () => {
  let sandbox
    = Cu.Sandbox(CC("@mozilla.org/systemprincipal;1", "nsIPrincipal")(),
                 {wantGlobalProperties: ["FileReader"]});
  return sandbox.FileReader;
});

// List of all custom globals exposed to devtools modules.
// Changes here should be mirrored to devtools/.eslintrc.
exports.globals = {
  isWorker: false,
  reportError: Cu.reportError,
  atob: atob,
  btoa: btoa,
  TextEncoder: TextEncoder,
  TextDecoder: TextDecoder,
  URL,
  loader: {
    lazyGetter: defineLazyGetter,
    lazyImporter: defineLazyModuleGetter,
    lazyServiceGetter: defineLazyServiceGetter,
    lazyRequireGetter: lazyRequireGetter,
    // Defined by Loader.jsm
    id: null
  },

  // Let new XMLHttpRequest do the right thing.
  XMLHttpRequest: function () {
    return Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
           .createInstance(Ci.nsIXMLHttpRequest);
  },

  Node: Ci.nsIDOMNode,
  Element: Ci.nsIDOMElement,
  DocumentFragment: Ci.nsIDOMDocumentFragment,

  // Make sure `define` function exists.  This allows defining some modules
  // in AMD format while retaining CommonJS compatibility through this hook.
  // JSON Viewer needs modules in AMD format, as it currently uses RequireJS
  // from a content document and can't access our usual loaders.  So, any
  // modules shared with the JSON Viewer should include a define wrapper:
  //
  //   // Make this available to both AMD and CJS environments
  //   define(function(require, exports, module) {
  //     ... code ...
  //   });
  //
  // Bug 1248830 will work out a better plan here for our content module
  // loading needs, especially as we head towards devtools.html.
  define(factory) {
    factory(this.require, this.exports, this.module);
  },
};
// SDK loader copy globals property descriptors on each module global object
// so that we have to memoize them from here in order to instanciate each
// global only once.
// `globals` is a cache object on which we put all global values
// and we set getters on `exports.globals` returning `globals` values.
let globals = {};
function lazyGlobal(name, getter) {
  defineLazyGetter(globals, name, getter);
  Object.defineProperty(exports.globals, name, {
    get: function () {
      return globals[name];
    },
    configurable: true,
    enumerable: true
  });
}

// Lazily define a few things so that the corresponding jsms are only loaded
// when used.
lazyGlobal("console", () => {
  return Cu.import("resource://gre/modules/Console.jsm", {}).console;
});
lazyGlobal("clearTimeout", () => {
  return Cu.import("resource://gre/modules/Timer.jsm", {}).clearTimeout;
});
lazyGlobal("setTimeout", () => {
  return Cu.import("resource://gre/modules/Timer.jsm", {}).setTimeout;
});
lazyGlobal("clearInterval", () => {
  return Cu.import("resource://gre/modules/Timer.jsm", {}).clearInterval;
});
lazyGlobal("setInterval", () => {
  return Cu.import("resource://gre/modules/Timer.jsm", {}).setInterval;
});
lazyGlobal("CSSRule", () => Ci.nsIDOMCSSRule);
lazyGlobal("DOMParser", () => {
  return CC("@mozilla.org/xmlextras/domparser;1", "nsIDOMParser");
});
lazyGlobal("CSS", () => {
  let sandbox
    = Cu.Sandbox(CC("@mozilla.org/systemprincipal;1", "nsIPrincipal")(),
                 {wantGlobalProperties: ["CSS"]});
  return sandbox.CSS;
});
lazyGlobal("WebSocket", () => {
  return Services.appShell.hiddenDOMWindow.WebSocket;
});
lazyGlobal("indexedDB", () => {
  let { indexedDB } = Cu.Sandbox(CC("@mozilla.org/systemprincipal;1", "nsIPrincipal")(),
                 {wantGlobalProperties: ["indexedDB"]});
  return require("devtools/shared/indexed-db").createDevToolsIndexedDB(indexedDB);
});
