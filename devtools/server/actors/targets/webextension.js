/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Target actor for a WebExtension add-on.
 *
 * This actor extends ParentProcessTargetActor.
 *
 * See devtools/docs/backend/actor-hierarchy.md for more details.
 */

const { extend } = require("devtools/shared/extend");
const { Ci, Cu, Cc } = require("chrome");
const Services = require("Services");

const {
  ParentProcessTargetActor,
  parentProcessTargetPrototype,
} = require("devtools/server/actors/targets/parent-process");
const makeDebugger = require("devtools/server/actors/utils/make-debugger");
const {
  webExtensionTargetSpec,
} = require("devtools/shared/specs/targets/webextension");

const Targets = require("devtools/server/actors/targets/index");
const TargetActorMixin = require("devtools/server/actors/targets/target-actor-mixin");
const {
  getChildDocShells,
} = require("devtools/server/actors/targets/window-global");

loader.lazyRequireGetter(
  this,
  "unwrapDebuggerObjectGlobal",
  "devtools/server/actors/thread",
  true
);
const FALLBACK_DOC_URL =
  "chrome://devtools/content/shared/webextension-fallback.html";

/**
 * Protocol.js expects only the prototype object, and does not maintain the prototype
 * chain when it constructs the ActorClass. For this reason we are using `extend` to
 * maintain the properties of WindowGlobalTargetActor.prototype
 */
const webExtensionTargetPrototype = extend({}, parentProcessTargetPrototype);

/**
 * Creates a target actor for debugging all the contexts associated to a target
 * WebExtensions add-on running in a child extension process. Most of the implementation
 * is inherited from ParentProcessTargetActor (which inherits most of its implementation
 * from WindowGlobalTargetActor).
 *
 * WebExtensionTargetActor is created by a WebExtensionActor counterpart, when its
 * parent actor's `connect` method has been called (on the listAddons RDP package),
 * it runs in the same process that the extension is running into (which can be the main
 * process if the extension is running in non-oop mode, or the child extension process
 * if the extension is running in oop-mode).
 *
 * A WebExtensionTargetActor contains all target-scoped actors, like a regular
 * ParentProcessTargetActor or WindowGlobalTargetActor.
 *
 * History lecture:
 * - The add-on actors used to not inherit WindowGlobalTargetActor because of the
 *   different way the add-on APIs where exposed to the add-on itself, and for this reason
 *   the Addon Debugger has only a sub-set of the feature available in the Tab or in the
 *   Browser Toolbox.
 * - In a WebExtensions add-on all the provided contexts (background, popups etc.),
 *   besides the Content Scripts which run in the content process, hooked to an existent
 *   tab, by creating a new WebExtensionActor which inherits from
 *   ParentProcessTargetActor, we can provide a full features Addon Toolbox (which is
 *   basically like a BrowserToolbox which filters the visible sources and frames to the
 *   one that are related to the target add-on).
 * - When the WebExtensions OOP mode has been introduced, this actor has been refactored
 *   and moved from the main process to the new child extension process.
 *
 * @param {DevToolsServerConnection} conn
 *        The connection to the client.
 * @param {nsIMessageSender} chromeGlobal.
 *        The chromeGlobal where this actor has been injected by the
 *        frame-connector.js connectToFrame method.
 * @param {Object} options
 *        - addonId: {String} the addonId of the target WebExtension.
 *        - addonBrowsingContextGroupId: {String} the BrowsingContextGroupId used by this addon.
 *        - chromeGlobal: {nsIMessageSender} The chromeGlobal where this actor
 *          has been injected by the frame-connector.js connectToFrame method.
 *        - isTopLevelTarget: {Boolean} flag to indicate if this is the top
 *          level target of the DevTools session
 *        - prefix: {String} the custom RDP prefix to use.
 *        - sessionContext Object
 *          The Session Context to help know what is debugged.
 *          See devtools/server/actors/watcher/session-context.js
 */
