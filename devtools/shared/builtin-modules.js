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

const { Cu, Cc, Ci } = require("chrome");
const promise = require("resource://gre/modules/Promise.jsm").Promise;
const jsmScope = require("resource://devtools/shared/Loader.jsm");
const { Services } = require("resource://gre/modules/Services.jsm");

const systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();

// Steal various globals only available in JSM scope (and not Sandbox one)
const {
  console,
  DOMPoint,
  DOMQuad,
  DOMRect,
  HeapSnapshot,
  NamedNodeMap,
  NodeFilter,
  StructuredCloneHolder,
  TelemetryStopwatch,
} = Cu.getGlobalForObject(jsmScope);

// Create a single Sandbox to access global properties needed in this module.
// Sandbox are memory expensive, so we should create as little as possible.
const debuggerSandbox = Cu.Sandbox(systemPrincipal, {
  // This sandbox is also reused for ChromeDebugger implementation.
  // As we want to load the `Debugger` API for debugging chrome contexts,
  // we have to ensure loading it in a distinct compartment from its debuggee.
  // invisibleToDebugger does that and helps the Debugger API identify the boundaries
  // between debuggee and debugger code.
  invisibleToDebugger: true,

  wantGlobalProperties: [
    "atob",
    "btoa",
    "Blob",
    "ChromeUtils",
    "CSS",
    "CSSRule",
    "DOMParser",
    "Element",
    "Event",
    "FileReader",
    "FormData",
    "indexedDB",
    "InspectorUtils",
    "Node",
    "TextDecoder",
    "TextEncoder",
    "URL",
    "XMLHttpRequest",
  ],
});

