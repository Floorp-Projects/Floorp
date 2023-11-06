/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  STATES: THREAD_STATES,
} = require("resource://devtools/server/actors/thread.js");

module.exports = {
  async addOrSetSessionDataEntry(
    targetActor,
    entries,
    isDocumentCreation,
    updateType
  ) {
    const { threadActor } = targetActor;
    // Same as comments for XHR breakpoints. See lines 117-118
    if (
      threadActor.state == THREAD_STATES.DETACHED &&
      !targetActor.targetType.endsWith("worker")
    ) {
      threadActor.attach();
    }
    if (updateType == "set") {
      threadActor.setActiveEventBreakpoints(entries);
    } else {
      threadActor.addEventBreakpoints(entries);
    }
  },

  removeSessionDataEntry(targetActor, entries, isDocumentCreation) {
    targetActor.threadActor.removeEventBreakpoints(entries);
  },
};
