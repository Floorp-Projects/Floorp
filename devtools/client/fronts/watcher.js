/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { watcherSpec } = require("devtools/shared/specs/watcher");
const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");

loader.lazyRequireGetter(
  this,
  "BrowsingContextTargetFront",
  "devtools/client/fronts/targets/browsing-context",
  true
);
loader.lazyRequireGetter(
  this,
  "ContentProcessTargetFront",
  "devtools/client/fronts/targets/content-process",
  true
);
loader.lazyRequireGetter(
  this,
  "WorkerTargetFront",
  "devtools/client/fronts/targets/worker",
  true
);

class WatcherFront extends FrontClassWithSpec(watcherSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    this._onTargetAvailable = this._onTargetAvailable.bind(this);
    this._onTargetDestroyed = this._onTargetDestroyed.bind(this);

    // Convert form, which is just JSON object to Fronts for these two events
    this.on("target-available-form", this._onTargetAvailable);
    this.on("target-destroyed-form", this._onTargetDestroyed);
  }

  form(json) {
    this.actorID = json.actor;
    this.traits = json.traits;
  }

  _onTargetAvailable(form) {
    let front;
    if (form.actor.includes("/contentProcessTarget")) {
      front = new ContentProcessTargetFront(this.conn, null, this);
    } else if (form.actor.includes("/workerTarget")) {
      front = new WorkerTargetFront(this.conn, null, this);
    } else {
      front = new BrowsingContextTargetFront(this.conn, null, this);
    }
    front.actorID = form.actor;
    front.form(form);
    this.manage(front);
    this.emit("target-available", front);
  }

  _onTargetDestroyed(form) {
    let front = this.getActorByID(form.actor);
    // For top level target, the target will be a child of the descriptor front,
    // which happens to be the parent front of the watcher.
    if (!front) {
      front = this.parentFront.getActorByID(form.actor);
    }
    this.emit("target-destroyed", front);
  }

  /**
   * Retrieve the already existing BrowsingContextTargetFront for the parent
   * BrowsingContext of the given BrowsingContext ID.
   */
  async getParentBrowsingContextTarget(browsingContextID) {
    const id = await this.getParentBrowsingContextID(browsingContextID);
    if (!id) {
      return null;
    }
    return this.getBrowsingContextTarget(id);
  }

  /**
   * Memoized getter for the "breakpoint-list" actor
   */
  async getBreakpointListActor() {
    if (!this._breakpointListActor) {
      this._breakpointListActor = await super.getBreakpointListActor();
    }
    return this._breakpointListActor;
  }

  /**
   * Memoized getter for the "target-configuration" actor
   */
  async getTargetConfigurationActor() {
    if (!this._targetConfigurationActor) {
      this._targetConfigurationActor = await super.getTargetConfigurationActor();
    }
    return this._targetConfigurationActor;
  }

  /**
   * For a given BrowsingContext ID, return the already existing BrowsingContextTargetFront
   */
  async getBrowsingContextTarget(id) {
    // First scan the watcher children as the watcher manages all the targets
    for (const front of this.poolChildren()) {
      if (front.browsingContextID == id) {
        return front;
      }
    }
    // But the top level target will be created by the Descriptor.getTarget() method
    // and so be hosted in the Descriptor's pool.
    // The parent front of the WatcherActor happens to be the Descriptor Actor.
    // This code could go away or be simplified if the Descriptor starts fetch all
    // the targets, including the top level one via the Watcher. i.e. drop Descriptor.getTarget().
    const topLevelTarget = await this.parentFront.getTarget();
    if (topLevelTarget.browsingContextID == id) {
      return topLevelTarget;
    }

    // If we could not find a browsing context target for the provided id, the
    // browsing context might not be the topmost browsing context of a given
    // process. For now we only create targets for the top browsing context of
    // each process, so we recursively check the parent browsing context ids
    // until we find a valid target.
    const parentBrowsingContextID = await this.getParentBrowsingContextID(id);
    if (parentBrowsingContextID && parentBrowsingContextID !== id) {
      return this.getBrowsingContextTarget(parentBrowsingContextID);
    }

    return null;
  }

  /**
   * Memoized getter for the "networkParent" actor
   */
  async getNetworkParentActor() {
    if (!this._networkParentActor) {
      this._networkParentActor = await super.getNetworkParentActor();
    }
    return this._networkParentActor;
  }
}
registerFront(WatcherFront);
