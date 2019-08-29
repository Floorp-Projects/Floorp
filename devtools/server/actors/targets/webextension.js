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
const { ActorClassWithSpec } = require("devtools/shared/protocol");
const {
  webExtensionTargetSpec,
} = require("devtools/shared/specs/targets/webextension");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");

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
 * maintain the properties of BrowsingContextTargetActor.prototype
 */
const webExtensionTargetPrototype = extend({}, parentProcessTargetPrototype);

/**
 * Creates a target actor for debugging all the contexts associated to a target
 * WebExtensions add-on running in a child extension process. Most of the implementation
 * is inherited from ParentProcessTargetActor (which inherits most of its implementation
 * from BrowsingContextTargetActor).
 *
 * WebExtensionTargetActor is created by a WebExtensionActor counterpart, when its
 * parent actor's `connect` method has been called (on the listAddons RDP package),
 * it runs in the same process that the extension is running into (which can be the main
 * process if the extension is running in non-oop mode, or the child extension process
 * if the extension is running in oop-mode).
 *
 * A WebExtensionTargetActor contains all target-scoped actors, like a regular
 * ParentProcessTargetActor or BrowsingContextTargetActor.
 *
 * History lecture:
 * - The add-on actors used to not inherit BrowsingContextTargetActor because of the
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
 * @param {DebuggerServerConnection} conn
 *        The connection to the client.
 * @param {nsIMessageSender} chromeGlobal.
 *        The chromeGlobal where this actor has been injected by the
 *        frame-connector.js connectToFrame method.
 * @param {string} prefix
 *        the custom RDP prefix to use.
 * @param {string} addonId
 *        the addonId of the target WebExtension.
 */
webExtensionTargetPrototype.initialize = function(
  conn,
  chromeGlobal,
  prefix,
  addonId
) {
  this.addonId = addonId;
  this.chromeGlobal = chromeGlobal;

  // Try to discovery an existent extension page to attach (which will provide the initial
  // URL shown in the window tittle when the addon debugger is opened).
  const extensionWindow = this._searchForExtensionWindow();

  parentProcessTargetPrototype.initialize.call(this, conn, extensionWindow);
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

  // Bind the _allowSource helper to this, it is used in the
  // BrowsingContextTargetActor to lazily create the TabSources instance.
  this._allowSource = this._allowSource.bind(this);
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

/**
 * Called when the actor is removed from the connection.
 */
webExtensionTargetPrototype.exit = function() {
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

  this.addon = null;
  this.addonId = null;

  return ParentProcessTargetActor.prototype.exit.apply(this);
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
  this.fallbackWindow.document.location.href = FALLBACK_DOC_URL;

  return this.fallbackWindow;
};

webExtensionTargetPrototype._destroyFallbackWindow = function() {
  if (this.fallbackWindow) {
    this.fallbackWindow = null;
  }
};

// Discovery an extension page to use as a default target window.
// NOTE: This currently fail to discovery an extension page running in a
// windowless browser when running in non-oop mode, and the background page
// is set later using _onNewExtensionWindow.
webExtensionTargetPrototype._searchForExtensionWindow = function() {
  for (const window of Services.ww.getWindowEnumerator(null)) {
    if (window.document.nodePrincipal.addonId == this.addonId) {
      return window;
    }
  }

  return this._searchFallbackWindow();
};

// Customized ParentProcessTargetActor/BrowsingContextTargetActor hooks.

webExtensionTargetPrototype._onDocShellDestroy = function(docShell) {
  // Stop watching this docshell (the unwatch() method will check if we
  // started watching it before).
  this._unwatchDocShell(docShell);

  // Let the _onDocShellDestroy notify that the docShell has been destroyed.
  const webProgress = docShell
    .QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIWebProgress);
  this._notifyDocShellDestroy(webProgress);

  // If the destroyed docShell was the current docShell and the actor is
  // currently attached, switch to the fallback window
  if (this.attached && docShell == this.docShell) {
    this._changeTopLevelDocument(this._searchForExtensionWindow());
  }
};

webExtensionTargetPrototype._onNewExtensionWindow = function(window) {
  if (!this.window || this.window === this.fallbackWindow) {
    this._changeTopLevelDocument(window);
  }
};

