/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { FrontClassWithSpec, Front } = require("devtools/shared/protocol");
const { perfSpec } = require("devtools/shared/specs/perf");

exports.PerfFront = FrontClassWithSpec(perfSpec, {
  initialize: function (client, form) {
    Front.prototype.initialize.call(this, client, form);
    this.actorID = form.perfActor;
    this.manage(this);
  }
});
