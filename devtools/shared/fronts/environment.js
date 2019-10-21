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
 * @param client DebuggerClient
 *        The debugger client parent.
 * @param form Object
 *        The form sent across the remote debugging protocol.
 */
class EnvironmentFront extends FrontClassWithSpec(environmentSpec) {
  constructor(client, form) {
    super(client, form);
    this._client = client;
    if (form) {
      this._form = form;
      this.actorID = form.actor;
      this.manage(this);
    }
  }

  get actor() {
    return this._form.actor;
  }
  get _transport() {
    return this._client._transport;
  }

  /**
   * Fetches the bindings introduced by this lexical environment.
   */
  getBindings() {
    return super.bindings();
  }
}

exports.EnvironmentFront = EnvironmentFront;
registerFront(EnvironmentFront);
