/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const lazy = {};
XPCOMUtils.defineLazyGetter(lazy, "DevToolsStartup", () => {
  return Cc["@mozilla.org/devtools/startup-clh;1"].getService(
    Ci.nsICommandLineHandler
  ).wrappedJSObject;
});

// We don't want to spend time initializing the full loader here so we create
// our own lazy require.
XPCOMUtils.defineLazyGetter(lazy, "Telemetry", function() {
  const { require } = ChromeUtils.import(
    "resource://devtools/shared/loader/Loader.jsm"
  );
  // eslint-disable-next-line no-shadow
  const Telemetry = require("devtools/client/shared/telemetry");

  return Telemetry;
});

const DEVTOOLS_POLICY_DISABLED_PREF = "devtools.policy.disabled";

const EXPORTED_SYMBOLS = ["DevToolsShim"];

function removeItem(array, callback) {
  const index = array.findIndex(callback);
  if (index >= 0) {
    array.splice(index, 1);
  }
}

/**
 * DevToolsShim is a singleton that provides a set of helpers to interact with DevTools,
 * that work whether Devtools are enabled or not.
 *
 * It can be used to start listening to devtools events before DevTools are ready. As soon
 * as DevTools are ready, the DevToolsShim will forward all the requests received until
 * then to the real DevTools instance.
 */
