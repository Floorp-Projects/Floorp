/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { DevToolsServer } = require("devtools/server/devtools-server");
const { Cc, Ci } = require("chrome");

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
    this._browsingContextTargetActor = null;
    this.isParent = options.parent;
    this.destroy = this.destroy.bind(this);
  },

  get browsingContextID() {
    if (this._browsingContextTargetActor) {
      return this._browsingContextTargetActor.docShell.browsingContext.id;
    }
    return null;
  },

  _parentProcessConnect() {
    const env = Cc["@mozilla.org/process/environment;1"].getService(
      Ci.nsIEnvironment
    );
    const isXpcshell = env.exists("XPCSHELL_TEST_PROFILE_DIR");
    let targetActor;
    if (isXpcshell) {
      // Check if we are running on xpcshell.
      // When running on xpcshell, there is no valid browsing context to attach to
      // and so ParentProcessTargetActor doesn't make sense as it inherits from
      // BrowsingContextTargetActor. So instead use ContentProcessTargetActor, which
      // matches xpcshell needs.
      targetActor = new ContentProcessTargetActor(this.conn);
    } else {
      // Create the target actor for the parent process, which is in the same process
      // as this target. Because we are in the same process, we have a true actor that
      // should be managed by the ProcessDescriptorActor.
      targetActor = new ParentProcessTargetActor(this.conn, {
        // This BrowsingContextTargetActor is special and will stay alive as long
        // as the toolbox/client is alive. It is the original top level target for
        // the BrowserToolbox and isTopLevelTarget should always be true here.
        isTopLevelTarget: true,
      });
      // this is a special field that only parent process with a browsing context
      // have, as they are the only processes at the moment that have child
      // browsing contexts
      this._browsingContextTargetActor = targetActor;
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
  async getTarget() {
    if (!DevToolsServer.allowChromeProcess) {
      return {
        error: "forbidden",
        message: "You are not allowed to debug processes.",
      };
    }
    if (this.isParent) {
      return this._parentProcessConnect();
    }
    // This is a remote process we are connecting to
    return this._childProcessConnect();
  },

  /**
   * Return a Watcher actor, allowing to keep track of targets which
   * already exists or will be created. It also helps knowing when they
   * are destroyed.
   */
  getWatcher() {
    if (!this.watcher) {
      this.watcher = new WatcherActor(this.conn);
      this.manage(this.watcher);
    }
    return this.watcher;
  },

  form() {
    return {
      actor: this.actorID,
      id: this.id,
      isParent: this.isParent,
      traits: {
        // Supports the Watcher actor. Can be removed as part of Bug 1680280.
        watcher: true,
      },
    };
  },

  destroy() {
    this._browsingContextTargetActor = null;
    Actor.prototype.destroy.call(this);
  },
});

exports.ProcessDescriptorActor = ProcessDescriptorActor;
