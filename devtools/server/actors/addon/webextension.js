/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Represents a WebExtension add-on in the parent process. This gives some metadata about
 * the add-on and watches for uninstall events. This uses a proxy to access the
 * WebExtension in the WebExtension process via the message manager.
 *
 * See devtools/docs/backend/actor-hierarchy.md for more details.
 */

const {DebuggerServer} = require("devtools/server/main");
const protocol = require("devtools/shared/protocol");
const {webExtensionSpec} = require("devtools/shared/specs/addon/webextension");

loader.lazyImporter(this, "AddonManager", "resource://gre/modules/AddonManager.jsm");
loader.lazyImporter(this, "ExtensionParent", "resource://gre/modules/ExtensionParent.jsm");

/**
 * Creates the actor that represents the addon in the parent process, which connects
 * itself to a WebExtensionTargetActor counterpart which is created in the extension
 * process (or in the main process if the WebExtensions OOP mode is disabled).
 *
 * The WebExtensionActor subscribes itself as an AddonListener on the AddonManager
 * and forwards this events to child actor (e.g. on addon reload or when the addon is
 * uninstalled completely) and connects to the child extension process using a `browser`
 * element provided by the extension internals (it is not related to any single extension,
 * but it will be created automatically to the currently selected "WebExtensions OOP mode"
 * and it persist across the extension reloads (it is destroyed once the actor exits).
 * WebExtensionActor is a child of RootActor, it can be retrieved via
 * RootActor.listAddons request.
 *
 * @param {DebuggerServerConnection} conn
 *        The connection to the client.
 * @param {AddonWrapper} addon
 *        The target addon.
 */
const WebExtensionActor = protocol.ActorClassWithSpec(webExtensionSpec, {
  initialize(conn, addon) {
    this.conn = conn;
    this.addon = addon;
    this.id = addon.id;
    this._childFormPromise = null;

    AddonManager.addAddonListener(this);
  },

  destroy() {
    AddonManager.removeAddonListener(this);

    this.addon = null;
    this._childFormPromise = null;

    if (this._destroyProxy) {
      this._destroyProxy();
      delete this._destroyProxy;
    }
  },

  setOptions() {
    // NOTE: not used anymore for webextensions, still used in the legacy addons,
    // addon manager is currently going to call it automatically on every addon.
  },

  reload() {
    return this.addon.reload().then(() => {
      return {};
    });
  },

  form() {
    const policy = ExtensionParent.WebExtensionPolicy.getByID(this.id);
    return {
      actor: this.actorID,
      id: this.id,
      name: this.addon.name,
      url: this.addon.sourceURI ? this.addon.sourceURI.spec : undefined,
      iconURL: this.addon.iconURL,
      debuggable: this.addon.isDebuggable,
      temporarilyInstalled: this.addon.temporarilyInstalled,
      type: this.addon.type,
      isWebExtension: this.addon.isWebExtension,
      isAPIExtension: this.addon.isAPIExtension,
      manifestURL: policy && policy.getURL("manifest.json"),
      warnings: ExtensionParent.DebugUtils.getExtensionManifestWarnings(this.id),
    };
  },

  connect() {
    if (this._childFormPormise) {
      return this._childFormPromise;
    }

    const proxy = new WebExtensionTargetActorProxy(this.conn, this);
    this._childFormPromise = proxy.connect().then(form => {
      // Merge into the child actor form, some addon metadata
      // (e.g. the addon name shown in the addon debugger window title).
      return Object.assign(form, {
        id: this.addon.id,
        name: this.addon.name,
        iconURL: this.addon.iconURL,
        // Set the isOOP attribute on the connected child actor form.
        isOOP: proxy.isOOP,
      });
    });
    this._destroyProxy = () => proxy.destroy();

    return this._childFormPromise;
  },

  // WebExtensionTargetActorProxy callbacks.

  onProxyDestroy() {
    // Invalidate the cached child actor and form Promise
    // if the child actor exits.
    this._childFormPromise = null;
    delete this._destroyProxy;
  },

  // AddonManagerListener callbacks.

  onInstalled(addon) {
    if (addon.id != this.id) {
      return;
    }

    // Update the AddonManager's addon object on reload/update.
    this.addon = addon;
  },

  onUninstalled(addon) {
    if (addon != this.addon) {
      return;
    }

    this.destroy();
  },
});

exports.WebExtensionActor = WebExtensionActor;

function WebExtensionTargetActorProxy(connection, parentActor) {
  this._conn = connection;
  this._parentActor = parentActor;
  this.addonId = parentActor.id;

  this._onChildExit = this._onChildExit.bind(this);

  this._form = null;
  this._browser = null;
  this._childActorID = null;
}

WebExtensionTargetActorProxy.prototype = {
  /**
   * Connect the webextension child actor.
   */
  async connect() {
    if (this._browser) {
      throw new Error("This actor is already connected to the extension process");
    }

    // Called when the debug browser element has been destroyed
    // (no actor is using it anymore to connect the child extension process).
    const onDestroy = this.destroy.bind(this);

    this._browser = await ExtensionParent.DebugUtils.getExtensionProcessBrowser(this);

    this._form = await DebuggerServer.connectToFrame(this._conn, this._browser, onDestroy,
                                                     {addonId: this.addonId});

    this._childActorID = this._form.actor;

    // Exit the proxy child actor if the child actor has been destroyed.
    this._mm.addMessageListener("debug:webext_child_exit", this._onChildExit);

    return this._form;
  },

  get isOOP() {
    return this._browser ? this._browser.isRemoteBrowser : undefined;
  },

  get _mm() {
    return this._browser && (
      this._browser.messageManager ||
      this._browser.frameLoader.messageManager);
  },

  destroy() {
    if (this._mm) {
      this._mm.removeMessageListener("debug:webext_child_exit", this._onChildExit);

      this._mm.sendAsyncMessage("debug:webext_parent_exit", {
        actor: this._childActorID,
      });

      ExtensionParent.DebugUtils.releaseExtensionProcessBrowser(this);
    }

    if (this._parentActor) {
      this._parentActor.onProxyDestroy();
    }

    this._parentActor = null;
    this._browser = null;
    this._childActorID = null;
    this._form = null;
  },

  /**
   * Handle the child actor exit.
   */
  _onChildExit(msg) {
    if (msg.json.actor !== this._childActorID) {
      return;
    }

    this.destroy();
  },
};
