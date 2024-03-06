/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  STATES: THREAD_STATES,
} = require("resource://devtools/server/actors/thread.js");
const Targets = require("resource://devtools/server/actors/targets/index.js");

module.exports = {
  async addOrSetSessionDataEntry(
    targetActor,
    entries,
    isDocumentCreation,
    updateType
  ) {
    // When debugging the whole browser (via the Browser Toolbox), we instantiate both content process and window global (FRAME) targets.
    // But the debugger will only use the content process target's thread actor.
    // Thread actor, Sources and Breakpoints have to be only managed for the content process target,
    // and we should explicitly ignore the window global target.
    if (
      targetActor.sessionContext.type == "all" &&
      targetActor.targetType === Targets.TYPES.FRAME &&
      targetActor.typeName != "parentProcessTarget"
    ) {
      return;
    }
    const { threadActor } = targetActor;
    if (updateType == "set") {
      threadActor.removeAllBreakpoints();
    }
    const isTargetCreation = threadActor.state == THREAD_STATES.DETACHED;
    if (isTargetCreation) {
      // If addOrSetSessionDataEntry is called during target creation, attach the
      // thread actor automatically and pass the initial breakpoints.
      await threadActor.attach({ breakpoints: entries });
    } else {
      // If addOrSetSessionDataEntry is called for an existing target, set the new
      // breakpoints on the already running thread actor.
      await Promise.all(
        entries.map(({ location, options }) =>
          threadActor.setBreakpoint(location, options)
        )
      );
    }
  },

  removeSessionDataEntry(targetActor, entries) {
    for (const { location } of entries) {
      targetActor.threadActor.removeBreakpoint(location);
    }
  },
};
