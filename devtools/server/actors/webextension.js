/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { extend } = require("devtools/shared/extend");
const { Ci, Cu, Cc } = require("chrome");
const Services = require("Services");

const { ChromeActor, chromePrototype } = require("./chrome");
const makeDebugger = require("./utils/make-debugger");
const { ActorClassWithSpec } = require("devtools/shared/protocol");
const { tabSpec } = require("devtools/shared/specs/tab");

loader.lazyRequireGetter(this, "unwrapDebuggerObjectGlobal", "devtools/server/actors/thread", true);
loader.lazyRequireGetter(this, "ChromeUtils");
const FALLBACK_DOC_MESSAGE = "Your addon does not have any document opened yet.";

/**
 * Creates a TabActor for debugging all the contexts associated to a target WebExtensions
 * add-on running in a child extension process.
 * Most of the implementation is inherited from ChromeActor (which inherits most of its
 * implementation from TabActor).
 * WebExtensionChildActor is created by a WebExtensionParentActor counterpart, when its
 * parent actor's `connect` method has been called (on the listAddons RDP package),
 * it runs in the same process that the extension is running into (which can be the main
 * process if the extension is running in non-oop mode, or the child extension process
 * if the extension is running in oop-mode).
 *
 * A WebExtensionChildActor contains all tab actors, like a regular ChromeActor
 * or TabActor.
 *
 * History lecture:
 * - The add-on actors used to not inherit TabActor because of the different way the
 * add-on APIs where exposed to the add-on itself, and for this reason the Addon Debugger
 * has only a sub-set of the feature available in the Tab or in the Browser Toolbox.
 * - In a WebExtensions add-on all the provided contexts (background, popups etc.),
 * besides the Content Scripts which run in the content process, hooked to an existent
 * tab, by creating a new WebExtensionActor which inherits from ChromeActor, we can
 * provide a full features Addon Toolbox (which is basically like a BrowserToolbox which
 * filters the visible sources and frames to the one that are related to the target
 * add-on).
 * - When the WebExtensions OOP mode has been introduced, this actor has been refactored
 * and moved from the main process to the new child extension process.
 *
 * @param {DebuggerServerConnection} conn
 *        The connection to the client.
 * @param {nsIMessageSender} chromeGlobal.
 *        The chromeGlobal where this actor has been injected by the
 *        DebuggerServer.connectToFrame method.
 * @param {string} prefix
 *        the custom RDP prefix to use.
 * @param {string} addonId
 *        the addonId of the target WebExtension.
 */

const webExtensionChildPrototype = extend({}, chromePrototype);

webExtensionChildPrototype.initialize = function(conn, chromeGlobal, prefix, addonId) {
  chromePrototype.initialize.call(this, conn);
  this._chromeGlobal = chromeGlobal;
  this._prefix = prefix;
  this.id = addonId;

  // Redefine the messageManager getter to return the chromeGlobal
  // as the messageManager for this actor (which is the browser XUL
  // element used by the parent actor running in the main process to
  // connect to the extension process).
  Object.defineProperty(this, "messageManager", {
    enumerable: true,
    configurable: true,
    get: () => {
      return this._chromeGlobal;
    }
  });

  // Bind the _allowSource helper to this, it is used in the
  // TabActor to lazily create the TabSources instance.
  this._allowSource = this._allowSource.bind(this);
  this._onParentExit = this._onParentExit.bind(this);

  this._chromeGlobal.addMessageListener("debug:webext_parent_exit", this._onParentExit);

  // Set the consoleAPIListener filtering options
  // (retrieved and used in the related webconsole child actor).
  this.consoleAPIListenerOptions = {
    addonId: this.id,
  };

  this.aps = Cc["@mozilla.org/addons/policy-service;1"]
               .getService(Ci.nsIAddonPolicyService);

  // This creates a Debugger instance for debugging all the add-on globals.
  this.makeDebugger = makeDebugger.bind(null, {
    findDebuggees: dbg => {
      return dbg.findAllGlobals().filter(this._shouldAddNewGlobalAsDebuggee);
    },
    shouldAddNewGlobalAsDebuggee: this._shouldAddNewGlobalAsDebuggee.bind(this),
  });

  // Try to discovery an existent extension page to attach (which will provide the initial
  // URL shown in the window tittle when the addon debugger is opened).
  let extensionWindow = this._searchForExtensionWindow();

  if (extensionWindow) {
    this._setWindow(extensionWindow);
  }
};

