/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that telemetry reports Firefox is not pinned on any OS at startup.
add_task(function check_startup_pinned_telemetry() {
  const scalars = TelemetryTestUtils.getProcessScalars("parent");
  const check = (key, val, msg) => Assert.strictEqual(scalars[key], val, msg);

  // Check the appropriate telemetry is set or not reported by platform.
  switch (AppConstants.platform) {
    case "win":
      check("os.environment.is_taskbar_pinned", false, "Pin set on win");
      check("os.environment.is_kept_in_dock", undefined, "Dock unset on win");
      break;
    case "macosx":
      check("os.environment.is_taskbar_pinned", undefined, "Pin unset on mac");
      check("os.environment.is_kept_in_dock", false, "Dock set on mac");
      break;
    default:
      check("os.environment.is_taskbar_pinned", undefined, "Pin unset on lin");
      check("os.environment.is_kept_in_dock", undefined, "Dock unset on lin");
      break;
  }
});
