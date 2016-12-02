/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var Cu = Components.utils;
const loaders = Cu.import("resource://gre/modules/commonjs/toolkit/loader.js", {});
const { devtools } = Cu.import("resource://devtools/shared/Loader.jsm", {});
const { joinURI } = devtools.require("devtools/shared/path");
const { assert } = devtools.require("devtools/shared/DevToolsUtils");
const Services = devtools.require("Services");
const { AppConstants } = devtools.require("resource://gre/modules/AppConstants.jsm");

const BROWSER_BASED_DIRS = [
  "resource://devtools/client/inspector/layout",
  "resource://devtools/client/jsonview",
  "resource://devtools/client/shared/vendor",
  "resource://devtools/client/shared/redux",
];

// Any directory that matches the following regular expression
// is also considered as browser based module directory.
// ('resource://devtools/client/.*/components/')
//
// An example:
// * `resource://devtools/client/inspector/components`
// * `resource://devtools/client/inspector/shared/components`
const browserBasedDirsRegExp =
  /^resource\:\/\/devtools\/client\/\S*\/components\//;

function clearCache() {
  Services.obs.notifyObservers(null, "startupcache-invalidate", null);
}

/*
 * Create a loader to be used in a browser environment. This evaluates
 * modules in their own environment, but sets window (the normal
 * global object) as the sandbox prototype, so when a variable is not
 * defined it checks `window` before throwing an error. This makes all
 * browser APIs available to modules by default, like a normal browser
 * environment, but modules are still evaluated in their own scope.
 *
 * Another very important feature of this loader is that it *only*
 * deals with modules loaded from under `baseURI`. Anything loaded
 * outside of that path will still be loaded from the devtools loader,
 * so all system modules are still shared and cached across instances.
 * An exception to this is anything under
 * `devtools/client/shared/{vendor/components}`, which is where shared libraries
 * and React components live that should be evaluated in a browser environment.
 *
 * @param string baseURI
 *        Base path to load modules from. If null or undefined, only
 *        the shared vendor/components modules are loaded with the browser
 *        loader.
 * @param Object window
 *        The window instance to evaluate modules within
 * @param Boolean useOnlyShared
 *        If true, ignores `baseURI` and only loads the shared
 *        BROWSER_BASED_DIRS via BrowserLoader.
 * @return Object
 *         An object with two properties:
 *         - loader: the Loader instance
 *         - require: a function to require modules with
 */
function BrowserLoader(options) {
  const browserLoaderBuilder = new BrowserLoaderBuilder(options);
  return {
    loader: browserLoaderBuilder.loader,
    require: browserLoaderBuilder.require
  };
}

/**
 * Private class used to build the Loader instance and require method returned
 * by BrowserLoader(baseURI, window).
 *
 * @param string baseURI
 *        Base path to load modules from.
 * @param Object window
 *        The window instance to evaluate modules within
 * @param Boolean useOnlyShared
 *        If true, ignores `baseURI` and only loads the shared
 *        BROWSER_BASED_DIRS via BrowserLoader.
 */
function BrowserLoaderBuilder({ baseURI, window, useOnlyShared }) {
  assert(!!baseURI !== !!useOnlyShared,
    "Cannot use both `baseURI` and `useOnlyShared`.");

  const loaderOptions = devtools.require("@loader/options");
  const dynamicPaths = {};
  const componentProxies = new Map();

  if (AppConstants.DEBUG || AppConstants.DEBUG_JS_MODULES) {
    dynamicPaths["devtools/client/shared/vendor/react"] =
      "resource://devtools/client/shared/vendor/react-dev";
  }

  const opts = {
    id: "browser-loader",
    sharedGlobal: true,
    sandboxPrototype: window,
    paths: Object.assign({}, dynamicPaths, loaderOptions.paths),
    invisibleToDebugger: loaderOptions.invisibleToDebugger,
    requireHook: (id, require) => {
      // If |id| requires special handling, simply defer to devtools
      // immediately.
      if (devtools.isLoaderPluginId(id)) {
        return devtools.require(id);
      }

      const uri = require.resolve(id);
      let isBrowserDir = BROWSER_BASED_DIRS.filter(dir => {
        return uri.startsWith(dir);
      }).length > 0;

      // If the URI doesn't match hardcoded paths try the regexp.
      if (!isBrowserDir) {
        isBrowserDir = uri.match(browserBasedDirsRegExp) != null;
      }

      if ((useOnlyShared || !uri.startsWith(baseURI)) && !isBrowserDir) {
        return devtools.require(uri);
      }

      return require(uri);
    },
    globals: {
      // Allow modules to use the window's console to ensure logs appear in a
      // tab toolbox, if one exists, instead of just the browser console.
      console: window.console,
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
      // Allow modules to use the DevToolsLoader lazy loading helpers.
      loader: {
        lazyGetter: devtools.lazyGetter,
        lazyImporter: devtools.lazyImporter,
        lazyServiceGetter: devtools.lazyServiceGetter,
        lazyRequireGetter: this.lazyRequireGetter.bind(this),
      },
    }
  };

  if (Services.prefs.getBoolPref("devtools.loader.hotreload")) {
    opts.loadModuleHook = (module, require) => {
      const { uri, exports } = module;

      if (exports.prototype &&
          exports.prototype.isReactComponent) {
        const { createProxy, getForceUpdate } =
              require("devtools/client/shared/vendor/react-proxy");
        const React = require("devtools/client/shared/vendor/react");

        if (!componentProxies.get(uri)) {
          const proxy = createProxy(exports);
          componentProxies.set(uri, proxy);
          module.exports = proxy.get();
        } else {
          const proxy = componentProxies.get(uri);
          const instances = proxy.update(exports);
          instances.forEach(getForceUpdate(React));
          module.exports = proxy.get();
        }
      }
      return exports;
    };
    const watcher = devtools.require("devtools/client/shared/devtools-file-watcher");
    let onFileChanged = (_, relativePath, path) => {
      this.hotReloadFile(componentProxies, "resource://devtools/" + relativePath);
    };
    watcher.on("file-changed", onFileChanged);
    window.addEventListener("unload", () => {
      watcher.off("file-changed", onFileChanged);
    });
  }

  const mainModule = loaders.Module(baseURI, joinURI(baseURI, "main.js"));
  this.loader = loaders.Loader(opts);
  this.require = loaders.Require(this.loader, mainModule);
}

BrowserLoaderBuilder.prototype = {
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
  lazyRequireGetter: function (obj, property, module, destructure) {
    devtools.lazyGetter(obj, property, () => {
      return destructure
          ? this.require(module)[property]
          : this.require(module || property);
    });
  },

  hotReloadFile: function (componentProxies, fileURI) {
    if (fileURI.match(/\.js$/)) {
      // Test for React proxy components
      const proxy = componentProxies.get(fileURI);
      if (proxy) {
        // Remove the old module and re-require the new one; the require
        // hook in the loader will take care of the rest
        delete this.loader.modules[fileURI];
        clearCache();
        this.require(fileURI);
      }
    }
  }
};

this.BrowserLoader = BrowserLoader;

this.EXPORTED_SYMBOLS = ["BrowserLoader"];