const {
  atob,
  btoa,
  Blob,
  ChromeUtils,
  CSS,
  CSSRule,
  DOMParser,
  Element,
  Event,
  FileReader,
  FormData,
  indexedDB,
  InspectorUtils,
  Node,
  TextDecoder,
  TextEncoder,
  URL,
  XMLHttpRequest,
} = debuggerSandbox;

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
    get: function() {
      // Redefine this accessor property as a data property.
      // Delete it first, to rule out "too much recursion" in case object is
      // a proxy whose defineProperty handler might unwittingly trigger this
      // getter again.
      delete object[name];
      const value = lambda.apply(object);
      Object.defineProperty(object, name, {
        value,
        writable: true,
        configurable: true,
        enumerable: true,
      });
      return value;
    },
    configurable: true,
    enumerable: true,
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
  defineLazyGetter(object, name, function() {
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

  defineLazyGetter(object, name, function() {
    const temp = {};
    try {
      ChromeUtils.import(resource, temp);

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
      const value = destructure
        ? require(module)[property]
        : require(module || property);
      Object.defineProperty(obj, property, {
        value,
        writable: true,
        configurable: true,
        enumerable: true,
      });
      return value;
    },
    configurable: true,
    enumerable: true,
  });
}

// List of pseudo modules exposed to all devtools modules.
exports.modules = {
  ChromeUtils,
  HeapSnapshot,
  promise,
  // Expose "chrome" Promise, which aren't related to any document
  // and so are never frozen, even if the browser loader module which
  // pull it is destroyed. See bug 1402779.
  Promise,
  Services: Object.create(Services),
  TelemetryStopwatch,
};

defineLazyGetter(exports.modules, "Debugger", () => {
  const global = Cu.getGlobalForObject(this);
  // Debugger may already have been added by RecordReplayControl getter
  if (global.Debugger) {
    return global.Debugger;
  }
  const { addDebuggerToGlobal } = ChromeUtils.import("resource://gre/modules/jsdebugger.jsm");
  addDebuggerToGlobal(global);
  return global.Debugger;
});

defineLazyGetter(exports.modules, "ChromeDebugger", () => {
  const { addDebuggerToGlobal } = ChromeUtils.import("resource://gre/modules/jsdebugger.jsm");
  addDebuggerToGlobal(debuggerSandbox);
  return debuggerSandbox.Debugger;
});

defineLazyGetter(exports.modules, "RecordReplayControl", () => {
  // addDebuggerToGlobal also adds the RecordReplayControl object.
  const global = Cu.getGlobalForObject(this);
  // RecordReplayControl may already have been added by Debugger getter
  if (global.RecordReplayControl) {
    return global.RecordReplayControl;
  }
  const { addDebuggerToGlobal } = ChromeUtils.import("resource://gre/modules/jsdebugger.jsm");
  addDebuggerToGlobal(global);
  return global.RecordReplayControl;
});

defineLazyGetter(exports.modules, "InspectorUtils", () => {
  if (exports.modules.Debugger.recordReplayProcessKind() == "Middleman") {
    const ReplayInspector = require("devtools/server/actors/replay/inspector");
    return ReplayInspector.createInspectorUtils(InspectorUtils);
  }
  return InspectorUtils;
});

defineLazyGetter(exports.modules, "Timer", () => {
  const {setTimeout, clearTimeout} = require("resource://gre/modules/Timer.jsm");
  // Do not return Cu.import result, as DevTools loader would freeze Timer.jsm globals...
  return {
    setTimeout,
    clearTimeout,
  };
});

defineLazyGetter(exports.modules, "xpcInspector", () => {
  return Cc["@mozilla.org/jsinspector;1"].getService(Ci.nsIJSInspector);
});

// List of all custom globals exposed to devtools modules.
// Changes here should be mirrored to devtools/.eslintrc.
exports.globals = {
  atob,
  Blob,
  btoa,
  console,
  CSS,
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
  DOMParser,
  DOMPoint,
  DOMQuad,
  NamedNodeMap,
  NodeFilter,
  DOMRect,
  Element,
  Event,
  FileReader,
  FormData,
  isWorker: false,
  loader: {
    lazyGetter: defineLazyGetter,
    lazyImporter: defineLazyModuleGetter,
    lazyServiceGetter: defineLazyServiceGetter,
    lazyRequireGetter: lazyRequireGetter,
    // Defined by Loader.jsm
    id: null,
  },
  Node,
  reportError: Cu.reportError,
  StructuredCloneHolder,
  TextDecoder,
  TextEncoder,
  URL,
  XMLHttpRequest,
};
// DevTools loader copy globals property descriptors on each module global
// object so that we have to memoize them from here in order to instantiate each
// global only once.
// `globals` is a cache object on which we put all global values
// and we set getters on `exports.globals` returning `globals` values.
const globals = {};
function lazyGlobal(name, getter) {
  defineLazyGetter(globals, name, getter);
  Object.defineProperty(exports.globals, name, {
    get: function() {
      return globals[name];
    },
    configurable: true,
    enumerable: true,
  });
}

// Lazily define a few things so that the corresponding jsms are only loaded
// when used.
lazyGlobal("clearTimeout", () => {
  return require("resource://gre/modules/Timer.jsm").clearTimeout;
});
lazyGlobal("setTimeout", () => {
  return require("resource://gre/modules/Timer.jsm").setTimeout;
});
lazyGlobal("clearInterval", () => {
  return require("resource://gre/modules/Timer.jsm").clearInterval;
});
lazyGlobal("setInterval", () => {
  return require("resource://gre/modules/Timer.jsm").setInterval;
});
lazyGlobal("WebSocket", () => {
  return Services.appShell.hiddenDOMWindow.WebSocket;
});
lazyGlobal("indexedDB", () => {
  return require("devtools/shared/indexed-db").createDevToolsIndexedDB(indexedDB);
});
lazyGlobal("isReplaying", () => {
  return exports.modules.Debugger.recordReplayProcessKind() == "Middleman";
});
lazyGlobal("CSSRule", () => {
  if (exports.modules.Debugger.recordReplayProcessKind() == "Middleman") {
    const ReplayInspector = require("devtools/server/actors/replay/inspector");
    return ReplayInspector.createCSSRule(CSSRule);
  }
  return CSSRule;
});
