"use strict";

const SCALAR_BUILDID_MISMATCH = "dom.contentprocess.buildID_mismatch";

add_task(async function test_aboutRestartRequired() {
  let CrashHandlers = {};
  ChromeUtils.import(
    "resource:///modules/ContentCrashHandlers.jsm",
    CrashHandlers
  );

  // Let's reset the counts.
  Services.telemetry.clearScalars();

  let scalars = TelemetryTestUtils.getProcessScalars("parent");

  // Check preconditions
  is(
    scalars[SCALAR_BUILDID_MISMATCH],
    undefined,
    "Build ID mismatch count should be undefined"
  );

  // Simulate buildID mismatch
  CrashHandlers.TabCrashHandler._crashedTabCount = 1;
  CrashHandlers.TabCrashHandler.sendToRestartRequiredPage(
    gBrowser.selectedTab.linkedBrowser
  );

  scalars = TelemetryTestUtils.getProcessScalars("parent");

  is(
    scalars[SCALAR_BUILDID_MISMATCH],
    1,
    "Build ID mismatch count should be 1."
  );
});
