/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { Ci, Cu } = require("chrome");
var Services = require("Services");
var { ActorPool } = require("devtools/server/actors/common");
var { TabSources } = require("./utils/TabSources");
var makeDebugger = require("./utils/make-debugger");
var { ConsoleAPIListener } = require("devtools/shared/webconsole/utils");
var DevToolsUtils = require("devtools/shared/DevToolsUtils");
var { assert, update } = DevToolsUtils;

loader.lazyRequireGetter(this, "AddonThreadActor", "devtools/server/actors/script", true);
loader.lazyRequireGetter(this, "unwrapDebuggerObjectGlobal", "devtools/server/actors/script", true);
loader.lazyRequireGetter(this, "mapURIToAddonID", "devtools/server/actors/utils/map-uri-to-addon-id");
loader.lazyRequireGetter(this, "WebConsoleActor", "devtools/server/actors/webconsole", true);

loader.lazyImporter(this, "AddonManager", "resource://gre/modules/AddonManager.jsm");

function BrowserAddonActor(aConnection, aAddon) {
  this.conn = aConnection;
  this._addon = aAddon;
  this._contextPool = new ActorPool(this.conn);
  this.conn.addActorPool(this._contextPool);
  this.threadActor = null;
  this._global = null;

  this._shouldAddNewGlobalAsDebuggee = this._shouldAddNewGlobalAsDebuggee.bind(this);

  this.makeDebugger = makeDebugger.bind(null, {
    findDebuggees: this._findDebuggees.bind(this),
    shouldAddNewGlobalAsDebuggee: this._shouldAddNewGlobalAsDebuggee
  });

  AddonManager.addAddonListener(this);
}
exports.BrowserAddonActor = BrowserAddonActor;

BrowserAddonActor.prototype = {
  actorPrefix: "addon",

  get exited() {
    return !this._addon;
  },

  get id() {
    return this._addon.id;
  },

  get url() {
    return this._addon.sourceURI ? this._addon.sourceURI.spec : undefined;
  },

  get attached() {
    return this.threadActor;
  },

  get global() {
    return this._global;
  },

  get sources() {
    if (!this._sources) {
      assert(this.threadActor, "threadActor should exist when creating sources.");
      this._sources = new TabSources(this.threadActor, this._allowSource);
    }
    return this._sources;
  },


  form: function BAA_form() {
    assert(this.actorID, "addon should have an actorID.");
    if (!this._consoleActor) {
      this._consoleActor = new AddonConsoleActor(this._addon, this.conn, this);
      this._contextPool.addActor(this._consoleActor);
    }

    return {
      actor: this.actorID,
      id: this.id,
      name: this._addon.name,
      url: this.url,
      iconURL: this._addon.iconURL,
      debuggable: this._addon.isDebuggable,
      temporarilyInstalled: this._addon.temporarilyInstalled,
      consoleActor: this._consoleActor.actorID,

      traits: {
        highlightable: false,
        networkMonitor: false,
      },
    };
  },

  disconnect: function BAA_disconnect() {
    this.conn.removeActorPool(this._contextPool);
    this._contextPool = null;
    this._consoleActor = null;
    this._addon = null;
    this._global = null;
    AddonManager.removeAddonListener(this);
  },

  setOptions: function BAA_setOptions(aOptions) {
    if ("global" in aOptions) {
      this._global = aOptions.global;
    }
  },

  onInstalled: function BAA_updateAddonWrapper(aAddon) {
    if (aAddon.id != this._addon.id) {
      return;
    }

    // Update the AddonManager's addon object on reload/update.
    this._addon = aAddon;
  },

  onDisabled: function BAA_onDisabled(aAddon) {
    if (aAddon != this._addon) {
      return;
    }

    this._global = null;
  },

  onUninstalled: function BAA_onUninstalled(aAddon) {
    if (aAddon != this._addon) {
      return;
    }

    if (this.attached) {
      this.onDetach();

      // The BrowserAddonActor is not a TabActor and it has to send
      // "tabDetached" directly to close the devtools toolbox window.
      this.conn.send({ from: this.actorID, type: "tabDetached" });
    }

    this.disconnect();
  },

  onAttach: function BAA_onAttach() {
    if (this.exited) {
      return { type: "exited" };
    }

    if (!this.attached) {
      this.threadActor = new AddonThreadActor(this.conn, this);
      this._contextPool.addActor(this.threadActor);
    }

    return { type: "tabAttached", threadActor: this.threadActor.actorID };
  },

  onDetach: function BAA_onDetach() {
    if (!this.attached) {
      return { error: "wrongState" };
    }

    this._contextPool.removeActor(this.threadActor);

    this.threadActor = null;
    this._sources = null;

    return { type: "detached" };
  },

  onReload: function BAA_onReload() {
    return this._addon.reload()
      .then(() => {
        return {}; // send an empty response
      });
  },

  preNest: function () {
    let e = Services.wm.getEnumerator(null);
    while (e.hasMoreElements()) {
      let win = e.getNext();
      let windowUtils = win.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils);
      windowUtils.suppressEventHandling(true);
      windowUtils.suspendTimeouts();
    }
  },

  postNest: function () {
    let e = Services.wm.getEnumerator(null);
    while (e.hasMoreElements()) {
      let win = e.getNext();
      let windowUtils = win.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils);
      windowUtils.resumeTimeouts();
      windowUtils.suppressEventHandling(false);
    }
  },

  /**
   * Return true if the given global is associated with this addon and should be
   * added as a debuggee, false otherwise.
   */
  _shouldAddNewGlobalAsDebuggee: function (aGlobal) {
    const global = unwrapDebuggerObjectGlobal(aGlobal);
    try {
      // This will fail for non-Sandbox objects, hence the try-catch block.
      let metadata = Cu.getSandboxMetadata(global);
      if (metadata) {
        return metadata.addonID === this.id;
      }
    } catch (e) {}

    if (global instanceof Ci.nsIDOMWindow) {
      return mapURIToAddonID(global.document.documentURIObject) == this.id;
    }

    // Check the global for a __URI__ property and then try to map that to an
    // add-on
    let uridescriptor = aGlobal.getOwnPropertyDescriptor("__URI__");
    if (uridescriptor && "value" in uridescriptor && uridescriptor.value) {
      let uri;
      try {
        uri = Services.io.newURI(uridescriptor.value, null, null);
      }
      catch (e) {
        DevToolsUtils.reportException(
          "BrowserAddonActor.prototype._shouldAddNewGlobalAsDebuggee",
          new Error("Invalid URI: " + uridescriptor.value)
        );
        return false;
      }

      if (mapURIToAddonID(uri) == this.id) {
        return true;
      }
    }

    return false;
  },

  /**
   * Override the eligibility check for scripts and sources to make
   * sure every script and source with a URL is stored when debugging
   * add-ons.
   */
  _allowSource: function (aSource) {
    // XPIProvider.jsm evals some code in every add-on's bootstrap.js. Hide it.
    if (aSource.url === "resource://gre/modules/addons/XPIProvider.jsm") {
      return false;
    }

    return true;
  },

  /**
   * Yield the current set of globals associated with this addon that should be
   * added as debuggees.
   */
  _findDebuggees: function (dbg) {
    return dbg.findAllGlobals().filter(this._shouldAddNewGlobalAsDebuggee);
  }
};

