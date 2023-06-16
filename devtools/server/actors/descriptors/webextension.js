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

const { Actor } = require("resource://devtools/shared/protocol.js");
const {
  webExtensionDescriptorSpec,
} = require("resource://devtools/shared/specs/descriptors/webextension.js");

const {
  connectToFrame,
} = require("resource://devtools/server/connectors/frame-connector.js");
const {
  createWebExtensionSessionContext,
} = require("resource://devtools/server/actors/watcher/session-context.js");

const lazy = {};
loader.lazyGetter(lazy, "AddonManager", () => {
  return ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
    { loadInDevToolsLoader: false }
  ).AddonManager;
});
loader.lazyGetter(lazy, "ExtensionParent", () => {
  return ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
    { loadInDevToolsLoader: false }
  ).ExtensionParent;
});
loader.lazyRequireGetter(
  this,
  "WatcherActor",
  "resource://devtools/server/actors/watcher.js",
  true
);

const BGSCRIPT_STATUSES = {
  RUNNING: "RUNNING",
  STOPPED: "STOPPED",
};

/**
 * Creates the actor that represents the addon in the parent process, which connects
 * itself to a WebExtensionTargetActor counterpart which is created in the extension
 * process (or in the main process if the WebExtensions OOP mode is disabled).
 *
 * The WebExtensionDescriptorActor subscribes itself as an AddonListener on the AddonManager
 * and forwards this events to child actor (e.g. on addon reload or when the addon is
 * uninstalled completely) and connects to the child extension process using a `browser`
 * element provided by the extension internals (it is not related to any single extension,
 * but it will be created automatically to the currently selected "WebExtensions OOP mode"
 * and it persist across the extension reloads (it is destroyed once the actor exits).
 * WebExtensionDescriptorActor is a child of RootActor, it can be retrieved via
 * RootActor.listAddons request.
 *
 * @param {DevToolsServerConnection} conn
 *        The connection to the client.
 * @param {AddonWrapper} addon
 *        The target addon.
 */
class WebExtensionDescriptorActor extends Actor {
  constructor(conn, addon) {
    super(conn, webExtensionDescriptorSpec);
    this.addon = addon;
    this.addonId = addon.id;
    this._childFormPromise = null;

    this._onChildExit = this._onChildExit.bind(this);
    this.destroy = this.destroy.bind(this);
    lazy.AddonManager.addAddonListener(this);
  }

  form() {
    const { addonId } = this;
    const policy = lazy.ExtensionParent.WebExtensionPolicy.getByID(addonId);
    const persistentBackgroundScript =
      lazy.ExtensionParent.DebugUtils.hasPersistentBackgroundScript(addonId);
    const backgroundScriptStatus = this._getBackgroundScriptStatus();

    return {
      actor: this.actorID,
      backgroundScriptStatus,
      // Note that until the policy becomes active,
      // getTarget/connectToFrame will fail attaching to the web extension:
      // https://searchfox.org/mozilla-central/rev/526a5089c61db85d4d43eb0e46edaf1f632e853a/toolkit/components/extensions/WebExtensionPolicy.cpp#551-553
      debuggable: policy?.active && this.addon.isDebuggable,
      hidden: this.addon.hidden,
      // iconDataURL is available after calling loadIconDataURL
      iconDataURL: this._iconDataURL,
      iconURL: this.addon.iconURL,
      id: addonId,
      isSystem: this.addon.isSystem,
      isWebExtension: this.addon.isWebExtension,
      manifestURL: policy && policy.getURL("manifest.json"),
      name: this.addon.name,
      persistentBackgroundScript,
      temporarilyInstalled: this.addon.temporarilyInstalled,
      traits: {
        supportsReloadDescriptor: true,
        // Supports the Watcher actor. Can be removed as part of Bug 1680280.
        watcher: true,
      },
      url: this.addon.sourceURI ? this.addon.sourceURI.spec : undefined,
      warnings: lazy.ExtensionParent.DebugUtils.getExtensionManifestWarnings(
        this.addonId
      ),
    };
  }

  /**
   * Return a Watcher actor, allowing to keep track of targets which
   * already exists or will be created. It also helps knowing when they
   * are destroyed.
   */
  async getWatcher(config = {}) {
    if (!this.watcher) {
      // Ensure connecting to the webextension frame in order to populate this._form
      await this._extensionFrameConnect();
      this.watcher = new WatcherActor(
        this.conn,
        createWebExtensionSessionContext(
          {
            addonId: this.addonId,
            browsingContextID: this._form.browsingContextID,
            innerWindowId: this._form.innerWindowId,
          },
          config
        )
      );
      this.manage(this.watcher);
    }
    return this.watcher;
  }

