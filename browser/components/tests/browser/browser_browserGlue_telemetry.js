/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that telemetry reports Firefox is not pinned on any OS at startup.
add_task(function check_startup_pinned_telemetry() {
  const scalars = TelemetryTestUtils.getProcessScalars("parent");

  // Check the appropriate telemetry is set or not reported by platform.
  switch (AppConstants.platform) {
    case "win":
      if (
        AppConstants.platform === "win" &&
        Services.sysinfo.getProperty("hasWinPackageId")
      ) {
        TelemetryTestUtils.assertScalarUnset(
          scalars,
          "os.environment.is_taskbar_pinned"
        );
        TelemetryTestUtils.assertScalarUnset(
          scalars,
          "os.environment.is_taskbar_pinned_private"
        );
      } else {
        TelemetryTestUtils.assertScalar(
          scalars,
          "os.environment.is_taskbar_pinned",
          false,
          "Pin set on win"
        );
        TelemetryTestUtils.assertScalar(
          scalars,
          "os.environment.is_taskbar_pinned_private",
          false,
          "Pin private set on win"
        );
      }
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
      TelemetryTestUtils.assertScalarUnset(
        scalars,
        "os.environment.is_taskbar_pinned_private"
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
        "os.environment.is_taskbar_pinned_private"
      );
      TelemetryTestUtils.assertScalarUnset(
        scalars,
        "os.environment.is_kept_in_dock"
      );
      break;
  }
});

// Check that telemetry reports whether Firefox is the default PDF handler.
// This is safe without any explicit coordination because idle tasks are
// guaranteed to have been invokedbefore the test harness invokes the test.  See
// https://searchfox.org/mozilla-central/rev/1674b86019a96f076e0f98f1d0f5f3ab9d4e9020/browser/components/BrowserGlue.jsm#2320-2324
// and
// https://searchfox.org/mozilla-central/rev/1674b86019a96f076e0f98f1d0f5f3ab9d4e9020/browser/base/content/browser.js#2364.
add_task(function check_is_default_handler_telemetry() {
  const scalars = TelemetryTestUtils.getProcessScalars("parent", true);

  // Check the appropriate telemetry is set or not reported by platform.
  switch (AppConstants.platform) {
    case "win":
      // We should always set whether we're the default PDF handler.
      Assert.ok("os.environment.is_default_handler" in scalars);
      Assert.deepEqual(
        [".pdf"],
        Object.keys(scalars["os.environment.is_default_handler"])
      );

      if (Cu.isInAutomation) {
        // But only in automation can we assume we're not the default handler.
        TelemetryTestUtils.assertKeyedScalar(
          scalars,
          "os.environment.is_default_handler",
          ".pdf",
          false,
          "Not default PDF handler on Windows"
        );
      }
      break;
    default:
      TelemetryTestUtils.assertScalarUnset(
        scalars,
        "os.environment.is_default_handler"
      );
      break;
  }
});
