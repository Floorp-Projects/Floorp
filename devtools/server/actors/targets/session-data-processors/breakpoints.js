/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { STATES: THREAD_STATES } = require("devtools/server/actors/thread");

module.exports = {
  async addSessionDataEntry(targetActor, entries, isDocumentCreation) {
    const isTargetCreation =
      targetActor.threadActor.state == THREAD_STATES.DETACHED;
    if (isTargetCreation && !targetActor.targetType.endsWith("worker")) {
      // If addSessionDataEntry is called during target creation, attach the
      // thread actor automatically and pass the initial breakpoints.
      // However, do not attach the thread actor for Workers. They use a codepath
      // which releases the worker on `attach`. For them, the client will call `attach`. (bug 1691986)
      await targetActor.threadActor.attach({ breakpoints: entries });
    } else {
      // If addSessionDataEntry is called for an existing target, set the new
      // breakpoints on the already running thread actor.
      await Promise.all(
        entries.map(({ location, options }) =>
          targetActor.threadActor.setBreakpoint(location, options)
        )
      );
    }
  },

  removeSessionDataEntry(targetActor, entries, isDocumentCreation) {
    for (const { location } of entries) {
      targetActor.threadActor.removeBreakpoint(location);
    }
  },
};
