/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Front, types } = require("devtools/shared/protocol.js");

module.exports = function({ resource, watcherFront, targetFront }) {
  // only "paused" have a frame attribute, and legacy listeners are already passing a FrameFront
  if (resource.frame && !(resource.frame instanceof Front)) {
    // Use ThreadFront as parent as debugger's commands.js expects FrameFront to be children
    // of the ThreadFront.
    resource.frame = types
      .getType("frame")
      .read(resource.frame, targetFront.threadFront);
  }

  // If we are using server side request (i.e. watcherFront is defined)
  // Fake paused and resumed events as the thread front depends on them.
  // We can't emit "EventEmitter" events, as ThreadFront uses `Front.before`
  // to listen for paused and resumed. ("before" is part of protocol.js Front and not part of EventEmitter)
  if (watcherFront) {
    if (resource.state == "paused") {
      targetFront.threadFront._beforePaused(resource);
    } else if (resource.state == "resumed") {
      targetFront.threadFront._beforeResumed(resource);
    }
  }

  return resource;
};
