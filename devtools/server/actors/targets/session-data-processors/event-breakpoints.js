/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { STATES: THREAD_STATES } = require("devtools/server/actors/thread");

module.exports = {
  async addSessionDataEntry(targetActor, entries, isDocumentCreation) {
    // Same as comments for XHR breakpoints. See lines 117-118
    if (
      targetActor.threadActor.state == THREAD_STATES.DETACHED &&
      !targetActor.targetType.endsWith("worker")
    ) {
      targetActor.threadActor.attach();
    }
    targetActor.threadActor.addEventBreakpoints(entries);
  },

  removeSessionDataEntry(targetActor, entries, isDocumentCreation) {
    targetActor.threadActor.removeEventBreakpoints(entries);
  },
};
