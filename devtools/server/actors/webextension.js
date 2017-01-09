/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci, Cu } = require("chrome");
const Services = require("Services");
const { ChromeActor } = require("./chrome");
const makeDebugger = require("./utils/make-debugger");

var DevToolsUtils = require("devtools/shared/DevToolsUtils");
var { assert } = DevToolsUtils;

loader.lazyRequireGetter(this, "mapURIToAddonID", "devtools/server/actors/utils/map-uri-to-addon-id");
loader.lazyRequireGetter(this, "unwrapDebuggerObjectGlobal", "devtools/server/actors/script", true);

loader.lazyImporter(this, "AddonManager", "resource://gre/modules/AddonManager.jsm");
loader.lazyImporter(this, "XPIProvider", "resource://gre/modules/addons/XPIProvider.jsm");

const FALLBACK_DOC_MESSAGE = "Your addon does not have any document opened yet.";

/**
 * Creates a TabActor for debugging all the contexts associated to a target WebExtensions
 * add-on.
 * Most of the implementation is inherited from ChromeActor (which inherits most of its
 * implementation from TabActor).
 * WebExtensionActor is a child of RootActor, it can be retrieved via
 * RootActor.listAddons request.
 * WebExtensionActor exposes all tab actors via its form() request, like TabActor.
 *
 * History lecture:
 * The add-on actors used to not inherit TabActor because of the different way the
 * add-on APIs where exposed to the add-on itself, and for this reason the Addon Debugger
 * has only a sub-set of the feature available in the Tab or in the Browser Toolbox.
 * In a WebExtensions add-on all the provided contexts (background and popup pages etc.),
 * besides the Content Scripts which run in the content process, hooked to an existent
 * tab, by creating a new WebExtensionActor which inherits from ChromeActor, we can
 * provide a full features Addon Toolbox (which is basically like a BrowserToolbox which
 * filters the visible sources and frames to the one that are related to the target
 * add-on).
 *
 * @param conn DebuggerServerConnection
 *        The connection to the client.
 * @param addon AddonWrapper
 *        The target addon.
 */
function WebExtensionActor(conn, addon) {
  ChromeActor.call(this, conn);

  this.id = addon.id;
  this.addon = addon;

  // Bind the _allowSource helper to this, it is used in the
  // TabActor to lazily create the TabSources instance.
  this._allowSource = this._allowSource.bind(this);

  // Set the consoleAPIListener filtering options
  // (retrieved and used in the related webconsole child actor).
  this.consoleAPIListenerOptions = {
    addonId: addon.id,
  };

  // This creates a Debugger instance for debugging all the add-on globals.
  this.makeDebugger = makeDebugger.bind(null, {
    findDebuggees: dbg => {
      return dbg.findAllGlobals().filter(this._shouldAddNewGlobalAsDebuggee);
    },
    shouldAddNewGlobalAsDebuggee: this._shouldAddNewGlobalAsDebuggee.bind(this),
  });

  // Discover the preferred debug global for the target addon
  this.preferredTargetWindow = null;
  this._findAddonPreferredTargetWindow();

  AddonManager.addAddonListener(this);
}
exports.WebExtensionActor = WebExtensionActor;

WebExtensionActor.prototype = Object.create(ChromeActor.prototype);

WebExtensionActor.prototype.actorPrefix = "webExtension";
WebExtensionActor.prototype.constructor = WebExtensionActor;

// NOTE: This is needed to catch in the webextension webconsole all the
// errors raised by the WebExtension internals that are not currently
// associated with any window.
WebExtensionActor.prototype.isRootActor = true;

WebExtensionActor.prototype.form = function () {
  assert(this.actorID, "addon should have an actorID.");

  let baseForm = ChromeActor.prototype.form.call(this);

  return Object.assign(baseForm, {
    actor: this.actorID,
    id: this.id,
    name: this.addon.name,
    url: this.addon.sourceURI ? this.addon.sourceURI.spec : undefined,
    iconURL: this.addon.iconURL,
    debuggable: this.addon.isDebuggable,
    temporarilyInstalled: this.addon.temporarilyInstalled,
    isWebExtension: this.addon.isWebExtension,
  });
};

WebExtensionActor.prototype._attach = function () {
  // NOTE: we need to be sure that `this.window` can return a
  // window before calling the ChromeActor.onAttach, or the TabActor
  // will not be subscribed to the child doc shell updates.

  // If a preferredTargetWindow exists, set it as the target for this actor
  // when the client request to attach this actor.
  if (this.preferredTargetWindow) {
    this._setWindow(this.preferredTargetWindow);
  } else {
    this._createFallbackWindow();
  }

  // Call ChromeActor's _attach to listen for any new/destroyed chrome docshell
  ChromeActor.prototype._attach.apply(this);
};

WebExtensionActor.prototype._detach = function () {
  this._destroyFallbackWindow();

  // Call ChromeActor's _detach to unsubscribe new/destroyed chrome docshell listeners.
  ChromeActor.prototype._detach.apply(this);
};

/**
 * Called when the actor is removed from the connection.
 */
WebExtensionActor.prototype.exit = function () {
  AddonManager.removeAddonListener(this);

  this.preferredTargetWindow = null;
  this.addon = null;
  this.id = null;

  return ChromeActor.prototype.exit.apply(this);
};

// Addon Specific Remote Debugging requestTypes and methods.

/**
 * Reloads the addon.
 */
WebExtensionActor.prototype.onReload = function () {
  return this.addon.reload()
    .then(() => {
      // send an empty response
      return {};
    });
};