webExtensionChildPrototype.typeName = "webExtension";

// NOTE: This is needed to catch in the webextension webconsole all the
// errors raised by the WebExtension internals that are not currently
// associated with any window.
webExtensionChildPrototype.isRootActor = true;

/**
 * Called when the actor is removed from the connection.
 */
webExtensionChildPrototype.exit = function() {
  if (this._chromeGlobal) {
    let chromeGlobal = this._chromeGlobal;
    this._chromeGlobal = null;

    chromeGlobal.removeMessageListener("debug:webext_parent_exit", this._onParentExit);

    chromeGlobal.sendAsyncMessage("debug:webext_child_exit", {
      actor: this.actorID
    });
  }

  this.addon = null;
  this.id = null;

  return ChromeActor.prototype.exit.apply(this);
};

// Private helpers.

webExtensionChildPrototype._createFallbackWindow = function() {
  if (this.fallbackWindow) {
    // Skip if there is already an existent fallback window.
    return;
  }

  // Create an empty hidden window as a fallback (e.g. the background page could be
  // not defined for the target add-on or not yet when the actor instance has been
  // created).
  this.fallbackWebNav = Services.appShell.createWindowlessBrowser(true);

  // Save the reference to the fallback DOMWindow.
  this.fallbackWindow = this.fallbackWebNav.QueryInterface(Ci.nsIInterfaceRequestor)
                                           .getInterface(Ci.nsIDOMWindow);

  // Insert the fallback doc message.
  this.fallbackWindow.document.body.innerText = FALLBACK_DOC_MESSAGE;
};

webExtensionChildPrototype._destroyFallbackWindow = function() {
  if (this.fallbackWebNav) {
    // Explicitly close the fallback windowless browser to prevent it to leak
    // (and to prevent it to freeze devtools xpcshell tests).
    this.fallbackWebNav.loadURI("about:blank", 0, null, null, null);
    this.fallbackWebNav.close();

    this.fallbackWebNav = null;
    this.fallbackWindow = null;
  }
};

// Discovery an extension page to use as a default target window.
// NOTE: This currently fail to discovery an extension page running in a
// windowless browser when running in non-oop mode, and the background page
// is set later using _onNewExtensionWindow.
webExtensionChildPrototype._searchForExtensionWindow = function() {
  let e = Services.ww.getWindowEnumerator(null);
  while (e.hasMoreElements()) {
    let window = e.getNext();

    if (window.document.nodePrincipal.addonId == this.id) {
      return window;
    }
  }

  return undefined;
};

// Customized ChromeActor/TabActor hooks.

webExtensionChildPrototype._onDocShellDestroy = function(docShell) {
  // Stop watching this docshell (the unwatch() method will check if we
  // started watching it before).
  this._unwatchDocShell(docShell);

  // Let the _onDocShellDestroy notify that the docShell has been destroyed.
  let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIWebProgress);
  this._notifyDocShellDestroy(webProgress);

  // If the destroyed docShell was the current docShell and the actor is
  // currently attached, switch to the fallback window
  if (this.attached && docShell == this.docShell) {
    // Creates a fallback window if it doesn't exist yet.
    this._createFallbackWindow();
    this._changeTopLevelDocument(this.fallbackWindow);
  }
};

webExtensionChildPrototype._onNewExtensionWindow = function(window) {
  if (!this.window || this.window === this.fallbackWindow) {
    this._changeTopLevelDocument(window);
  }
};

webExtensionChildPrototype._attach = function() {
  // NOTE: we need to be sure that `this.window` can return a
  // window before calling the ChromeActor.onAttach, or the TabActor
  // will not be subscribed to the child doc shell updates.

  if (!this.window || this.window.document.nodePrincipal.addonId !== this.id) {
    // Discovery an existent extension page to attach.
    let extensionWindow = this._searchForExtensionWindow();

    if (!extensionWindow) {
      this._createFallbackWindow();
      this._setWindow(this.fallbackWindow);
    } else {
      this._setWindow(extensionWindow);
    }
  }

  // Call ChromeActor's _attach to listen for any new/destroyed chrome docshell
  ChromeActor.prototype._attach.apply(this);
};

