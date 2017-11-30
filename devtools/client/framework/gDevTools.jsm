/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This JSM is here to keep some compatibility with existing add-ons.
 * Please now use the modules:
 * - devtools/client/framework/devtools for gDevTools
 * - devtools/client/framework/devtools-browser for gDevToolsBrowser
 */

this.EXPORTED_SYMBOLS = [ "gDevTools", "gDevToolsBrowser" ];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

const { loader } = Cu.import("resource://devtools/shared/Loader.jsm", {});

/**
 * Do not directly map to the commonjs modules so that callsites of
 * gDevTools.jsm do not have to do anything to access to the very last version
 * of the module. The `devtools` and `browser` getter are always going to
 * retrieve the very last version of the modules.
 */
Object.defineProperty(this, "require", {
  get() {
    let { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});
    return require;
  }
});
Object.defineProperty(this, "devtools", {
  get() {
    return require("devtools/client/framework/devtools").gDevTools;
  }
});
Object.defineProperty(this, "browser", {
  get() {
    return require("devtools/client/framework/devtools-browser").gDevToolsBrowser;
  }
});

/**
 * gDevTools is a singleton that controls the Firefox Developer Tools.
 *
 * It is an instance of a DevTools class that holds a set of tools. It has the
 * same lifetime as the browser.
 */
let gDevToolsMethods = [
  // Used by: - b2g desktop.js
  //          - nsContextMenu
  //          - /devtools code
  "showToolbox",

  // Used by Addon SDK and /devtools
  "closeToolbox",
  "getToolbox",

  // Used by Addon SDK, main.js and tests:
  "registerTool",
  "registerTheme",
  "unregisterTool",
  "unregisterTheme",

  // Used by main.js and test
  "getToolDefinitionArray",
  "getThemeDefinitionArray",

  // Used by WebExtensions devtools API
  "getTheme",

  // Used by theme-switching.js
  "getThemeDefinition",
  "emit",

  // Used by /devtools
  "on",
  "off",
  "once",

  // Used by tests
  "getToolDefinitionMap",
  "getThemeDefinitionMap",
  "getDefaultTools",
  "getAdditionalTools",
  "getToolDefinition",
];
this.gDevTools = {
  // Used by tests
  get _toolboxes() {
    return devtools._toolboxes;
  },
  get _tools() {
    return devtools._tools;
  },
  *[Symbol.iterator ]() {
    for (let toolbox of this._toolboxes) {
      yield toolbox;
    }
  }
};
gDevToolsMethods.forEach(name => {
  this.gDevTools[name] = (...args) => {
    return devtools[name].apply(devtools, args);
  };
});


/**
 * gDevToolsBrowser exposes functions to connect the gDevTools instance with a
 * Firefox instance.
 */
let gDevToolsBrowserMethods = [
  // used by browser-sets.inc, command
  "toggleToolboxCommand",

  // Used by browser.js itself, by setting a oncommand string...
  "selectToolCommand",

  // Used by browser-sets.inc, command
  "openAboutDebugging",

  // Used by browser-sets.inc, command
  "openConnectScreen",

  // Used by browser-sets.inc, command
  //         itself, webide widget
  "openWebIDE",

  // Used by browser-sets.inc, command
  "openContentProcessToolbox",

  // Used by browser.js
  "registerBrowserWindow",

  // Used by devtools-browser.js for the Toggle Toolbox status
  "hasToolboxOpened",

  // Used by browser.js
  "forgetBrowserWindow"
];
this.gDevToolsBrowser = {
  // Used by webide.js
  get isWebIDEInitialized() {
    return browser.isWebIDEInitialized;
  },
  // Used by a test (should be removed)
  get _trackedBrowserWindows() {
    return browser._trackedBrowserWindows;
  }
};
gDevToolsBrowserMethods.forEach(name => {
  this.gDevToolsBrowser[name] = (...args) => {
    return browser[name].apply(browser, args);
  };
});
