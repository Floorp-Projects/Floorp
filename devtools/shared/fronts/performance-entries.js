/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Front, FrontClassWithSpec } = require("devtools/shared/protocol");
const performanceSpec = require("devtools/shared/specs/performance-entries");

var PerformanceEntriesFront = FrontClassWithSpec(performanceSpec, {
  initialize: function (client, form) {
    Front.prototype.initialize.call(this, client);
    this.actorID = form.performanceEntriesActor;
    this.manage(this);
  },
});

exports.PerformanceEntriesFront = PerformanceEntriesFront;
