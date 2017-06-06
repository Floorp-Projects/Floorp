/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "DevToolsShim",
];

function removeItem(array, callback) {
  let index = array.findIndex(callback);
  if (index >= 0) {
    array.splice(index, 1);
  }
}

/**
 * The DevToolsShim is a part of the DevTools go faster project, which moves the Firefox
 * DevTools outside of mozilla-central to an add-on. It aims to bridge the gap for
 * existing mozilla-central code that still needs to interact with DevTools (such as
 * web-extensions).
 *
 * DevToolsShim is a singleton that provides a set of helpers to interact with DevTools,
 * that work whether the DevTools addon is installed or not. It can be used to start
 * listening to events, register tools, themes. As soon as a DevTools addon is installed
 * the DevToolsShim will forward all the requests received until then to the real DevTools
 * instance.
 *
 * DevToolsShim.isInstalled() can also be used to know if DevTools are currently
 * installed.
 */
this.DevToolsShim = {
  gDevTools: null,
  listeners: [],
  tools: [],
  themes: [],

  /**
   * Check if DevTools are currently installed and available.
   *
   * @return {Boolean} true if DevTools are installed.
   */
  isInstalled: function () {
    return !!this.gDevTools;
  },

  /**
   * Register an instance of gDevTools. Should be called by DevTools during startup.
   *
   * @param {DevTools} a devtools instance (from client/framework/devtools)
   */
  register: function (gDevTools) {
    this.gDevTools = gDevTools;
    this._onDevToolsRegistered();
    this.gDevTools.emit("devtools-registered");
  },

  /**
   * Unregister the current instance of gDevTools. Should be called by DevTools during
   * shutdown.
   */
  unregister: function () {
    if (this.isInstalled()) {
      this.gDevTools.emit("devtools-unregistered");
      this.gDevTools = null;
    }
  },

  /**
   * The following methods can be called before DevTools are installed:
   * - on
   * - off
   * - registerTool
   * - unregisterTool
   * - registerTheme
   * - unregisterTheme
   *
   * If DevTools are not installed when calling the method, DevToolsShim will call the
   * appropriate method as soon as a gDevTools instance is registered.
   */

  /**
   * This method is used by browser/components/extensions/ext-devtools.js for the events:
   * - toolbox-created
   * - toolbox-destroyed
   */
  on: function (event, listener) {
    if (this.isInstalled()) {
      this.gDevTools.on(event, listener);
    } else {
      this.listeners.push([event, listener]);
    }
  },

  /**
   * This method is currently only used by devtools code, but is kept here for consistency
   * with on().
   */
  off: function (event, listener) {
    if (this.isInstalled()) {
      this.gDevTools.off(event, listener);
    } else {
      removeItem(this.listeners, ([e, l]) => e === event && l === listener);
    }
  },

  /**
   * This method is only used by the addon-sdk and should be removed when Firefox 56 is
   * no longer supported.
   */
  registerTool: function (tool) {
    if (this.isInstalled()) {
      this.gDevTools.registerTool(tool);
    } else {
      this.tools.push(tool);
    }
  },

  /**
   * This method is only used by the addon-sdk and should be removed when Firefox 56 is
   * no longer supported.
   */
  unregisterTool: function (tool) {
    if (this.isInstalled()) {
      this.gDevTools.unregisterTool(tool);
    } else {
      removeItem(this.tools, t => t === tool);
    }
  },

  /**
   * This method is only used by the addon-sdk and should be removed when Firefox 56 is
   * no longer supported.
   */
  registerTheme: function (theme) {
    if (this.isInstalled()) {
      this.gDevTools.registerTheme(theme);
    } else {
      this.themes.push(theme);
    }
  },

  /**
   * This method is only used by the addon-sdk and should be removed when Firefox 56 is
   * no longer supported.
   */
  unregisterTheme: function (theme) {
    if (this.isInstalled()) {
      this.gDevTools.unregisterTheme(theme);
    } else {
      removeItem(this.themes, t => t === theme);
    }
  },

  /**
   * Called from SessionStore.jsm in mozilla-central when saving the current state.
   *
   * @return {Array} array of currently opened scratchpad windows. Empty array if devtools
   *         are not installed
   */
  getOpenedScratchpads: function () {
    if (!this.isInstalled()) {
      return [];
    }
    return this.gDevTools.getOpenedScratchpads();
  },

  /**
   * Called from SessionStore.jsm in mozilla-central when restoring a state that contained
   * opened scratchpad windows.
   */
  restoreScratchpadSession: function (scratchpads) {
    if (!this.isInstalled()) {
      return;
    }
    this.gDevTools.restoreScratchpadSession(scratchpads);
  },

  _onDevToolsRegistered: function () {
    // Register all pending event listeners on the real gDevTools object.
    for (let [event, listener] of this.listeners) {
      this.gDevTools.on(event, listener);
    }

    for (let tool of this.tools) {
      this.gDevTools.registerTool(tool);
    }

    for (let theme of this.themes) {
      this.gDevTools.registerTheme(theme);
    }

    this.listeners = [];
    this.tools = [];
    this.themes = [];
  },
};
