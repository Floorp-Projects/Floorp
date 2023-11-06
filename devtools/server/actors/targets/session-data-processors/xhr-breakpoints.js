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
    if (updateType == "set") {
      threadActor.removeAllXHRBreakpoints();
    }

    // The thread actor has to be initialized in order to correctly
    // retrieve the stack trace when hitting an XHR
    if (
      threadActor.state == THREAD_STATES.DETACHED &&
      !targetActor.targetType.endsWith("worker")
    ) {
      await threadActor.attach();
    }

    await Promise.all(
      entries.map(({ path, method }) =>
        threadActor.setXHRBreakpoint(path, method)
      )
    );
  },

  removeSessionDataEntry(targetActor, entries, isDocumentCreation) {
    for (const { path, method } of entries) {
      targetActor.threadActor.removeXHRBreakpoint(path, method);
    }
  },
};
