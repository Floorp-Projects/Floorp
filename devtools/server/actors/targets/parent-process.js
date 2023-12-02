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

const {
  DevToolsServer,
} = require("resource://devtools/server/devtools-server.js");
const {
  getChildDocShells,
  WindowGlobalTargetActor,
} = require("resource://devtools/server/actors/targets/window-global.js");
const makeDebugger = require("resource://devtools/server/actors/utils/make-debugger.js");

const {
  parentProcessTargetSpec,
} = require("resource://devtools/shared/specs/targets/parent-process.js");

class ParentProcessTargetActor extends WindowGlobalTargetActor {
  /**
   * Creates a target actor for debugging all the chrome content in the parent process.
   * Most of the implementation is inherited from WindowGlobalTargetActor.
   * ParentProcessTargetActor is a child of RootActor, it can be instantiated via
   * RootActor.getProcess request. ParentProcessTargetActor exposes all target-scoped actors
   * via its form() request, like WindowGlobalTargetActor.
   *
   * @param conn DevToolsServerConnection
   *        The connection to the client.
   * @param {Object} options
   *        - isTopLevelTarget: {Boolean} flag to indicate if this is the top
   *          level target of the DevTools session
   *        - sessionContext Object
   *          The Session Context to help know what is debugged.
   *          See devtools/server/actors/watcher/session-context.js
   *        - customSpec Object
   *          WebExtensionTargetActor inherits from ParentProcessTargetActor
   *          and has to use its own protocol.js specification object.
   */
  constructor(
    conn,
    { isTopLevelTarget, sessionContext, customSpec = parentProcessTargetSpec }
  ) {
    super(conn, {
      isTopLevelTarget,
      sessionContext,
      customSpec,
    });

    // This creates a Debugger instance for chrome debugging all globals.
    this.makeDebugger = makeDebugger.bind(null, {
      findDebuggees: dbg =>
        dbg.findAllGlobals().map(g => g.unsafeDereference()),
      shouldAddNewGlobalAsDebuggee: () => true,
    });

    // Ensure catching the creation of any new content docshell
    this.watchNewDocShells = true;

    this.isRootActor = true;

    // Listen for any new/destroyed chrome docshell
    Services.obs.addObserver(this, "chrome-webnavigation-create");
    Services.obs.addObserver(this, "chrome-webnavigation-destroy");

    // If we are the parent process target actor and not a subclass
    // (i.e. if we aren't the webext target actor)
    // set the parent process docshell:
    if (customSpec == parentProcessTargetSpec) {
      this.setDocShell(this._getInitialDocShell());
    }
  }

  // Overload setDocShell in order to observe all the docshells.
  // WindowGlobalTargetActor only observes the top level one,
  // but we also need to observe all of them for WebExtensionTargetActor subclass.
  setDocShell(initialDocShell) {
    super.setDocShell(initialDocShell);

    // Iterate over all top-level windows.
    for (const { docShell } of Services.ww.getWindowEnumerator()) {
      if (docShell == this.docShell) {
        continue;
      }
      this._progressListener.watch(docShell);
    }
  }

  _getInitialDocShell() {
    // Defines the default docshell selected for the target actor
    let window = Services.wm.getMostRecentWindow(
      DevToolsServer.chromeWindowType
    );

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
    return window.docShell;
  }

  /**
   * Getter for the list of all docshells in this targetActor
   * @return {Array}
   */
  get docShells() {
    // Iterate over all top-level windows and all their docshells.
    let docShells = [];
    for (const { docShell } of Services.ww.getWindowEnumerator()) {
      docShells = docShells.concat(getChildDocShells(docShell));
    }

    return docShells;
  }

  observe(subject, topic, data) {
    super.observe(subject, topic, data);
    if (this.isDestroyed()) {
      return;
    }

    subject.QueryInterface(Ci.nsIDocShell);

    if (topic == "chrome-webnavigation-create") {
      this._onDocShellCreated(subject);
    } else if (topic == "chrome-webnavigation-destroy") {
      this._onDocShellDestroy(subject);
    }
  }

  _detach() {
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

    return super._detach();
  }
}

exports.ParentProcessTargetActor = ParentProcessTargetActor;
