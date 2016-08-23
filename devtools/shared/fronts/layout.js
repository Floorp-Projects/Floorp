/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {reflowSpec} = require("devtools/shared/specs/layout");
const protocol = require("devtools/shared/protocol");

/**
 * Usage example of the reflow front:
 *
 * let front = ReflowFront(toolbox.target.client, toolbox.target.form);
 * front.on("reflows", this._onReflows);
 * front.start();
 * // now wait for events to come
 */
const ReflowFront = protocol.FrontClassWithSpec(reflowSpec, {
  initialize: function (client, {reflowActor}) {
    protocol.Front.prototype.initialize.call(this, client, {actor: reflowActor});
    this.manage(this);
  },

  destroy: function () {
    protocol.Front.prototype.destroy.call(this);
  },
});

exports.ReflowFront = ReflowFront;
