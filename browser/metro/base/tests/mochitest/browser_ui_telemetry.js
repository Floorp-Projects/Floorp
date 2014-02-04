// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function test() {
  runTests();
}

function getTelemetryPayload() {
  return Cu.import("resource://gre/modules/TelemetryPing.jsm", {}).
    TelemetryPing.getPayload();
}

gTests.push({
  desc: "Test browser-ui telemetry",
  run: function testBrowserUITelemetry() {
    // startup should have registered simple measures function
    is(getTelemetryPayload().info.appName, "MetroFirefox");

    let simpleMeasurements = getTelemetryPayload().simpleMeasurements;
    ok(simpleMeasurements, "simpleMeasurements are truthy");
    ok(simpleMeasurements.UITelemetry["metro-ui"]["window-width"], "window-width measurement was captured");
    ok(simpleMeasurements.UITelemetry["metro-ui"]["window-height"], "window-height measurement was captured");
  }
});