  async getTarget() {
    const form = await this._extensionFrameConnect();
    // Merge into the child actor form, some addon metadata
    // (e.g. the addon name shown in the addon debugger window title).
    return Object.assign(form, {
      iconURL: this.addon.iconURL,
      id: this.addon.id,
      name: this.addon.name,
    });
  }

  getChildren() {
    return [];
  }

  async _extensionFrameConnect() {
    if (this._form) {
      return this._form;
    }

    this._browser =
      await lazy.ExtensionParent.DebugUtils.getExtensionProcessBrowser(this);

    const policy = lazy.ExtensionParent.WebExtensionPolicy.getByID(
      this.addonId
    );
    this._form = await connectToFrame(this.conn, this._browser, this.destroy, {
      addonId: this.addonId,
      addonBrowsingContextGroupId: policy.browsingContextGroupId,
      // Bug 1754452: This flag is passed by the client to getWatcher(), but the server
      // doesn't support this anyway. So always pass false here and keep things simple.
      // Once we enable this flag, we will stop using connectToFrame and instantiate
      // the WebExtensionTargetActor from watcher code instead, so that shouldn't
      // introduce an issue for the future.
      isServerTargetSwitchingEnabled: false,
    });

    // connectToFrame may resolve to a null form,
    // in case the browser element is destroyed before it is fully connected to it.
    if (!this._form) {
      throw new Error(
        "browser element destroyed while connecting to it: " + this.addon.name
      );
    }

    this._childActorID = this._form.actor;

    // Exit the proxy child actor if the child actor has been destroyed.
    this._mm.addMessageListener("debug:webext_child_exit", this._onChildExit);

    return this._form;
  }

  /**
   * Note that reloadDescriptor is the common API name for descriptors
   * which support to be reloaded, while WebExtensionDescriptorActor::reload
   * is a legacy API which is for instance used from web-ext.
   *
   * bypassCache has no impact for addon reloads.
   */
  reloadDescriptor({ bypassCache }) {
    return this.reload();
  }

  async reload() {
    await this.addon.reload();
    return {};
  }

  async terminateBackgroundScript() {
    await lazy.ExtensionParent.DebugUtils.terminateBackgroundScript(
      this.addonId
    );
  }

  // This function will be called from RootActor in case that the devtools client
  // retrieves list of addons with `iconDataURL` option.
  async loadIconDataURL() {
    this._iconDataURL = await this.getIconDataURL();
  }

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
      console.warn(`Failed to create data url from [${this.addon.iconURL}]`);
      return null;
    }
  }

  // Private Methods
  _getBackgroundScriptStatus() {
    const isRunning = lazy.ExtensionParent.DebugUtils.isBackgroundScriptRunning(
      this.addonId
    );
    // The background script status doesn't apply to this addon (e.g. the addon
    // type doesn't have any code, like staticthemes/langpacks/dictionaries, or
    // the extension does not have a background script at all).
    if (isRunning === undefined) {
      return undefined;
    }

    return isRunning ? BGSCRIPT_STATUSES.RUNNING : BGSCRIPT_STATUSES.STOPPED;
  }

  get _mm() {
    return (
      this._browser &&
      (this._browser.messageManager || this._browser.frameLoader.messageManager)
    );
  }

  /**
   * Handle the child actor exit.
   */
  _onChildExit(msg) {
    if (msg.json.actor !== this._childActorID) {
      return;
    }

    this.destroy();
  }

  // AddonManagerListener callbacks.
  onInstalled(addon) {
    if (addon.id != this.addonId) {
      return;
    }

    // Update the AddonManager's addon object on reload/update.
    this.addon = addon;
  }

  onUninstalled(addon) {
    if (addon != this.addon) {
      return;
    }

    this.destroy();
  }

  destroy() {
    lazy.AddonManager.removeAddonListener(this);

    this.addon = null;
    if (this._mm) {
      this._mm.removeMessageListener(
        "debug:webext_child_exit",
        this._onChildExit
      );

      this._mm.sendAsyncMessage("debug:webext_parent_exit", {
        actor: this._childActorID,
      });

      lazy.ExtensionParent.DebugUtils.releaseExtensionProcessBrowser(this);
    }

    this._browser = null;
    this._childActorID = null;

    this.emit("descriptor-destroyed");

    super.destroy();
  }
}

exports.WebExtensionDescriptorActor = WebExtensionDescriptorActor;