BrowserAddonActor.prototype.requestTypes = {
  "attach": BrowserAddonActor.prototype.onAttach,
  "detach": BrowserAddonActor.prototype.onDetach,
  "reload": BrowserAddonActor.prototype.onReload
};

/**
 * The AddonConsoleActor implements capabilities needed for the add-on web
 * console feature.
 *
 * @constructor
 * @param object aAddon
 *        The add-on that this console watches.
 * @param object aConnection
 *        The connection to the client, DebuggerServerConnection.
 * @param object aParentActor
 *        The parent BrowserAddonActor actor.
 */
function AddonConsoleActor(aAddon, aConnection, aParentActor)
{
  this.addon = aAddon;
  WebConsoleActor.call(this, aConnection, aParentActor);
}

AddonConsoleActor.prototype = Object.create(WebConsoleActor.prototype);

update(AddonConsoleActor.prototype, {
  constructor: AddonConsoleActor,

  actorPrefix: "addonConsole",

  /**
   * The add-on that this console watches.
   */
  addon: null,

  /**
   * The main add-on JS global
   */
  get window() {
    return this.parentActor.global;
  },

  /**
   * Destroy the current AddonConsoleActor instance.
   */
  disconnect: function ACA_disconnect()
  {
    WebConsoleActor.prototype.disconnect.call(this);
    this.addon = null;
  },

  /**
   * Handler for the "startListeners" request.
   *
   * @param object aRequest
   *        The JSON request object received from the Web Console client.
   * @return object
   *         The response object which holds the startedListeners array.
   */
  onStartListeners: function ACA_onStartListeners(aRequest)
  {
    let startedListeners = [];

    while (aRequest.listeners.length > 0) {
      let listener = aRequest.listeners.shift();
      switch (listener) {
        case "ConsoleAPI":
          if (!this.consoleAPIListener) {
            this.consoleAPIListener =
              new ConsoleAPIListener(null, this, { addonId: this.addon.id });
            this.consoleAPIListener.init();
          }
          startedListeners.push(listener);
          break;
      }
    }
    return {
      startedListeners: startedListeners,
      nativeConsoleAPI: true,
      traits: this.traits,
    };
  },
});

AddonConsoleActor.prototype.requestTypes = Object.create(WebConsoleActor.prototype.requestTypes);
AddonConsoleActor.prototype.requestTypes.startListeners = AddonConsoleActor.prototype.onStartListeners;