webExtensionTargetPrototype._attach = function() {
  // NOTE: we need to be sure that `this.window` can return a window before calling the
  // ParentProcessTargetActor.onAttach, or the BrowsingContextTargetActor will not be
  // subscribed to the child doc shell updates.

  if (
    !this.window ||
    this.window.document.nodePrincipal.addonId !== this.addonId
  ) {
    // Discovery an existent extension page (or fallback window) to attach.
    this._setWindow(this._searchForExtensionWindow());
  }

  // Call ParentProcessTargetActor's _attach to listen for any new/destroyed chrome
  // docshell.
  ParentProcessTargetActor.prototype._attach.apply(this);
};

webExtensionTargetPrototype._detach = function() {
  // Call ParentProcessTargetActor's _detach to unsubscribe new/destroyed chrome docshell
  // listeners.
  ParentProcessTargetActor.prototype._detach.apply(this);

  // Stop watching for new extension windows.
  this._destroyFallbackWindow();
};

/**
 * Return the json details related to a docShell.
 */
webExtensionTargetPrototype._docShellToWindow = function(docShell) {
  const baseWindowDetails = ParentProcessTargetActor.prototype._docShellToWindow.call(
    this,
    docShell
  );

  const webProgress = docShell
    .QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIWebProgress);
  const window = webProgress.DOMWindow;

  // Collect the addonID from the document origin attributes and its sameType top level
  // frame.
  const addonID = window.document.nodePrincipal.addonId;
  const sameTypeRootAddonID =
    docShell.sameTypeRootTreeItem.domWindow.document.nodePrincipal.addonId;

  return Object.assign(baseWindowDetails, {
    addonID,
    sameTypeRootAddonID,
  });
};

/**
 * Return an array of the json details related to an array/iterator of docShells.
 */
webExtensionTargetPrototype._docShellsToWindows = function(docshells) {
  return ParentProcessTargetActor.prototype._docShellsToWindows
    .call(this, docshells)
    .filter(windowDetails => {
      // Filter the docShells based on the addon id of the window or
      // its sameType top level frame.
      return (
        windowDetails.addonID === this.addonId ||
        windowDetails.sameTypeRootAddonID === this.addonId
      );
    });
};

webExtensionTargetPrototype.isExtensionWindow = function(window) {
  return window.document.nodePrincipal.addonId == this.addonId;
};

webExtensionTargetPrototype.isExtensionWindowDescendent = function(window) {
  // Check if the source is coming from a descendant docShell of an extension window.
  const rootWin = window.docShell.sameTypeRootTreeItem.domWindow;
  return this.isExtensionWindow(rootWin);
};

/**
 * Return true if the given source is associated with this addon and should be
 * added to the visible sources (retrieved and used by the webbrowser actor module).
 */
webExtensionTargetPrototype._allowSource = function(source) {
  // Use the source.element to detect the allowed source, if any.
  if (source.element) {
    try {
      const domEl = unwrapDebuggerObjectGlobal(source.element);
      return (
        this.isExtensionWindow(domEl.ownerGlobal) ||
        this.isExtensionWindowDescendent(domEl.ownerGlobal)
      );
    } catch (e) {
      // If the source's window is dead then the above will throw.
      DevToolsUtils.reportException("WebExtensionTarget.allowSource", e);
      return false;
    }
  }

  // Fallback to check the uri if there is no source.element associated to the source.

  // Retrieve the first component of source.url in the form "url1 -> url2 -> ...".
  const url = source.url.split(" -> ").pop();

  // Filter out the code introduced by evaluating code in the webconsole.
  if (url === "debugger eval code") {
    return false;
  }

  let uri;

  // Try to decode the url.
  try {
    uri = Services.io.newURI(url);
  } catch (err) {
    Cu.reportError(`Unexpected invalid url: ${url}`);
    return false;
  }

  // Filter out resource and chrome sources (which are related to the loaded internals).
  if (["resource", "chrome", "file"].includes(uri.scheme)) {
    return false;
  }

  try {
    const addonID = this.aps.extensionURIToAddonId(uri);

    return addonID == this.addonId;
  } catch (err) {
    // extensionURIToAddonId raises an exception on non-extension URLs.
    return false;
  }
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

    // Change top level document as a simulated frame switching.
    if (global.document.ownerGlobal && this.isExtensionWindow(global)) {
      this._onNewExtensionWindow(global.document.ownerGlobal);
    }

    return (
      global.document.ownerGlobal &&
      this.isExtensionWindowDescendent(global.document.ownerGlobal)
    );
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

  this.exit();
};

exports.WebExtensionTargetActor = ActorClassWithSpec(
  webExtensionTargetSpec,
  webExtensionTargetPrototype
);
