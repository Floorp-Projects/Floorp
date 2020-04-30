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
}
registerFront(WatcherFront);
