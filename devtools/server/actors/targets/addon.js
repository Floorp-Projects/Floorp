/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Target actor for a legacy (non-WebExtension) add-on.
 *
 * See devtools/docs/backend/actor-hierarchy.md for more details.
 */

var { Ci, Cu } = require("chrome");
var Services = require("Services");
var { ActorPool } = require("devtools/server/actors/common");
var { TabSources } = require("devtools/server/actors/utils/TabSources");
var makeDebugger = require("devtools/server/actors/utils/make-debugger");
var DevToolsUtils = require("devtools/shared/DevToolsUtils");
var { assert } = DevToolsUtils;

loader.lazyRequireGetter(this, "AddonThreadActor", "devtools/server/actors/thread", true);
loader.lazyRequireGetter(this, "unwrapDebuggerObjectGlobal", "devtools/server/actors/thread", true);
loader.lazyRequireGetter(this, "AddonConsoleActor", "devtools/server/actors/addon/console", true);

loader.lazyImporter(this, "AddonManager", "resource://gre/modules/AddonManager.jsm");

function AddonTargetActor(connection, addon) {
  this.conn = connection;
  this._addon = addon;
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
exports.AddonTargetActor = AddonTargetActor;

AddonTargetActor.prototype = {
  actorPrefix: "addonTarget",

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

  form: function BAAForm() {
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
      type: this._addon.type,
      isWebExtension: this._addon.isWebExtension,
      isAPIExtension: this._addon.isAPIExtension,
      consoleActor: this._consoleActor.actorID,

      traits: {
        highlightable: false,
        networkMonitor: false,
      },
    };
  },

  destroy() {
    this.conn.removeActorPool(this._contextPool);
    this._contextPool = null;
    this._consoleActor = null;
    this._addon = null;
    this._global = null;
    AddonManager.removeAddonListener(this);
  },

  setOptions: function BAASetOptions(options) {
    if ("global" in options) {
      this._global = options.global;
    }
  },

  onInstalled: function BAAUpdateAddonWrapper(addon) {
    if (addon.id != this._addon.id) {
      return;
    }

    // Update the AddonManager's addon object on reload/update.
    this._addon = addon;
  },

  onDisabled: function BAAOnDisabled(addon) {
    if (addon != this._addon) {
      return;
    }

    this._global = null;
  },

  onUninstalled: function BAAOnUninstalled(addon) {
    if (addon != this._addon) {
      return;
    }

    if (this.attached) {
      this.onDetach();

      // The AddonTargetActor is not a BrowsingContextTargetActor and it has to send
      // "tabDetached" directly to close the devtools toolbox window.
      this.conn.send({ from: this.actorID, type: "tabDetached" });
    }

    this.destroy();
  },

  onAttach: function BAAOnAttach() {
    if (this.exited) {
      return { type: "exited" };
    }

    if (!this.attached) {
      this.threadActor = new AddonThreadActor(this.conn, this);
      this._contextPool.addActor(this.threadActor);
    }

    return { type: "tabAttached", threadActor: this.threadActor.actorID };
  },

  onDetach: function BAAOnDetach() {
    if (!this.attached) {
      return { error: "wrongState" };
    }

    this._contextPool.removeActor(this.threadActor);

    this.threadActor = null;
    this._sources = null;

    return { type: "detached" };
  },

  onReload: function BAAOnReload() {
    return this._addon.reload()
      .then(() => {
        // send an empty response
        return {};
      });
  },

  preNest: function() {
    const e = Services.wm.getEnumerator(null);
    while (e.hasMoreElements()) {
      const win = e.getNext();
      const windowUtils = win.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils);
      windowUtils.suppressEventHandling(true);
      windowUtils.suspendTimeouts();
    }
  },

  postNest: function() {
    const e = Services.wm.getEnumerator(null);
    while (e.hasMoreElements()) {
      const win = e.getNext();
      const windowUtils = win.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils);
      windowUtils.resumeTimeouts();
      windowUtils.suppressEventHandling(false);
    }
  },

  /**
   * Return true if the given global is associated with this addon and should be
   * added as a debuggee, false otherwise.
   */
  _shouldAddNewGlobalAsDebuggee: function(givenGlobal) {
    const global = unwrapDebuggerObjectGlobal(givenGlobal);
    try {
      // This will fail for non-Sandbox objects, hence the try-catch block.
      const metadata = Cu.getSandboxMetadata(global);
      if (metadata) {
        return metadata.addonID === this.id;
      }
    } catch (e) {
      // ignore
    }

    if (global instanceof Ci.nsIDOMWindow) {
      return global.document.nodePrincipal.addonId == this.id;
    }

    return false;
  },

  /**
   * Override the eligibility check for scripts and sources to make
   * sure every script and source with a URL is stored when debugging
   * add-ons.
   */
  _allowSource: function(source) {
    // XPIProvider.jsm evals some code in every add-on's bootstrap.js. Hide it.
    if (source.url === "resource://gre/modules/addons/XPIProvider.jsm") {
      return false;
    }

    return true;
  },

  /**
   * Yield the current set of globals associated with this addon that should be
   * added as debuggees.
   */
  _findDebuggees: function(dbg) {
    return dbg.findAllGlobals().filter(this._shouldAddNewGlobalAsDebuggee);
  }
};

AddonTargetActor.prototype.requestTypes = {
  "attach": AddonTargetActor.prototype.onAttach,
  "detach": AddonTargetActor.prototype.onDetach,
  "reload": AddonTargetActor.prototype.onReload
};

