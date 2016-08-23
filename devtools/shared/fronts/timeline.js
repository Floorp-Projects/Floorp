/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Front,
  FrontClassWithSpec,
} = require("devtools/shared/protocol");
const { timelineSpec } = require("devtools/shared/specs/timeline");

/**
 * TimelineFront, the front for the TimelineActor.
 */
const TimelineFront = FrontClassWithSpec(timelineSpec, {
  initialize: function (client, { timelineActor }) {
    Front.prototype.initialize.call(this, client, { actor: timelineActor });
    this.manage(this);
  },
  destroy: function () {
    Front.prototype.destroy.call(this);
  },
});

exports.TimelineFront = TimelineFront;
