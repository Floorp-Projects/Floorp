/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Represents any process running in Firefox.
 * This can be:
 *  - the parent process, where all top level chrome window runs:
 *    like browser.xhtml, sidebars, devtools iframes, the browser console, ...
 *  - any content process
 *
 * There is some special cases in the class around:
 *  - xpcshell, where there is only one process which doesn't expose any DOM document
 *    And instead of exposing a ParentProcessTargetActor, getTarget will return
 *    a ContentProcessTargetActor.
 *  - background task, similarly to xpcshell, they don't expose any DOM document
 *    and this also works with a ContentProcessTargetActor.
 *
 * See devtools/docs/backend/actor-hierarchy.md for more details.
 */

const { DevToolsServer } = require("devtools/server/devtools-server");
const { Cc, Ci } = require("chrome");

const {
  createBrowserSessionContext,
  createContentProcessSessionContext,
} = require("devtools/server/actors/watcher/session-context");
const { ActorClassWithSpec, Actor } = require("devtools/shared/protocol");
const {
  processDescriptorSpec,
} = require("devtools/shared/specs/descriptors/process");

loader.lazyRequireGetter(
  this,
  "ContentProcessTargetActor",
  "devtools/server/actors/targets/content-process",
  true
);
loader.lazyRequireGetter(
  this,
  "ParentProcessTargetActor",
  "devtools/server/actors/targets/parent-process",
  true
);
loader.lazyRequireGetter(
  this,
  "connectToContentProcess",
  "devtools/server/connectors/content-process-connector",
  true
);
loader.lazyRequireGetter(
  this,
  "WatcherActor",
  "devtools/server/actors/watcher",
  true
);

