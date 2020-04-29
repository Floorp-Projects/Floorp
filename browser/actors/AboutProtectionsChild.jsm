/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["AboutProtectionsChild"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { RemotePageChild } = ChromeUtils.import(
  "resource://gre/actors/RemotePageChild.jsm"
);

class AboutProtectionsChild extends RemotePageChild {
  actorCreated() {
    super.actorCreated();

    this.exportFunctions(["RPMRecordTelemetryEvent"]);
  }

  RPMRecordTelemetryEvent(category, event, object, value, extra) {
    return Services.telemetry.recordEvent(
      category,
      event,
      object,
      value,
      extra
    );
  }
}