webExtensionTargetPrototype.initialize = function(
  conn,
  {
    addonId,
    addonBrowsingContextGroupId,
    chromeGlobal,
    isTopLevelTarget,
    prefix,
    sessionContext,
  }
) {
  this.addonId = addonId;
  this.addonBrowsingContextGroupId = addonBrowsingContextGroupId;
  this.chromeGlobal = chromeGlobal;

  // Expose the BrowsingContext of the fallback document,
  // which is the one this target actor will always refer to via its form()
  // and all resources should be related to this one as we currently spawn
  // only just this one target actor to debug all webextension documents.
  this.devtoolsSpawnedBrowsingContextForWebExtension =
    chromeGlobal.browsingContext;

  // Try to discovery an existent extension page to attach (which will provide the initial
  // URL shown in the window tittle when the addon debugger is opened).
  const extensionWindow = this._searchForExtensionWindow();

  parentProcessTargetPrototype.initialize.call(this, conn, {
    isTopLevelTarget,
    window: extensionWindow,
    sessionContext,
  });
  this._chromeGlobal = chromeGlobal;
  this._prefix = prefix;

  // Redefine the messageManager getter to return the chromeGlobal
  // as the messageManager for this actor (which is the browser XUL
  // element used by the parent actor running in the main process to
  // connect to the extension process).
  Object.defineProperty(this, "messageManager", {
    enumerable: true,
    configurable: true,
    get: () => {
      return this._chromeGlobal;
    },
  });

  this._onParentExit = this._onParentExit.bind(this);

  this._chromeGlobal.addMessageListener(
    "debug:webext_parent_exit",
    this._onParentExit
  );

  // Set the consoleAPIListener filtering options
  // (retrieved and used in the related webconsole child actor).
  this.consoleAPIListenerOptions = {
    addonId: this.addonId,
  };

  this.aps = Cc["@mozilla.org/addons/policy-service;1"].getService(
    Ci.nsIAddonPolicyService
  );

  // This creates a Debugger instance for debugging all the add-on globals.
  this.makeDebugger = makeDebugger.bind(null, {
    findDebuggees: dbg => {
      return dbg.findAllGlobals().filter(this._shouldAddNewGlobalAsDebuggee);
    },
    shouldAddNewGlobalAsDebuggee: this._shouldAddNewGlobalAsDebuggee.bind(this),
  });
};

// NOTE: This is needed to catch in the webextension webconsole all the
// errors raised by the WebExtension internals that are not currently
// associated with any window.
webExtensionTargetPrototype.isRootActor = true;

// Override the ParentProcessTargetActor's override in order to only iterate
// over the docshells specific to this add-on
Object.defineProperty(webExtensionTargetPrototype, "docShells", {
  get: function() {
    // Iterate over all top-level windows and all their docshells.
    let docShells = [];
    for (const window of Services.ww.getWindowEnumerator(null)) {
      docShells = docShells.concat(getChildDocShells(window.docShell));
    }
    // Then filter out the ones specific to the add-on
    return docShells.filter(docShell => {
      return this.isExtensionWindowDescendent(docShell.domWindow);
    });
  },
});

/**
 * Called when the actor is removed from the connection.
 */
webExtensionTargetPrototype.destroy = function() {
  if (this._chromeGlobal) {
    const chromeGlobal = this._chromeGlobal;
    this._chromeGlobal = null;

    chromeGlobal.removeMessageListener(
      "debug:webext_parent_exit",
      this._onParentExit
    );

    chromeGlobal.sendAsyncMessage("debug:webext_child_exit", {
      actor: this.actorID,
    });
  }

  if (this.fallbackWindow) {
    this.fallbackWindow = null;
  }

  this.addon = null;
  this.addonId = null;

  return ParentProcessTargetActor.prototype.destroy.apply(this);
};

// Private helpers.

webExtensionTargetPrototype._searchFallbackWindow = function() {
  if (this.fallbackWindow) {
    // Skip if there is already an existent fallback window.
    return this.fallbackWindow;
  }

  // Set and initialize the fallbackWindow (which initially is a empty
  // about:blank browser), this window is related to a XUL browser element
  // specifically created for the devtools server and it is never used
  // or navigated anywhere else.
  this.fallbackWindow = this.chromeGlobal.content;

  // Add the addonId in the URL to retrieve this information in other devtools
  // helpers. The addonId is usually populated in the principal, but this will
  // not be the case for the fallback window because it is loaded from chrome://
  // instead of moz-extension://${addonId}
  this.fallbackWindow.document.location.href = `${FALLBACK_DOC_URL}#${this.addonId}`;

  return this.fallbackWindow;
};

