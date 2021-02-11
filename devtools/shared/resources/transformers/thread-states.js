/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Front, types } = require("devtools/shared/protocol.js");

module.exports = function({ resource, watcherFront, targetFront }) {
  // only "paused" have a frame attribute, and legacy listeners are already passing a FrameFront
  if (resource.frame && !(resource.frame instanceof Front)) {
    resource.frame = types.getType("frame").read(resource.frame, targetFront);
  }

  return resource;
};