const ProcessDescriptorActor = ActorClassWithSpec(processDescriptorSpec, {
  initialize(connection, options = {}) {
    if ("id" in options && typeof options.id != "number") {
      throw Error("process connect requires a valid `id` attribute.");
    }
    Actor.prototype.initialize.call(this, connection);
    this.id = options.id;
    this._windowGlobalTargetActor = null;
    this.isParent = options.parent;
    this.destroy = this.destroy.bind(this);
  },

  get browsingContextID() {
    if (this._windowGlobalTargetActor) {
      return this._windowGlobalTargetActor.docShell.browsingContext.id;
    }
    return null;
  },

  get isWindowlessParent() {
    return this.isParent && (this.isXpcshell || this.isBackgroundTaskMode);
  },

  get isXpcshell() {
    const env = Cc["@mozilla.org/process/environment;1"].getService(
      Ci.nsIEnvironment
    );
    return env.exists("XPCSHELL_TEST_PROFILE_DIR");
  },

  get isBackgroundTaskMode() {
    const bts = Cc["@mozilla.org/backgroundtasks;1"]?.getService(
      Ci.nsIBackgroundTasks
    );
    return bts && bts.isBackgroundTaskMode;
  },

  _parentProcessConnect({ isBrowserToolboxFission }) {
    let targetActor;
    if (this.isWindowlessParent) {
      // Check if we are running on xpcshell or in background task mode.
      // In these modes, there is no valid browsing context to attach to
      // and so ParentProcessTargetActor doesn't make sense as it inherits from
      // WindowGlobalTargetActor. So instead use ContentProcessTargetActor, which
      // matches the needs of these modes.
      targetActor = new ContentProcessTargetActor(this.conn, {
        isXpcShellTarget: true,
        sessionContext: createContentProcessSessionContext(),
      });
    } else {
      // Create the target actor for the parent process, which is in the same process
      // as this target. Because we are in the same process, we have a true actor that
      // should be managed by the ProcessDescriptorActor.
      targetActor = new ParentProcessTargetActor(this.conn, {
        // This target actor is special and will stay alive as long
        // as the toolbox/client is alive. It is the original top level target for
        // the BrowserToolbox and isTopLevelTarget should always be true here.
        // (It isn't the typical behavior of WindowGlobalTargetActor's base class)
        isTopLevelTarget: true,
        sessionContext: createBrowserSessionContext({
          isBrowserToolboxFission,
        }),
      });
      // this is a special field that only parent process with a browsing context
      // have, as they are the only processes at the moment that have child
      // browsing contexts
      this._windowGlobalTargetActor = targetActor;
    }
    this.manage(targetActor);
    // to be consistent with the return value of the _childProcessConnect, we are returning
    // the form here. This might be memoized in the future
    return targetActor.form();
  },

  /**
   * Connect to a remote process actor, always a ContentProcess target.
   */
  async _childProcessConnect() {
    const { id } = this;
    const mm = this._lookupMessageManager(id);
    if (!mm) {
      return {
        error: "noProcess",
        message: "There is no process with id '" + id + "'.",
      };
    }
    const childTargetForm = await connectToContentProcess(
      this.conn,
      mm,
      this.destroy
    );
    return childTargetForm;
  },

  _lookupMessageManager(id) {
    for (let i = 0; i < Services.ppmm.childCount; i++) {
      const mm = Services.ppmm.getChildAt(i);

      // A zero id is used for the parent process, instead of its actual pid.
      if (id ? mm.osPid == id : mm.isInProcess) {
        return mm;
      }
    }
    return null;
  },

  /**
   * Connect the a process actor.
   */
  async getTarget({ isBrowserToolboxFission }) {
    if (!DevToolsServer.allowChromeProcess) {
      return {
        error: "forbidden",
        message: "You are not allowed to debug processes.",
      };
    }
    if (this.isParent) {
      return this._parentProcessConnect({ isBrowserToolboxFission });
    }
    // This is a remote process we are connecting to
    return this._childProcessConnect();
  },

  /**
   * Return a Watcher actor, allowing to keep track of targets which
   * already exists or will be created. It also helps knowing when they
   * are destroyed.
   */
  getWatcher({ isBrowserToolboxFission }) {
    if (!this.watcher) {
      this.watcher = new WatcherActor(
        this.conn,
        createBrowserSessionContext({ isBrowserToolboxFission })
      );
      this.manage(this.watcher);
    }
    return this.watcher;
  },

  form() {
    return {
      actor: this.actorID,
      id: this.id,
      isParent: this.isParent,
      isWindowlessParent: this.isWindowlessParent,
      traits: {
        // Supports the Watcher actor. Can be removed as part of Bug 1680280.
        // Bug 1687461: WatcherActor only supports the parent process, where we debug everything.
        // For the "Browser Content Toolbox", where we debug only one content process,
        // we will still be using legacy listeners.
        watcher: this.isParent,
        // ParentProcessTargetActor can be reloaded.
        supportsReloadDescriptor: this.isParent && !this.isWindowlessParent,
      },
    };
  },

  async reloadDescriptor() {
    if (!this.isParent || this.isWindowlessParent) {
      throw new Error(
        "reloadDescriptor is only available for parent process descriptors"
      );
    }

    // Reload for the parent process will restart the whole browser
    //
    // This aims at replicate `DevelopmentHelpers.quickRestart`
    // This allows a user to do a full firefox restart + session restore
    // Via Ctrl+Alt+R on the Browser Console/Toolbox

    // Maximize the chance of fetching new source content by clearing the cache
    Services.obs.notifyObservers(null, "startupcache-invalidate");

    // Avoid safemode popup from appearing on restart
    const env = Cc["@mozilla.org/process/environment;1"].getService(
      Ci.nsIEnvironment
    );
    env.set("MOZ_DISABLE_SAFE_MODE_KEY", "1");

    Services.startup.quit(
      Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart
    );
  },

  destroy() {
    this.emit("descriptor-destroyed");

    this._windowGlobalTargetActor = null;
    Actor.prototype.destroy.call(this);
  },
});

exports.ProcessDescriptorActor = ProcessDescriptorActor;
