/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Manages the base loader (base-loader.js) instance used to load the developer tools.
 */

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
var { Loader, Require, resolveURI, unload } = ChromeUtils.import(
  "resource://devtools/shared/base-loader.js"
);
var { requireRawId } = ChromeUtils.import(
  "resource://devtools/shared/loader-plugin-raw.jsm"
);

const EXPORTED_SYMBOLS = [
  "DevToolsLoader",
  "require",
  "loader",
  // Export StructuredCloneHolder for its use from builtin-modules
  "StructuredCloneHolder",
];

var gNextLoaderID = 0;

/**
 * The main devtools API. The standard instance of this loader is exported as
 * |loader| below, but if a fresh copy of the loader is needed, then a new
 * one can also be created.
 *
 * The two following boolean flags are used to control the sandboxes into
 * which the modules are loaded.
 * @param invisibleToDebugger boolean
 *        If true, the modules won't be visible by the Debugger API.
 *        This typically allows to hide server modules from the debugger panel.
 * @param freshCompartment boolean
 *        If true, the modules will be forced to be loaded in a distinct
 *        compartment. It is typically used to load the modules in a distinct
 *        system compartment, different from the main one, which is shared by
 *        all JSMs, XPCOMs and modules loaded with this flag set to true.
 *        We use this in order to debug modules loaded in this shared system
 *        compartment. The debugger actor has to be running in a distinct
 *        compartment than the context it is debugging.
 */
function DevToolsLoader({
  invisibleToDebugger = false,
  freshCompartment = false,
} = {}) {
  const paths = {
    // ⚠ DISCUSSION ON DEV-DEVELOPER-TOOLS REQUIRED BEFORE MODIFYING ⚠
    devtools: "resource://devtools",
    // ⚠ DISCUSSION ON DEV-DEVELOPER-TOOLS REQUIRED BEFORE MODIFYING ⚠
    acorn: "resource://devtools/shared/acorn",
    // ⚠ DISCUSSION ON DEV-DEVELOPER-TOOLS REQUIRED BEFORE MODIFYING ⚠
    "acorn/util/walk": "resource://devtools/shared/acorn/walk.js",
    // ⚠ DISCUSSION ON DEV-DEVELOPER-TOOLS REQUIRED BEFORE MODIFYING ⚠
    // Allow access to xpcshell test items from the loader.
    "xpcshell-test": "resource://test",

    // ⚠ DISCUSSION ON DEV-DEVELOPER-TOOLS REQUIRED BEFORE MODIFYING ⚠
    // Allow access to locale data using paths closer to what is
    // used in the source tree.
    "devtools/client/locales": "chrome://devtools/locale",
    "devtools/shared/locales": "chrome://devtools-shared/locale",
    "devtools/startup/locales": "chrome://devtools-startup/locale",
    "toolkit/locales": "chrome://global/locale",
  };

  // When creating a Loader invisible to the Debugger, we have to ensure
  // using only modules and not depend on any JSM. As everything that is
  // not loaded with Loader isn't going to respect `invisibleToDebugger`.
  // But we have to keep using Promise.jsm for other loader to prevent
  // breaking unhandled promise rejection in tests.
  if (invisibleToDebugger) {
    paths.promise = "resource://gre/modules/Promise-backend.js";
  }

  this.loader = new Loader({
    paths,
    invisibleToDebugger,
    freshCompartment,
    sandboxName: "DevTools (Module loader)",
    // Make sure `define` function exists. JSON Viewer needs modules in AMD
    // format, as it currently uses RequireJS from a content document and
    // can't access our usual loaders. So, any modules shared with the JSON
    // Viewer should include a define wrapper:
    //
    //   // Make this available to both AMD and CJS environments
    //   define(function(require, exports, module) {
    //     ... code ...
    //   });
    //
    // Bug 1248830 will work out a better plan here for our content module
    // loading needs, especially as we head towards devtools.html.
    supportAMDModules: true,
    requireHook: (id, require) => {
      if (id.startsWith("raw!") || id.startsWith("theme-loader!")) {
        return requireRawId(id, require);
      }
      return require(id);
    },
  });

  this.require = Require(this.loader, { id: "devtools" });

  // Fetch custom pseudo modules and globals
  const { modules, globals } = this.require("devtools/shared/builtin-modules");

  // When creating a Loader for the browser toolbox, we have to use
  // Promise-backend.js, as a Loader module. Instead of Promise.jsm which
  // can't be flagged as invisible to debugger.
  if (invisibleToDebugger) {
    delete modules.promise;
  }

  // Register custom pseudo modules to the current loader instance
  for (const id in modules) {
    const uri = resolveURI(id, this.loader.mapping);
    this.loader.modules[uri] = {
      get exports() {
        return modules[id];
      },
    };
  }

  // Register custom globals to the current loader instance
  Object.defineProperties(
    this.loader.globals,
    Object.getOwnPropertyDescriptors(globals)
  );

  // Define the loader id for these two usecases:
  // * access via the JSM (this.id)
  // let { loader } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
  // loader.id
  this.id = gNextLoaderID++;
  // * access via module's `loader` global
  // loader.id
  globals.loader.id = this.id;

  // Expose lazy helpers on `loader`
  // ie. when you use it like that from a JSM:
  // let { loader } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
  // loader.lazyGetter(...);
  this.lazyGetter = globals.loader.lazyGetter;
  this.lazyImporter = globals.loader.lazyImporter;
  this.lazyServiceGetter = globals.loader.lazyServiceGetter;
  this.lazyRequireGetter = globals.loader.lazyRequireGetter;
}

DevToolsLoader.prototype = {
  destroy: function(reason = "shutdown") {
    unload(this.loader, reason);
    delete this.loader;
  },

  /**
   * Return true if |id| refers to something requiring help from a
   * loader plugin.
   */
  isLoaderPluginId: function(id) {
    return id.startsWith("raw!");
  },
};

// Export the standard instance of DevToolsLoader used by the tools.
var loader = new DevToolsLoader({
  /**
   * Sets whether the compartments loaded by this instance should be invisible
   * to the debugger.  Invisibility is needed for loaders that support debugging
   * of chrome code.  This is true of remote target environments, like Fennec or
   * B2G.  It is not the default case for desktop Firefox because we offer the
   * Browser Toolbox for chrome debugging there, which uses its own, separate
   * loader instance.
   * @see devtools/client/framework/browser-toolbox/Launcher.jsm
   */
  invisibleToDebugger: Services.appinfo.name !== "Firefox",
});

var require = loader.require;
