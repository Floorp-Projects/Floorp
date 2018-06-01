/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Manages the base loader (base-loader.js) instance used to load the developer tools.
 */

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm", {});
var { Loader, Require, resolveURI, unload } =
  ChromeUtils.import("resource://devtools/shared/base-loader.js", {});
var { requireRawId } = ChromeUtils.import("resource://devtools/shared/loader-plugin-raw.jsm", {});

this.EXPORTED_SYMBOLS = ["DevToolsLoader", "devtools", "BuiltinProvider",
                         "require", "loader",
                         // Export StructuredCloneHolder for its use from builtin-modules
                         "StructuredCloneHolder"];

/**
 * Providers are different strategies for loading the devtools.
 */

/**
 * Used when the tools should be loaded from the Firefox package itself.
 * This is the default case.
 */
function BuiltinProvider() {}
BuiltinProvider.prototype = {
  load: function() {
    const paths = {
      // ⚠ DISCUSSION ON DEV-DEVELOPER-TOOLS REQUIRED BEFORE MODIFYING ⚠
      "devtools": "resource://devtools",
      // ⚠ DISCUSSION ON DEV-DEVELOPER-TOOLS REQUIRED BEFORE MODIFYING ⚠
      "gcli": "resource://devtools/shared/gcli/source/lib/gcli",
      // ⚠ DISCUSSION ON DEV-DEVELOPER-TOOLS REQUIRED BEFORE MODIFYING ⚠
      "acorn": "resource://devtools/shared/acorn",
      // ⚠ DISCUSSION ON DEV-DEVELOPER-TOOLS REQUIRED BEFORE MODIFYING ⚠
      "acorn/util/walk": "resource://devtools/shared/acorn/walk.js",
      // ⚠ DISCUSSION ON DEV-DEVELOPER-TOOLS REQUIRED BEFORE MODIFYING ⚠
      "source-map": "resource://devtools/shared/sourcemap/source-map.js",
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
    if (this.invisibleToDebugger) {
      paths.promise = "resource://gre/modules/Promise-backend.js";
    }
    this.loader = new Loader({
      paths,
      invisibleToDebugger: this.invisibleToDebugger,
      sharedGlobal: true,
      sandboxName: "DevTools (Module loader)",
      requireHook: (id, require) => {
        if (id.startsWith("raw!") || id.startsWith("theme-loader!")) {
          return requireRawId(id, require);
        }
        return require(id);
      },
    });
  },

  unload: function(reason) {
    unload(this.loader, reason);
    delete this.loader;
  },
};

var gNextLoaderID = 0;

/**
 * The main devtools API. The standard instance of this loader is exported as
 * |devtools| below, but if a fresh copy of the loader is needed, then a new
 * one can also be created.
 */
this.DevToolsLoader = function DevToolsLoader() {
  this.require = this.require.bind(this);

  Services.obs.addObserver(this, "devtools-unload");
};

DevToolsLoader.prototype = {
  destroy: function(reason = "shutdown") {
    Services.obs.removeObserver(this, "devtools-unload");

    if (this._provider) {
      this._provider.unload(reason);
      delete this._provider;
    }
  },

  get provider() {
    if (!this._provider) {
      this._loadProvider();
    }
    return this._provider;
  },

  _provider: null,

  get id() {
    if (this._id) {
      return this._id;
    }
    this._id = ++gNextLoaderID;
    return this._id;
  },

  /**
   * A dummy version of require, in case a provider hasn't been chosen yet when
   * this is first called.  This will then be replaced by the real version.
   * @see setProvider
   */
  require: function() {
    if (!this._provider) {
      this._loadProvider();
    }
    return this.require.apply(this, arguments);
  },

  /**
   * Return true if |id| refers to something requiring help from a
   * loader plugin.
   */
  isLoaderPluginId: function(id) {
    return id.startsWith("raw!");
  },

  /**
   * Override the provider used to load the tools.
   */
  setProvider: function(provider) {
    if (provider === this._provider) {
      return;
    }

    if (this._provider) {
      delete this.require;
      this._provider.unload("newprovider");
    }
    this._provider = provider;

    // Pass through internal loader settings specific to this loader instance
    this._provider.invisibleToDebugger = this.invisibleToDebugger;

    this._provider.load();
    this.require = Require(this._provider.loader, { id: "devtools" });

    // Fetch custom pseudo modules and globals
    const { modules, globals } = this.require("devtools/shared/builtin-modules");

    // When creating a Loader for the browser toolbox, we have to use
    // Promise-backend.js, as a Loader module. Instead of Promise.jsm which
    // can't be flagged as invisible to debugger.
    if (this.invisibleToDebugger) {
      delete modules.promise;
    }

    // Register custom pseudo modules to the current loader instance
    const loader = this._provider.loader;
    for (const id in modules) {
      const uri = resolveURI(id, loader.mapping);
      loader.modules[uri] = {
        get exports() {
          return modules[id];
        }
      };
    }

    // Register custom globals to the current loader instance
    globals.loader.id = this.id;
    Object.defineProperties(loader.globals, Object.getOwnPropertyDescriptors(globals));

    // Expose lazy helpers on loader
    this.lazyGetter = globals.loader.lazyGetter;
    this.lazyImporter = globals.loader.lazyImporter;
    this.lazyServiceGetter = globals.loader.lazyServiceGetter;
    this.lazyRequireGetter = globals.loader.lazyRequireGetter;
  },

  /**
   * Choose a default tools provider based on the preferences.
   */
  _loadProvider: function() {
    this.setProvider(new BuiltinProvider());
  },

  /**
   * Handles "devtools-unload" event
   *
   * @param String data
   *    reason passed to modules when unloaded
   */
  observe: function(subject, topic, data) {
    if (topic != "devtools-unload") {
      return;
    }
    this.destroy(data);
  },

  /**
   * Sets whether the compartments loaded by this instance should be invisible
   * to the debugger.  Invisibility is needed for loaders that support debugging
   * of chrome code.  This is true of remote target environments, like Fennec or
   * B2G.  It is not the default case for desktop Firefox because we offer the
   * Browser Toolbox for chrome debugging there, which uses its own, separate
   * loader instance.
   * @see devtools/client/framework/ToolboxProcess.jsm
   */
  invisibleToDebugger: Services.appinfo.name !== "Firefox"
};

// Export the standard instance of DevToolsLoader used by the tools.
this.devtools = this.loader = new DevToolsLoader();

this.require = this.devtools.require.bind(this.devtools);

// For compatibility reasons, expose these symbols on "devtools":
Object.defineProperty(this.devtools, "Toolbox", {
  get: () => this.require("devtools/client/framework/toolbox").Toolbox
});
Object.defineProperty(this.devtools, "TargetFactory", {
  get: () => this.require("devtools/client/framework/target").TargetFactory
});
