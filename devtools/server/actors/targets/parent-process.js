/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Target actor for the entire parent process.
 *
 * This actor extends WindowGlobalTargetActor.
 * This actor is extended by WebExtensionTargetActor.
 *
 * See devtools/docs/backend/actor-hierarchy.md for more details.
 */

const { Ci } = require("chrome");
const { DevToolsServer } = require("devtools/server/devtools-server");
const {
  getChildDocShells,
  WindowGlobalTargetActor,
  windowGlobalTargetPrototype,
} = require("devtools/server/actors/targets/window-global");
const makeDebugger = require("devtools/server/actors/utils/make-debugger");

const { extend } = require("devtools/shared/extend");
const {
  parentProcessTargetSpec,
} = require("devtools/shared/specs/targets/parent-process");
const Targets = require("devtools/server/actors/targets/index");
const TargetActorMixin = require("devtools/server/actors/targets/target-actor-mixin");

/**
 * Protocol.js expects only the prototype object, and does not maintain the prototype
 * chain when it constructs the ActorClass. For this reason we are using `extend` to
 * maintain the properties of WindowGlobalTargetActor.prototype
 */
const parentProcessTargetPrototype = extend({}, windowGlobalTargetPrototype);

/**
 * Creates a target actor for debugging all the chrome content in the parent process.
 * Most of the implementation is inherited from WindowGlobalTargetActor.
 * ParentProcessTargetActor is a child of RootActor, it can be instantiated via
 * RootActor.getProcess request. ParentProcessTargetActor exposes all target-scoped actors
 * via its form() request, like WindowGlobalTargetActor.
 *
 * @param connection DevToolsServerConnection
 *        The connection to the client.
 * @param window {Window} object (optional)
 *        If the upper class already knows against which window the actor should attach,
 *        it is passed as a constructor argument here.
 * @param {Object} options
 *        - isTopLevelTarget: {Boolean} flag to indicate if this is the top
 *          level target of the DevTools session
 *        - window: {Window} If the upper class already knows against which
 *          window the actor should attach, it is passed as a constructor
 *          argument here.
 *        - sessionContext Object
 *          The Session Context to help know what is debugged.
 *          See devtools/server/actors/watcher/session-context.js
 */
parentProcessTargetPrototype.initialize = function(
  connection,
  { isTopLevelTarget, window, sessionContext }
) {
  // Defines the default docshell selected for the target actor
  if (!window) {
    window = Services.wm.getMostRecentWindow(DevToolsServer.chromeWindowType);
  }

  // Default to any available top level window if there is no expected window
  // eg when running ./mach run --chrome chrome://browser/content/aboutTabCrashed.xhtml --jsdebugger
  if (!window) {
    window = Services.wm.getMostRecentWindow(null);
  }

  // We really want _some_ window at least, so fallback to the hidden window if
  // there's nothing else (such as during early startup).
  if (!window) {
    window = Services.appShell.hiddenDOMWindow;
  }

  WindowGlobalTargetActor.prototype.initialize.call(this, connection, {
    docShell: window.docShell,
    isTopLevelTarget,
    sessionContext,
  });

  // This creates a Debugger instance for chrome debugging all globals.
  this.makeDebugger = makeDebugger.bind(null, {
    findDebuggees: dbg => dbg.findAllGlobals(),
    shouldAddNewGlobalAsDebuggee: () => true,
  });

  // Ensure catching the creation of any new content docshell
  this.watchNewDocShells = true;

  // Listen for any new/destroyed chrome docshell
  Services.obs.addObserver(this, "chrome-webnavigation-create");
  Services.obs.addObserver(this, "chrome-webnavigation-destroy");

  // Iterate over all top-level windows.
  for (const { docShell } of Services.ww.getWindowEnumerator()) {
    if (docShell == this.docShell) {
      continue;
    }
    this._progressListener.watch(docShell);
  }
};

parentProcessTargetPrototype.isRootActor = true;

/**
 * Getter for the list of all docshells in this targetActor
 * @return {Array}
 */
Object.defineProperty(parentProcessTargetPrototype, "docShells", {
  get() {
    // Iterate over all top-level windows and all their docshells.
    let docShells = [];
    for (const { docShell } of Services.ww.getWindowEnumerator()) {
      docShells = docShells.concat(getChildDocShells(docShell));
    }

    return docShells;
  },
});

parentProcessTargetPrototype.observe = function(subject, topic, data) {
  WindowGlobalTargetActor.prototype.observe.call(this, subject, topic, data);
  if (this.isDestroyed()) {
    return;
  }

  subject.QueryInterface(Ci.nsIDocShell);

  if (topic == "chrome-webnavigation-create") {
    this._onDocShellCreated(subject);
  } else if (topic == "chrome-webnavigation-destroy") {
    this._onDocShellDestroy(subject);
  }
};

parentProcessTargetPrototype._detach = function() {
  if (this.isDestroyed()) {
    return false;
  }

  Services.obs.removeObserver(this, "chrome-webnavigation-create");
  Services.obs.removeObserver(this, "chrome-webnavigation-destroy");

  // Iterate over all top-level windows.
  for (const { docShell } of Services.ww.getWindowEnumerator()) {
    if (docShell == this.docShell) {
      continue;
    }
    this._progressListener.unwatch(docShell);
  }

  return WindowGlobalTargetActor.prototype._detach.call(this);
};

exports.parentProcessTargetPrototype = parentProcessTargetPrototype;
exports.ParentProcessTargetActor = TargetActorMixin(
  Targets.TYPES.FRAME,
  parentProcessTargetSpec,
  parentProcessTargetPrototype
);
