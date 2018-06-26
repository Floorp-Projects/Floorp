/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Target actor for the entire parent process.
 *
 * This actor extends BrowsingContextTargetActor.
 * This actor is extended by WebExtensionTargetActor.
 *
 * See devtools/docs/backend/actor-hierarchy.md for more details.
 */

const { Ci } = require("chrome");
const Services = require("Services");
const { DebuggerServer } = require("devtools/server/main");
const {
  getChildDocShells,
  BrowsingContextTargetActor,
  browsingContextTargetPrototype
} = require("devtools/server/actors/targets/browsing-context");
const makeDebugger = require("devtools/server/actors/utils/make-debugger");

const { extend } = require("devtools/shared/extend");
const { ActorClassWithSpec } = require("devtools/shared/protocol");
const { parentProcessTargetSpec } = require("devtools/shared/specs/targets/parent-process");

/**
 * Protocol.js expects only the prototype object, and does not maintain the prototype
 * chain when it constructs the ActorClass. For this reason we are using `extend` to
 * maintain the properties of BrowsingContextTargetActor.prototype
 */
const parentProcessTargetPrototype = extend({}, browsingContextTargetPrototype);

/**
 * Creates a target actor for debugging all the chrome content in the parent process.
 * Most of the implementation is inherited from BrowsingContextTargetActor.
 * ParentProcessTargetActor is a child of RootActor, it can be instantiated via
 * RootActor.getProcess request. ParentProcessTargetActor exposes all target-scoped actors
 * via its form() request, like BrowsingContextTargetActor.
 *
 * @param connection DebuggerServerConnection
 *        The connection to the client.
 */
parentProcessTargetPrototype.initialize = function(connection) {
  BrowsingContextTargetActor.prototype.initialize.call(this, connection);

  // This creates a Debugger instance for chrome debugging all globals.
  this.makeDebugger = makeDebugger.bind(null, {
    findDebuggees: dbg => dbg.findAllGlobals(),
    shouldAddNewGlobalAsDebuggee: () => true
  });

  // Ensure catching the creation of any new content docshell
  this.listenForNewDocShells = true;

  // Defines the default docshell selected for the target actor
  let window = Services.wm.getMostRecentWindow(DebuggerServer.chromeWindowType);

  // Default to any available top level window if there is no expected window
  // (for example when we open firefox with -webide argument)
  if (!window) {
    window = Services.wm.getMostRecentWindow(null);
  }

  // We really want _some_ window at least, so fallback to the hidden window if
  // there's nothing else (such as during early startup).
  if (!window) {
    try {
      window = Services.appShell.hiddenDOMWindow;
    } catch (e) {
      // On XPCShell, the above line will throw.
    }
  }

  // On XPCShell, there is no window/docshell
  const docShell = window ? window.QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIDocShell)
                        : null;
  Object.defineProperty(this, "docShell", {
    value: docShell,
    configurable: true
  });
};

parentProcessTargetPrototype.isRootActor = true;

/**
 * Getter for the list of all docshells in this targetActor
 * @return {Array}
 */
Object.defineProperty(parentProcessTargetPrototype, "docShells", {
  get: function() {
    // Iterate over all top-level windows and all their docshells.
    let docShells = [];
    const e = Services.ww.getWindowEnumerator();
    while (e.hasMoreElements()) {
      const window = e.getNext();
      const docShell = window.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIWebNavigation)
                           .QueryInterface(Ci.nsIDocShell);
      docShells = docShells.concat(getChildDocShells(docShell));
    }

    return docShells;
  }
});

parentProcessTargetPrototype.observe = function(subject, topic, data) {
  BrowsingContextTargetActor.prototype.observe.call(this, subject, topic, data);
  if (!this.attached) {
    return;
  }

  subject.QueryInterface(Ci.nsIDocShell);

  if (topic == "chrome-webnavigation-create") {
    this._onDocShellCreated(subject);
  } else if (topic == "chrome-webnavigation-destroy") {
    this._onDocShellDestroy(subject);
  }
};

parentProcessTargetPrototype._attach = function() {
  if (this.attached) {
    return false;
  }

  BrowsingContextTargetActor.prototype._attach.call(this);

  // Listen for any new/destroyed chrome docshell
  Services.obs.addObserver(this, "chrome-webnavigation-create");
  Services.obs.addObserver(this, "chrome-webnavigation-destroy");

  // Iterate over all top-level windows.
  const e = Services.ww.getWindowEnumerator();
  while (e.hasMoreElements()) {
    const window = e.getNext();
    const docShell = window.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIWebNavigation)
                         .QueryInterface(Ci.nsIDocShell);
    if (docShell == this.docShell) {
      continue;
    }
    this._progressListener.watch(docShell);
  }
  return undefined;
};

parentProcessTargetPrototype._detach = function() {
  if (!this.attached) {
    return false;
  }

  Services.obs.removeObserver(this, "chrome-webnavigation-create");
  Services.obs.removeObserver(this, "chrome-webnavigation-destroy");

  // Iterate over all top-level windows.
  const e = Services.ww.getWindowEnumerator();
  while (e.hasMoreElements()) {
    const window = e.getNext();
    const docShell = window.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIWebNavigation)
                         .QueryInterface(Ci.nsIDocShell);
    if (docShell == this.docShell) {
      continue;
    }
    this._progressListener.unwatch(docShell);
  }

  BrowsingContextTargetActor.prototype._detach.call(this);
  return undefined;
};

/* ThreadActor hooks. */

/**
 * Prepare to enter a nested event loop by disabling debuggee events.
 */
parentProcessTargetPrototype.preNest = function() {
  // Disable events in all open windows.
  const e = Services.wm.getEnumerator(null);
  while (e.hasMoreElements()) {
    const win = e.getNext();
    const windowUtils = win.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIDOMWindowUtils);
    windowUtils.suppressEventHandling(true);
    windowUtils.suspendTimeouts();
  }
};

/**
 * Prepare to exit a nested event loop by enabling debuggee events.
 */
parentProcessTargetPrototype.postNest = function(nestData) {
  // Enable events in all open windows.
  const e = Services.wm.getEnumerator(null);
  while (e.hasMoreElements()) {
    const win = e.getNext();
    const windowUtils = win.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIDOMWindowUtils);
    windowUtils.resumeTimeouts();
    windowUtils.suppressEventHandling(false);
  }
};

exports.parentProcessTargetPrototype = parentProcessTargetPrototype;
exports.ParentProcessTargetActor =
  ActorClassWithSpec(parentProcessTargetSpec, parentProcessTargetPrototype);
