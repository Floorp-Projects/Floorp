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
      // Regarding `updateType`, `entries` is always a partial set of configurations.
      // We will acknowledge the passed attribute, but if we had set some other attributes
      // before this call, they will stay as-is.
      // So it is as if this session data was also using "add" updateType.
      await targetActor.threadActor.reconfigure(threadOptions);
    }
  },

  removeSessionDataEntry(targetActor, entries, isDocumentCreation) {
    // configuration data entries are always added/updated, never removed.
  },
};
