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
    // Only WindowGlobalTargetActor implements updateTargetConfiguration,
    // skip targetActor data entry update for other targets.
    if (typeof targetActor.updateTargetConfiguration == "function") {
      const options = {};
      for (const { key, value } of entries) {
        options[key] = value;
      }
      // Regarding `updateType`, `entries` is always a partial set of configurations.
      // We will acknowledge the passed attribute, but if we had set some other attributes
      // before this call, they will stay as-is.
      // So it is as if this session data was also using "add" updateType.
      targetActor.updateTargetConfiguration(options, isDocumentCreation);
    }
  },

  removeSessionDataEntry(targetActor, entries, isDocumentCreation) {
    // configuration data entries are always added/updated, never removed.
  },
};