const DevToolsShim = {
  _gDevTools: null,
  listeners: [],

  get telemetry() {
    if (!this._telemetry) {
      this._telemetry = new lazy.Telemetry();
      this._telemetry.setEventRecordingEnabled(true);
    }
    return this._telemetry;
  },

  /**
   * Returns true if DevTools are enabled. This now only depends on the policy.
   * TODO: Merge isEnabled and isDisabledByPolicy.
   */
  isEnabled() {
    return !this.isDisabledByPolicy();
  },

  /**
   * Returns true if the devtools are completely disabled and can not be enabled. All
   * entry points should return without throwing, initDevTools should never be called.
   */
  isDisabledByPolicy() {
    return Services.prefs.getBoolPref(DEVTOOLS_POLICY_DISABLED_PREF, false);
  },

  /**
   * Check if DevTools have already been initialized.
   *
   * @return {Boolean} true if DevTools are initialized.
   */
  isInitialized() {
    return !!this._gDevTools;
  },

  /**
   * Returns the array of the existing toolboxes. This method is part of the compatibility
   * layer for webextensions.
   *
   * @return {Array<Toolbox>}
   *   An array of toolboxes.
   */
  getToolboxes() {
    if (this.isInitialized()) {
      return this._gDevTools.getToolboxes();
    }

    return [];
  },

  /**
   * Register an instance of gDevTools. Should be called by DevTools during startup.
   *
   * @param {DevTools} a devtools instance (from client/framework/devtools)
   */
  register(gDevTools) {
    this._gDevTools = gDevTools;
    this._onDevToolsRegistered();
    this._gDevTools.emit("devtools-registered");
  },

  /**
   * Unregister the current instance of gDevTools. Should be called by DevTools during
   * shutdown.
   */
  unregister() {
    if (this.isInitialized()) {
      this._gDevTools.emit("devtools-unregistered");
      this._gDevTools = null;
    }
  },

  /**
   * The following methods can be called before DevTools are initialized:
   * - on
   * - off
   *
   * If DevTools are not initialized when calling the method, DevToolsShim will call the
   * appropriate method as soon as a gDevTools instance is registered.
   */

  /**
   * This method is used by browser/components/extensions/ext-devtools.js for the events:
   * - toolbox-created
   * - toolbox-destroyed
   */
  on(event, listener) {
    if (this.isInitialized()) {
      this._gDevTools.on(event, listener);
    } else {
      this.listeners.push([event, listener]);
    }
  },

  /**
   * This method is currently only used by devtools code, but is kept here for consistency
   * with on().
   */
  off(event, listener) {
    if (this.isInitialized()) {
      this._gDevTools.off(event, listener);
    } else {
      removeItem(this.listeners, ([e, l]) => e === event && l === listener);
    }
  },

  /**
   * Called from SessionStore.jsm in mozilla-central when saving the current state.
   *
   * @param {Object} state
   *                 A SessionStore state object that gets modified by reference
   */
  saveDevToolsSession(state) {
    if (!this.isInitialized()) {
      return;
    }

    this._gDevTools.saveDevToolsSession(state);
  },

  /**
   * Called from SessionStore.jsm in mozilla-central when restoring a previous session.
   * Will always be called, even if the session does not contain DevTools related items.
   */
  restoreDevToolsSession(session) {
    if (!this.isEnabled()) {
      return;
    }

    const { browserConsole, browserToolbox } = session;
    const hasDevToolsData = browserConsole || browserToolbox;
    if (!hasDevToolsData) {
      // Do not initialize DevTools unless there is DevTools specific data in the session.
      return;
    }

    this.initDevTools("SessionRestore");
    this._gDevTools.restoreDevToolsSession(session);
  },

  isDevToolsUser() {
    return lazy.DevToolsStartup.isDevToolsUser();
  },

  /**
   * Called from nsContextMenu.js in mozilla-central when using the Inspect Accessibility
   * context menu item.
   *
   * @param {XULTab} tab
   *        The browser tab on which inspect accessibility was used.
   * @param {ElementIdentifier} domReference
   *        Identifier generated by ContentDOMReference. It is a unique pair of
   *        BrowsingContext ID and a numeric ID.
   * @return {Promise} a promise that resolves when the accessible node is selected in the
   *         accessibility inspector or that resolves immediately if DevTools are not
   *         enabled.
   */
  inspectA11Y(tab, domReference) {
    if (!this.isEnabled()) {
      if (!this.isDisabledByPolicy()) {
        lazy.DevToolsStartup.openInstallPage("ContextMenu");
      }
      return Promise.resolve();
    }

    // Record the timing at which this event started in order to compute later in
    // gDevTools.showToolbox, the complete time it takes to open the toolbox.
    // i.e. especially take `DevToolsStartup.initDevTools` into account.
    const startTime = Cu.now();

    this.initDevTools("ContextMenu");

    return this._gDevTools.inspectA11Y(tab, domReference, startTime);
  },

  /**
   * Called from nsContextMenu.js in mozilla-central when using the Inspect Element
   * context menu item.
   *
   * @param {XULTab} tab
   *        The browser tab on which inspect node was used.
   * @param {ElementIdentifier} domReference
   *        Identifier generated by ContentDOMReference. It is a unique pair of
   *        BrowsingContext ID and a numeric ID.
   * @return {Promise} a promise that resolves when the node is selected in the inspector
   *         markup view or that resolves immediately if DevTools are not enabled.
   */
  inspectNode(tab, domReference) {
    if (!this.isEnabled()) {
      if (!this.isDisabledByPolicy()) {
        lazy.DevToolsStartup.openInstallPage("ContextMenu");
      }
      return Promise.resolve();
    }

    // Record the timing at which this event started in order to compute later in
    // gDevTools.showToolbox, the complete time it takes to open the toolbox.
    // i.e. especially take `DevToolsStartup.initDevTools` into account.
    const startTime = Cu.now();

    this.initDevTools("ContextMenu");

    return this._gDevTools.inspectNode(tab, domReference, startTime);
  },

  _onDevToolsRegistered() {
    // Register all pending event listeners on the real gDevTools object.
    for (const [event, listener] of this.listeners) {
      this._gDevTools.on(event, listener);
    }

    this.listeners = [];
  },

  /**
   * Initialize DevTools via DevToolsStartup if needed. This method throws if DevTools are
   * not enabled.. If the entry point is supposed to trigger the onboarding, call it
   * explicitly via DevToolsStartup.openInstallPage().
   *
   * @param {String} reason
   *        optional, if provided should be a valid entry point for DEVTOOLS_ENTRY_POINT
   *        in toolkit/components/telemetry/Histograms.json
   */
  initDevTools(reason) {
    if (!this.isEnabled()) {
      throw new Error("DevTools are not enabled and can not be initialized.");
    }

    if (reason) {
      const window = Services.wm.getMostRecentWindow("navigator:browser");

      this.telemetry.addEventProperty(
        window,
        "open",
        "tools",
        null,
        "shortcut",
        ""
      );
      this.telemetry.addEventProperty(
        window,
        "open",
        "tools",
        null,
        "entrypoint",
        reason
      );
    }

    if (!this.isInitialized()) {
      lazy.DevToolsStartup.initDevTools(reason);
    }
  },
};

/**
 * Compatibility layer for webextensions.
 *
 * Those methods are called only after a DevTools webextension was loaded in DevTools,
 * therefore DevTools should always be available when they are called.
 */
const webExtensionsMethods = [
  "createCommandsForTabForWebExtension",
  "getTheme",
  "openBrowserConsole",
];

/**
 * Compatibility layer for other third parties.
 */
const otherToolMethods = [
  // gDevTools.showToolboxForTab is used by wptrunner to start devtools
  // https://github.com/web-platform-tests/wpt
  // And also, Quick Actions on URL bar.
  "showToolboxForTab",
  // Used for Quick Actions on URL bar.
  "hasToolboxForTab",
  // Used for Quick Actions test.
  "getToolboxForTab",
];

for (const method of [...webExtensionsMethods, ...otherToolMethods]) {
  DevToolsShim[method] = function() {
    if (!this.isEnabled()) {
      throw new Error(
        "Could not call a DevToolsShim webextension method ('" +
          method +
          "'): DevTools are not initialized."
      );
    }

    this.initDevTools();
    return this._gDevTools[method].apply(this._gDevTools, arguments);
  };
}
