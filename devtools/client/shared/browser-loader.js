/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var { classes: Cc, interfaces: Ci, utils: Cu } = Components;

const loaders = Cu.import("resource://gre/modules/commonjs/toolkit/loader.js", {});
const { devtools } = Cu.import("resource://devtools/shared/Loader.jsm", {});
const { joinURI } = devtools.require("devtools/shared/path");
const { Services } = devtools.require("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AppConstants.jsm");

const BROWSER_BASED_DIRS = [
  "resource://devtools/client/jsonview",
  "resource://devtools/client/shared/vendor",
  "resource://devtools/client/shared/components",
  "resource://devtools/client/shared/redux"
];

function clearCache() {
  Services.obs.notifyObservers(null, "startupcache-invalidate", null);
}

function hotReloadFile(window, require, loader, componentProxies, fileURI) {
  dump("Hot reloading: " + fileURI + "\n");

  if (fileURI.match(/\.js$/)) {
    // Test for React proxy components
    const proxy = componentProxies.get(fileURI);
    if (proxy) {
      // Remove the old module and re-require the new one; the require
      // hook in the loader will take care of the rest
      delete loader.modules[fileURI];
      clearCache();
      require(fileURI);
    }
  } else if (fileURI.match(/\.css$/)) {
    const links = [...window.document.getElementsByTagNameNS("http://www.w3.org/1999/xhtml", "link")];
    links.forEach(link => {
      if (link.href.indexOf(fileURI) === 0) {
        const parentNode = link.parentNode;
        const newLink = window.document.createElementNS("http://www.w3.org/1999/xhtml", "link");
        newLink.rel = "stylesheet";
        newLink.type = "text/css";
        newLink.href = fileURI + "?s=" + Math.random();

        parentNode.insertBefore(newLink, link);
        parentNode.removeChild(link);
      }
    });
  }
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
 *        Base path to load modules from.
 * @param Object window
 *        The window instance to evaluate modules within
 * @return Object
 *         An object with two properties:
 *         - loader: the Loader instance
 *         - require: a function to require modules with
 */
function BrowserLoader(baseURI, window) {
  const loaderOptions = devtools.require("@loader/options");
  const dynamicPaths = {};
  const componentProxies = new Map();
  const hotReloadEnabled = Services.prefs.getBoolPref("devtools.loader.hotreload");

  if(AppConstants.DEBUG || AppConstants.DEBUG_JS_MODULES) {
    dynamicPaths["devtools/client/shared/vendor/react"] =
      "resource://devtools/client/shared/vendor/react-dev";
  };

  const opts = {
    id: "browser-loader",
    sharedGlobal: true,
    sandboxPrototype: window,
    paths: Object.assign({}, dynamicPaths, loaderOptions.paths),
    invisibleToDebugger: loaderOptions.invisibleToDebugger,
    requireHook: (id, require) => {
      const uri = require.resolve(id);
      const isBrowserDir = BROWSER_BASED_DIRS.filter(dir => {
        return uri.startsWith(dir);
      }).length > 0;

      if (!uri.startsWith(baseURI) && !isBrowserDir) {
        return devtools.require(uri);
      }

      return require(uri);
    },
    globals: {
      // Allow modules to use the window's console to ensure logs appear in a
      // tab toolbox, if one exists, instead of just the browser console.
      console: window.console,
      // Make sure 'define' function exists. This allows reusing AMD modules.
      define: function(callback) {
        callback(this.require, this.exports, this.module);
        return this.exports;
      }
    }
  };

  if(hotReloadEnabled) {
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
        }
        else {
          const proxy = componentProxies.get(uri);
          const instances = proxy.update(exports);
          instances.forEach(getForceUpdate(React));
          module.exports = proxy.get();
        }
      }
      return exports;
    }
  }


  const mainModule = loaders.Module(baseURI, joinURI(baseURI, "main.js"));
  const mainLoader = loaders.Loader(opts);
  const require = loaders.Require(mainLoader, mainModule);

  if (hotReloadEnabled) {
    const watcher = devtools.require("devtools/client/shared/file-watcher");
    function onFileChanged(_, fileURI) {
      hotReloadFile(window, require, mainLoader, componentProxies, fileURI);
    }
    watcher.on("file-changed", onFileChanged);

    window.addEventListener("unload", () => {
      watcher.off("file-changed", onFileChanged);
    });
  }

  return {
    loader: mainLoader,
    require: require
  };
}

this.BrowserLoader = BrowserLoader;

this.EXPORTED_SYMBOLS = ["BrowserLoader"];
