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

const { Cu, Cc, Ci, Services } = require("chrome");
const jsmScope = require("resource://devtools/shared/loader/Loader.jsm");

const systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();

// Steal various globals only available in JSM scope (and not Sandbox one)
const {
  CanonicalBrowsingContext,
  BrowsingContext,
  WebExtensionPolicy,
  WindowGlobalParent,
  WindowGlobalChild,
  console,
  DebuggerNotificationObserver,
  DOMPoint,
  DOMQuad,
  DOMRect,
  HeapSnapshot,
  IOUtils,
  L10nRegistry,
  Localization,
  NamedNodeMap,
  NodeFilter,
  PathUtils,
  StructuredCloneHolder,
  TelemetryStopwatch,
} = Cu.getGlobalForObject(jsmScope);

// Create a single Sandbox to access global properties needed in this module.
// Sandbox are memory expensive, so we should create as little as possible.
const debuggerSandbox = (exports.internalSandbox = Cu.Sandbox(systemPrincipal, {
  // This sandbox is also reused for ChromeDebugger implementation.
  // As we want to load the `Debugger` API for debugging chrome contexts,
  // we have to ensure loading it in a distinct compartment from its debuggee.
  freshCompartment: true,

  wantGlobalProperties: [
    "AbortController",
    "atob",
    "btoa",
    "Blob",
    "ChromeUtils",
    "crypto",
    "CSS",
    "CSSRule",
    "DOMParser",
    "Element",
    "Event",
    "FileReader",
    "FormData",
    "Headers",
    "indexedDB",
    "InspectorUtils",
    "Node",
    "TextDecoder",
    "TextEncoder",
    "URL",
    "URLSearchParams",
    "Window",
    "XMLHttpRequest",
  ],
}));

const {
  AbortController,
  atob,
  btoa,
  Blob,
  ChromeUtils,
  crypto,
  CSS,
  CSSRule,
  DOMParser,
  Element,
  Event,
  FileReader,
  FormData,
  Headers,
  indexedDB,
  InspectorUtils,
  Node,
  TextDecoder,
  TextEncoder,
  URL,
  URLSearchParams,
  Window,
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
    get() {
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
 * be imported until first use.
 *
 * @param object
 *        The object to define the lazy getter on.
 * @param name
 *        The name of the getter to define on object for the module.
 * @param resource
 *        The URL used to obtain the module.
 */
function defineLazyModuleGetter(object, name, resource) {
  defineLazyGetter(object, name, function() {
    try {
      return ChromeUtils.import(resource)[name];
    } catch (ex) {
      Cu.reportError("Failed to load module " + resource + ".");
      throw ex;
    }
  });
}

/**
 * Define a getter property on the given object that requires the given
 * module. This enables delaying importing modules until the module is
 * actually used.
 *
 * Several getters can be defined at once by providing an array of
 * properties and enabling destructuring.
 *
 * @param { Object } obj
 *    The object to define the property on.
 * @param { String | Array<String> } properties
 *    String: Name of the property for the getter.
 *    Array<String>: When destructure is true, properties can be an array of
 *    strings to create several getters at once.
 * @param { String } module
 *    The module path.
 * @param { Boolean } destructure
 *    Pass true if the property name is a member of the module's exports.
 */
function lazyRequireGetter(obj, properties, module, destructure) {
  if (Array.isArray(properties) && !destructure) {
    throw new Error(
      "Pass destructure=true to call lazyRequireGetter with an array of properties"
    );
  }

  if (!Array.isArray(properties)) {
    properties = [properties];
  }

  for (const property of properties) {
    defineLazyGetter(obj, property, () => {
      return destructure
        ? require(module)[property]
        : require(module || property);
    });
  }
}

// List of pseudo modules exposed to all devtools modules.
exports.modules = {
  DebuggerNotificationObserver,
  HeapSnapshot,
  InspectorUtils,
  // Expose "chrome" Promise, which aren't related to any document
  // and so are never frozen, even if the browser loader module which
  // pull it is destroyed. See bug 1402779.
  Promise,
  TelemetryStopwatch,
};

defineLazyGetter(exports.modules, "Debugger", () => {
  const global = Cu.getGlobalForObject(this);
  // Debugger may already have been added.
  if (global.Debugger) {
    return global.Debugger;
  }
  const { addDebuggerToGlobal } = ChromeUtils.import(
    "resource://gre/modules/jsdebugger.jsm"
  );
  addDebuggerToGlobal(global);
  return global.Debugger;
});

defineLazyGetter(exports.modules, "ChromeDebugger", () => {
  const { addDebuggerToGlobal } = ChromeUtils.import(
    "resource://gre/modules/jsdebugger.jsm"
  );
  addDebuggerToGlobal(debuggerSandbox);
  return debuggerSandbox.Debugger;
});

defineLazyGetter(exports.modules, "xpcInspector", () => {
  return Cc["@mozilla.org/jsinspector;1"].getService(Ci.nsIJSInspector);
});

// List of all custom globals exposed to devtools modules.
// Changes here should be mirrored to devtools/.eslintrc.
exports.globals = {
  AbortController,
  atob,
  Blob,
  btoa,
  CanonicalBrowsingContext,
  ChromeUtils,
  BrowsingContext,
  WebExtensionPolicy,
  WindowGlobalParent,
  WindowGlobalChild,
  console,
  crypto,
  CSS,
  CSSRule,
  DOMParser,
  DOMPoint,
  DOMQuad,
  Event,
  NamedNodeMap,
  NodeFilter,
  DOMRect,
  Element,
  FileReader,
  FormData,
  Headers,
  IOUtils,
  isWorker: false,
  L10nRegistry,
  loader: {
    lazyGetter: defineLazyGetter,
    lazyImporter: defineLazyModuleGetter,
    lazyServiceGetter: defineLazyServiceGetter,
    lazyRequireGetter,
    // Defined by Loader.jsm
    id: null,
  },
  Localization,
  Node,
  PathUtils,
  reportError: Cu.reportError,
  Services: Object.create(Services),
  StructuredCloneHolder,
  TextDecoder,
  TextEncoder,
  URL,
  URLSearchParams,
  Window,
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
    get() {
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
  return require("devtools/shared/indexed-db").createDevToolsIndexedDB(
    indexedDB
  );
});
