/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { reflowSpec } = require("devtools/shared/specs/reflow");
const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");

/**
 * Usage example of the reflow front:
 *
 * let front = await target.getFront("reflow");
 * front.on("reflows", this._onReflows);
 * front.start();
 * // now wait for events to come
 */
class ReflowFront extends FrontClassWithSpec(reflowSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    // Attribute name from which to retrieve the actorID out of the target actor's form
    this.formAttributeName = "reflowActor";
  }
}

exports.ReflowFront = ReflowFront;
registerFront(ReflowFront);