webExtensionChildPrototype._detach = function() {
  // Call ChromeActor's _detach to unsubscribe new/destroyed chrome docshell listeners.
  ChromeActor.prototype._detach.apply(this);

  // Stop watching for new extension windows.
  this._destroyFallbackWindow();
};

/**
 * Return the json details related to a docShell.
 */
webExtensionChildPrototype._docShellToWindow = function(docShell) {
  const baseWindowDetails = ChromeActor.prototype._docShellToWindow.call(this, docShell);

  let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIWebProgress);
  let window = webProgress.DOMWindow;

  // Collect the addonID from the document origin attributes and its sameType top level
  // frame.
  let addonID = window.document.nodePrincipal.addonId;
  let sameTypeRootAddonID = docShell.QueryInterface(Ci.nsIDocShellTreeItem)
                                    .sameTypeRootTreeItem
                                    .QueryInterface(Ci.nsIInterfaceRequestor)
                                    .getInterface(Ci.nsIDOMWindow)
                                    .document.nodePrincipal.addonId;

  return Object.assign(baseWindowDetails, {
    addonID,
    sameTypeRootAddonID,
  });
};

/**
 * Return an array of the json details related to an array/iterator of docShells.
 */
webExtensionChildPrototype._docShellsToWindows = function(docshells) {
  return ChromeActor.prototype._docShellsToWindows.call(this, docshells)
                    .filter(windowDetails => {
                      // Filter the docShells based on the addon id of the window or
                      // its sameType top level frame.
                      return windowDetails.addonID === this.id ||
                             windowDetails.sameTypeRootAddonID === this.id;
                    });
};

webExtensionChildPrototype.isExtensionWindow = function(window) {
  return window.document.nodePrincipal.addonId == this.id;
};

webExtensionChildPrototype.isExtensionWindowDescendent = function(window) {
  // Check if the source is coming from a descendant docShell of an extension window.
  let docShell = window.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDocShell);
  let rootWin = docShell.sameTypeRootTreeItem.QueryInterface(Ci.nsIInterfaceRequestor)
                                             .getInterface(Ci.nsIDOMWindow);
  return this.isExtensionWindow(rootWin);
};

/**
 * Return true if the given source is associated with this addon and should be
 * added to the visible sources (retrieved and used by the webbrowser actor module).
 */
webExtensionChildPrototype._allowSource = function(source) {
  // Use the source.element to detect the allowed source, if any.
  if (source.element) {
    let domEl = unwrapDebuggerObjectGlobal(source.element);
    return (this.isExtensionWindow(domEl.ownerGlobal) ||
            this.isExtensionWindowDescendent(domEl.ownerGlobal));
  }

  // Fallback to check the uri if there is no source.element associated to the source.

  // Retrieve the first component of source.url in the form "url1 -> url2 -> ...".
  let url = source.url.split(" -> ").pop();

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
    let addonID = this.aps.extensionURIToAddonId(uri);

    return addonID == this.id;
  } catch (err) {
    // extensionURIToAddonId raises an exception on non-extension URLs.
    return false;
  }
};

/**
 * Return true if the given global is associated with this addon and should be
 * added as a debuggee, false otherwise.
 */
webExtensionChildPrototype._shouldAddNewGlobalAsDebuggee = function(newGlobal) {
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

    // Filter out any global which contains a XUL document.
    if (ChromeUtils.getClassName(global.document) == "XULDocument") {
      return false;
    }

    // Change top level document as a simulated frame switching.
    if (global.document.ownerGlobal && this.isExtensionWindow(global)) {
      this._onNewExtensionWindow(global.document.ownerGlobal);
    }

    return global.document.ownerGlobal &&
           this.isExtensionWindowDescendent(global.document.ownerGlobal);
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

// Handlers for the messages received from the parent actor.

webExtensionChildPrototype._onParentExit = function(msg) {
  if (msg.json.actor !== this.actorID) {
    return;
  }

  this.exit();
};

exports.WebExtensionChildActor = ActorClassWithSpec(tabSpec, webExtensionChildPrototype);
