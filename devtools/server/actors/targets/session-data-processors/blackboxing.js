/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.exports = {
  async addOrSetSessionDataEntry(
    targetActor,
    entries,
    isDocumentCreation,
    updateType
  ) {
    const { sourcesManager } = targetActor;
    if (updateType == "set") {
      sourcesManager.clearAllBlackBoxing();
    }
    for (const { url, range } of entries) {
      sourcesManager.blackBox(url, range);
    }
  },

  removeSessionDataEntry(targetActor, entries, isDocumentCreation) {
    for (const { url, range } of entries) {
      targetActor.sourcesManager.unblackBox(url, range);
    }
  },
};
