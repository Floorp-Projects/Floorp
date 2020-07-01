/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const { environmentSpec } = require("devtools/shared/specs/environment");

/**
 * Environment fronts are used to manipulate the lexical environment actors.
 *
 * @param {DevToolsClient} client: The devtools client parent.
 * @param {Front} targetFront: The target front this EnvironmentFront will be created for.
 * @param {Front} parentFront: The parent front of this EnvironmentFront.
 */
class EnvironmentFront extends FrontClassWithSpec(environmentSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);
    this._client = client;
  }

  get actor() {
    return this.actorID;
  }

  get _transport() {
    return this._client._transport;
  }
}

exports.EnvironmentFront = EnvironmentFront;
registerFront(EnvironmentFront);
