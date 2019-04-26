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

const protocol = require("devtools/shared/protocol");
const { webExtensionSpec } = require("devtools/shared/specs/addon/webextension");
const { WebExtensionTargetActorProxy } = require("devtools/server/actors/targets/webextension-proxy");

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
    this.addonId = addon.id;
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

  reload() {
    return this.addon.reload().then(() => {
      return {};
    });
  },

  form() {
    const policy = ExtensionParent.WebExtensionPolicy.getByID(this.addonId);
    return {
      actor: this.actorID,
      debuggable: this.addon.isDebuggable,
      hidden: this.addon.hidden,
      // iconDataURL is available after calling loadIconDataURL
      iconDataURL: this._iconDataURL,
      iconURL: this.addon.iconURL,
      id: this.addonId,
      isAPIExtension: this.addon.isAPIExtension,
      isSystem: this.addon.isSystem,
      isWebExtension: this.addon.isWebExtension,
      manifestURL: policy && policy.getURL("manifest.json"),
      name: this.addon.name,
      temporarilyInstalled: this.addon.temporarilyInstalled,
      type: this.addon.type,
      url: this.addon.sourceURI ? this.addon.sourceURI.spec : undefined,
      warnings: ExtensionParent.DebugUtils.getExtensionManifestWarnings(this.addonId),
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
        iconURL: this.addon.iconURL,
        id: this.addon.id,
        // Set the isOOP attribute on the connected child actor form.
        isOOP: proxy.isOOP,
        name: this.addon.name,
      });
    });
    this._destroyProxy = () => proxy.destroy();

    return this._childFormPromise;
  },

  // This function will be called from RootActor in case that the debugger client
  // retrieves list of addons with `iconDataURL` option.
  async loadIconDataURL() {
    this._iconDataURL = await this.getIconDataURL();
  },

  async getIconDataURL() {
    if (!this.addon.iconURL) {
      return null;
    }

    const xhr = new XMLHttpRequest();
    xhr.responseType = "blob";
    xhr.open("GET", this.addon.iconURL, true);

    if (this.addon.iconURL.toLowerCase().endsWith(".svg")) {
      // Maybe SVG, thus force to change mime type.
      xhr.overrideMimeType("image/svg+xml");
    }

    try {
      const blob = await new Promise((resolve, reject) => {
        xhr.onload = () => resolve(xhr.response);
        xhr.onerror = reject;
        xhr.send();
      });

      const reader = new FileReader();
      return await new Promise((resolve, reject) => {
        reader.onloadend = () => resolve(reader.result);
        reader.onerror = reject;
        reader.readAsDataURL(blob);
      });
    } catch (_) {
      console.warn(`Failed to create data url from [${ this.addon.iconURL }]`);
      return null;
    }
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
    if (addon.id != this.addonId) {
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
