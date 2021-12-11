/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that telemetry reports Firefox is not pinned on any OS at startup.
add_task(function check_startup_pinned_telemetry() {
  const scalars = TelemetryTestUtils.getProcessScalars("parent");

  // Check the appropriate telemetry is set or not reported by platform.
  switch (AppConstants.platform) {
    case "win":
      TelemetryTestUtils.assertScalar(
        scalars,
        "os.environment.is_taskbar_pinned",
        false,
        "Pin set on win"
      );
      TelemetryTestUtils.assertScalarUnset(
        scalars,
        "os.environment.is_kept_in_dock"
      );
      break;
    case "macosx":
      TelemetryTestUtils.assertScalarUnset(
        scalars,
        "os.environment.is_taskbar_pinned"
      );
      TelemetryTestUtils.assertScalar(
        scalars,
        "os.environment.is_kept_in_dock",
        false,
        "Dock set on mac"
      );
      break;
    default:
      TelemetryTestUtils.assertScalarUnset(
        scalars,
        "os.environment.is_taskbar_pinned"
      );
      TelemetryTestUtils.assertScalarUnset(
        scalars,
        "os.environment.is_kept_in_dock"
      );
      break;
  }
});
