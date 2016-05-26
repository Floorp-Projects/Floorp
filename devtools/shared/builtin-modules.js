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
        atob, btoa, Iterator } = jsmScope;

/**
 * Defines a getter on a specified object that will be created upon first use.
 *
 * @param aObject
 *        The object to define the lazy getter on.
 * @param aName
 *        The name of the getter to define on aObject.
 * @param aLambda
 *        A function that returns what the getter should return.  This will
 *        only ever be called once.
 */
function defineLazyGetter(aObject, aName, aLambda)
{
  Object.defineProperty(aObject, aName, {
    get: function () {
      // Redefine this accessor property as a data property.
      // Delete it first, to rule out "too much recursion" in case aObject is
      // a proxy whose defineProperty handler might unwittingly trigger this
      // getter again.
      delete aObject[aName];
      let value = aLambda.apply(aObject);
      Object.defineProperty(aObject, aName, {
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
 * @param aObject
 *        The object to define the lazy getter on.
 * @param aName
 *        The name of the getter to define on aObject for the service.
 * @param aContract
 *        The contract used to obtain the service.
 * @param aInterfaceName
 *        The name of the interface to query the service to.
 */
function defineLazyServiceGetter(aObject, aName, aContract, aInterfaceName)
{
  defineLazyGetter(aObject, aName, function XPCU_serviceLambda() {
    return Cc[aContract].getService(Ci[aInterfaceName]);
  });
}

/**
 * Defines a getter on a specified object for a module.  The module will not
 * be imported until first use. The getter allows to execute setup and
 * teardown code (e.g.  to register/unregister to services) and accepts
 * a proxy object which acts on behalf of the module until it is imported.
 *
 * @param aObject
 *        The object to define the lazy getter on.
 * @param aName
 *        The name of the getter to define on aObject for the module.
 * @param aResource
 *        The URL used to obtain the module.
 * @param aSymbol
 *        The name of the symbol exported by the module.
 *        This parameter is optional and defaults to aName.
 * @param aPreLambda
 *        A function that is executed when the proxy is set up.
 *        This will only ever be called once.
 * @param aPostLambda
 *        A function that is executed when the module has been imported to
 *        run optional teardown procedures on the proxy object.
 *        This will only ever be called once.
 * @param aProxy
 *        An object which acts on behalf of the module to be imported until
 *        the module has been imported.
 */
function defineLazyModuleGetter(aObject, aName, aResource, aSymbol,
                                aPreLambda, aPostLambda, aProxy)
{
  let proxy = aProxy || {};

  if (typeof (aPreLambda) === "function") {
    aPreLambda.apply(proxy);
  }

  defineLazyGetter(aObject, aName, function XPCU_moduleLambda() {
    var temp = {};
    try {
      Cu.import(aResource, temp);

      if (typeof (aPostLambda) === "function") {
        aPostLambda.apply(proxy);
      }
    } catch (ex) {
      Cu.reportError("Failed to load module " + aResource + ".");
      throw ex;
    }
    return temp[aSymbol || aName];
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

defineLazyGetter(exports.modules, "indexedDB", () => {
  // On xpcshell, we can't instantiate indexedDB without crashing
  try {
    let sandbox
      = Cu.Sandbox(CC("@mozilla.org/systemprincipal;1", "nsIPrincipal")(),
                   {wantGlobalProperties: ["indexedDB"]});
    return sandbox.indexedDB;

  } catch (e) {
    return {};
  }
});

defineLazyGetter(exports.modules, "CSS", () => {
  let sandbox
    = Cu.Sandbox(CC("@mozilla.org/systemprincipal;1", "nsIPrincipal")(),
                 {wantGlobalProperties: ["CSS"]});
  return sandbox.CSS;
});

// List of all custom globals exposed to devtools modules.
// Changes here should be mirrored to devtools/.eslintrc.
const globals = exports.globals = {
  isWorker: false,
  reportError: Cu.reportError,
  atob: atob,
  btoa: btoa,
  _Iterator: Iterator,
  loader: {
    lazyGetter: defineLazyGetter,
    lazyImporter: defineLazyModuleGetter,
    lazyServiceGetter: defineLazyServiceGetter,
    lazyRequireGetter: lazyRequireGetter,
    id: null // Defined by Loader.jsm
  },

  // Let new XMLHttpRequest do the right thing.
  XMLHttpRequest: function () {
    return Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
           .createInstance(Ci.nsIXMLHttpRequest);
  },

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

// Lazily define a few things so that the corresponding jsms are only loaded
// when used.
defineLazyGetter(globals, "console", () => {
  return Cu.import("resource://gre/modules/Console.jsm", {}).console;
});

defineLazyGetter(globals, "clearTimeout", () => {
  return Cu.import("resource://gre/modules/Timer.jsm", {}).clearTimeout;
});
defineLazyGetter(globals, "setTimeout", () => {
  return Cu.import("resource://gre/modules/Timer.jsm", {}).setTimeout;
});
defineLazyGetter(globals, "clearInterval", () => {
  return Cu.import("resource://gre/modules/Timer.jsm", {}).clearInterval;
});
defineLazyGetter(globals, "setInterval", () => {
  return Cu.import("resource://gre/modules/Timer.jsm", {}).setInterval;
});
defineLazyGetter(globals, "URL", () => {
  let sandbox
    = Cu.Sandbox(CC("@mozilla.org/systemprincipal;1", "nsIPrincipal")(),
                 {wantGlobalProperties: ["URL"]});
  return sandbox.URL;
});