// Discovery an extension page to use as a default target window.
// NOTE: This currently fail to discovery an extension page running in a
// windowless browser when running in non-oop mode, and the background page
// is set later using _onNewExtensionWindow.
webExtensionTargetPrototype._searchForExtensionWindow = function() {
  // Looks if there is any top level add-on document:
  // (we do not want to pass any nested add-on iframe)
  const docShell = this.docShells.find(d =>
    this.isTopLevelExtensionWindow(d.domWindow)
  );
  if (docShell) {
    return docShell.domWindow;
  }

  return this._searchFallbackWindow();
};

// Customized ParentProcessTargetActor/WindowGlobalTargetActor hooks.

webExtensionTargetPrototype._onDocShellCreated = function(docShell) {
  // Compare against the BrowsingContext's group ID as the document's principal addonId
  // won't be set yet for freshly created docshells. It will be later set, when loading the addon URL.
  // But right now, it is still on the initial about:blank document and the principal isn't related to the add-on.
  if (docShell.browsingContext.group.id != this.addonBrowsingContextGroupId) {
    return;
  }
  ParentProcessTargetActor.prototype._onDocShellCreated.call(this, docShell);
};

webExtensionTargetPrototype._onDocShellDestroy = function(docShell) {
  if (docShell.browsingContext.group.id != this.addonBrowsingContextGroupId) {
    return;
  }
  // Stop watching this docshell (the unwatch() method will check if we
  // started watching it before).
  this._unwatchDocShell(docShell);

  // Let the _onDocShellDestroy notify that the docShell has been destroyed.
  const webProgress = docShell
    .QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIWebProgress);
  this._notifyDocShellDestroy(webProgress);

  // If the destroyed docShell was the current docShell and the actor is
  // not destroyed, switch to the fallback window
  if (!this.isDestroyed() && docShell == this.docShell) {
    this._changeTopLevelDocument(this._searchForExtensionWindow());
  }
};

webExtensionTargetPrototype._onNewExtensionWindow = function(window) {
  if (!this.window || this.window === this.fallbackWindow) {
    this._changeTopLevelDocument(window);
  }
};

webExtensionTargetPrototype.isTopLevelExtensionWindow = function(window) {
  const { docShell } = window;
  const isTopLevel = docShell.sameTypeRootTreeItem == docShell;
  return isTopLevel && window.document.nodePrincipal.addonId == this.addonId;
};

webExtensionTargetPrototype.isExtensionWindowDescendent = function(window) {
  // Check if the source is coming from a descendant docShell of an extension window.
  // We may have an iframe that loads http content which won't use the add-on principal.
  const rootWin = window.docShell.sameTypeRootTreeItem.domWindow;
  return rootWin.document.nodePrincipal.addonId == this.addonId;
};

/**
 * Return true if the given global is associated with this addon and should be
 * added as a debuggee, false otherwise.
 */
webExtensionTargetPrototype._shouldAddNewGlobalAsDebuggee = function(
  newGlobal
) {
  const global = unwrapDebuggerObjectGlobal(newGlobal);

  if (global instanceof Ci.nsIDOMWindow) {
    try {
      global.document;
    } catch (e) {
      // The global might be a sandbox with a window object in its proto chain. If the
      // window navigated away since the sandbox was created, it can throw a security
      // exception during this property check as the sandbox no longer has access to
      // its own proto.
      return false;
    }
    // When `global` is a sandbox it may be a nsIDOMWindow object,
    // but won't be the real Window object. Retrieve it via document's ownerGlobal.
    const window = global.document.ownerGlobal;
    if (!window) {
      return false;
    }

    // Change top level document as a simulated frame switching.
    if (this.isTopLevelExtensionWindow(window)) {
      this._onNewExtensionWindow(window);
    }

    return this.isExtensionWindowDescendent(window);
  }

  try {
    // This will fail for non-Sandbox objects, hence the try-catch block.
    const metadata = Cu.getSandboxMetadata(global);
    if (metadata) {
      return metadata.addonID === this.addonId;
    }
  } catch (e) {
    // Unable to retrieve the sandbox metadata.
  }

  return false;
};

// Handlers for the messages received from the parent actor.

webExtensionTargetPrototype._onParentExit = function(msg) {
  if (msg.json.actor !== this.actorID) {
    return;
  }

  this.destroy();
};

exports.WebExtensionTargetActor = TargetActorMixin(
  Targets.TYPES.FRAME,
  webExtensionTargetSpec,
  webExtensionTargetPrototype
);
