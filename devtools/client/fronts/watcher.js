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
    const front = new BrowsingContextTargetFront(this.conn, null, this);
    front.actorID = form.actor;
    front.form(form);
    this.manage(front);
    this.emit("target-available", front);
  }

  _onTargetDestroyed(form) {
    const front = this.actor(form.actor);
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
    return null;
  }
}
registerFront(WatcherFront);
