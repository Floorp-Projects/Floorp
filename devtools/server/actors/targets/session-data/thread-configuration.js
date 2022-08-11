/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { STATES: THREAD_STATES } = require("devtools/server/actors/thread");

module.exports = {
  async addSessionDataEntry(targetActor, entries, isDocumentCreation) {
    const threadOptions = {};

    for (const { key, value } of entries) {
      threadOptions[key] = value;
    }

    if (
      !targetActor.targetType.endsWith("worker") &&
      targetActor.threadActor.state == THREAD_STATES.DETACHED
    ) {
      await targetActor.threadActor.attach(threadOptions);
    } else {
      await targetActor.threadActor.reconfigure(threadOptions);
    }
  },

  removeSessionDataEntry(targetActor, entries, isDocumentCreation) {
    // configuration data entries are always added/updated, never removed.
  },
};
