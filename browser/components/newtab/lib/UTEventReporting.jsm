/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

/**
 * Note: the schema can be found in
 * https://searchfox.org/mozilla-central/source/toolkit/components/telemetry/Events.yaml
 */
const EXTRAS_FIELD_NAMES = [
  "addon_version",
  "session_id",
  "page",
  "user_prefs",
  "action_position",
];

this.UTEventReporting = class UTEventReporting {
  constructor() {
    Services.telemetry.setEventRecordingEnabled("activity_stream", true);
    this.sendUserEvent = this.sendUserEvent.bind(this);
    this.sendSessionEndEvent = this.sendSessionEndEvent.bind(this);
    this.sendTrailheadEnrollEvent = this.sendTrailheadEnrollEvent.bind(this);
  }

  _createExtras(data) {
    // Make a copy of the given data and delete/modify it as needed.
    let utExtras = Object.assign({}, data);
    for (let field of Object.keys(utExtras)) {
      if (EXTRAS_FIELD_NAMES.includes(field)) {
        utExtras[field] = String(utExtras[field]);
        continue;
      }
      delete utExtras[field];
    }
    return utExtras;
  }

  sendUserEvent(data) {
    let mainFields = ["event", "source"];
    let eventFields = mainFields.map(field => String(data[field]) || null);

    Services.telemetry.recordEvent(
      "activity_stream",
      "event",
      ...eventFields,
      this._createExtras(data)
    );
  }

  sendSessionEndEvent(data) {
    Services.telemetry.recordEvent(
      "activity_stream",
      "end",
      "session",
      String(data.session_duration),
      this._createExtras(data)
    );
  }

  sendTrailheadEnrollEvent(data) {
    Services.telemetry.recordEvent(
      "activity_stream",
      "enroll",
      "preference_study",
      data.experiment,
      {
        experimentType: data.type,
        branch: data.branch,
      }
    );
  }

  uninit() {
    Services.telemetry.setEventRecordingEnabled("activity_stream", false);
  }
};

const EXPORTED_SYMBOLS = ["UTEventReporting"];