/**
 * Set the preferred global for the add-on (called from the AddonManager).
 */
WebExtensionActor.prototype.setOptions = function (addonOptions) {
  if ("global" in addonOptions) {
    // Set the proposed debug global as the preferred target window
    // (the actor will eventually set it as the target once it is attached)
    this.preferredTargetWindow = addonOptions.global;
  }
};

// AddonManagerListener callbacks.

WebExtensionActor.prototype.onInstalled = function (addon) {
  if (addon.id != this.id) {
    return;
  }

  // Update the AddonManager's addon object on reload/update.
  this.addon = addon;
};

WebExtensionActor.prototype.onUninstalled = function (addon) {
  if (addon != this.addon) {
    return;
  }

  this.exit();
};

WebExtensionActor.prototype.onPropertyChanged = function (addon, changedPropNames) {
  if (addon != this.addon) {
    return;
  }

  // Refresh the preferred debug global on disabled/reloaded/upgraded addon.
  if (changedPropNames.includes("debugGlobal")) {
    this._findAddonPreferredTargetWindow();
  }
};

// Private helpers

WebExtensionActor.prototype._createFallbackWindow = function () {
  if (this.fallbackWindow) {
    // Skip if there is already an existent fallback window.
    return;
  }

  // Create an empty hidden window as a fallback (e.g. the background page could be
  // not defined for the target add-on or not yet when the actor instance has been
  // created).
  this.fallbackWebNav = Services.appShell.createWindowlessBrowser(true);
  this.fallbackWebNav.loadURI(
    `data:text/html;charset=utf-8,${FALLBACK_DOC_MESSAGE}`,
    0, null, null, null
  );

  this.fallbackDocShell = this.fallbackWebNav
    .QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIDocShell);

  Object.defineProperty(this, "docShell", {
    value: this.fallbackDocShell,
    configurable: true
  });

  // Save the reference to the fallback DOMWindow
  this.fallbackWindow = this.fallbackDocShell.QueryInterface(Ci.nsIInterfaceRequestor)
                                             .getInterface(Ci.nsIDOMWindow);
};

WebExtensionActor.prototype._destroyFallbackWindow = function () {
  if (this.fallbackWebNav) {
    // Explicitly close the fallback windowless browser to prevent it to leak
    // (and to prevent it to freeze devtools xpcshell tests).
    this.fallbackWebNav.loadURI("about:blank", 0, null, null, null);
    this.fallbackWebNav.close();

    this.fallbackWebNav = null;
    this.fallbackWindow = null;
  }
};

/**
 * Discover the preferred debug global and switch to it if the addon has been attached.
 */
WebExtensionActor.prototype._findAddonPreferredTargetWindow = function () {
  return new Promise(resolve => {
    let activeAddon = XPIProvider.activeAddons.get(this.id);

    if (!activeAddon) {
      // The addon is not active, the background page is going to be destroyed,
      // navigate to the fallback window (if it already exists).
      resolve(null);
    } else {
      AddonManager.getAddonByInstanceID(activeAddon.instanceID)
        .then(privateWrapper => {
          let targetWindow = privateWrapper.getDebugGlobal();

          // Do not use the preferred global if it is not a DOMWindow as expected.
          if (!(targetWindow instanceof Ci.nsIDOMWindow)) {
            targetWindow = null;
          }

          resolve(targetWindow);
        });
    }
  }).then(preferredTargetWindow => {
    this.preferredTargetWindow = preferredTargetWindow;

    if (!preferredTargetWindow) {
      // Create a fallback window if no preferred target window has been found.
      this._createFallbackWindow();
    } else if (this.attached) {
      // Change the top level document if the actor is already attached.
      this._changeTopLevelDocument(preferredTargetWindow);
    }
  });
};

/**
 * Return an array of the json details related to an array/iterator of docShells.
 */
WebExtensionActor.prototype._docShellsToWindows = function (docshells) {
  return ChromeActor.prototype._docShellsToWindows.call(this, docshells)
                    .filter(windowDetails => {
                      // filter the docShells based on the addon id
                      return windowDetails.addonID == this.id;
                    });
};

/**
 * Return true if the given source is associated with this addon and should be
 * added to the visible sources (retrieved and used by the webbrowser actor module).
 */
WebExtensionActor.prototype._allowSource = function (source) {
  try {
    let uri = Services.io.newURI(source.url);
    let addonID = mapURIToAddonID(uri);

    return addonID == this.id;
  } catch (e) {
    return false;
  }
};

/**
 * Return true if the given global is associated with this addon and should be
 * added as a debuggee, false otherwise.
 */
WebExtensionActor.prototype._shouldAddNewGlobalAsDebuggee = function (newGlobal) {
  const global = unwrapDebuggerObjectGlobal(newGlobal);

  if (global instanceof Ci.nsIDOMWindow) {
    return global.document.nodePrincipal.originAttributes.addonId == this.id;
  }

  try {
    // This will fail for non-Sandbox objects, hence the try-catch block.
    let metadata = Cu.getSandboxMetadata(global);
    if (metadata) {
      return metadata.addonID === this.id;
    }
  } catch (e) {
    // Unable to retrieve the sandbox metadata.
  }

  return false;
};

/**
 * Override WebExtensionActor requestTypes:
 * - redefined `reload`, which should reload the target addon
 *   (instead of the entire browser as the regular ChromeActor does).
 */
WebExtensionActor.prototype.requestTypes.reload = WebExtensionActor.prototype.onReload;
