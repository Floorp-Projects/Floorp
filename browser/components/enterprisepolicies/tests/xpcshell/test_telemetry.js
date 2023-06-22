/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);
const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);

add_task(async function test_telemetry_basic() {
  await setupPolicyEngineWithJson({
    policies: {
      DisableAboutSupport: true,
    },
  });

  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("parent"),
    "policies.is_enterprise",
    true
  );
});

add_task(async function test_telemetry_just_roots() {
  await setupPolicyEngineWithJson({
    policies: {
      Certificates: {
        ImportEnterpriseRoots: true,
      },
    },
  });

  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("parent"),
    "policies.is_enterprise",
    AppConstants.IS_ESR
  );
});

add_task(async function test_telemetry_roots_plus_policy() {
  await setupPolicyEngineWithJson({
    policies: {
      DisableAboutSupport: true,
      Certificates: {
        ImportEnterpriseRoots: true,
      },
    },
  });

  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("parent"),
    "policies.is_enterprise",
    true
  );
});

add_task(async function test_telemetry_esr() {
  await setupPolicyEngineWithJson({});
  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("parent"),
    "policies.is_enterprise",
    AppConstants.IS_ESR
  );
});

add_task(async function test_telemetry_esr_mac_eol() {
  Services.prefs
    .getDefaultBranch(null)
    .setCharPref("distribution.id", "mozilla-mac-eol-esr115");
  await setupPolicyEngineWithJson({});
  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("parent"),
    "policies.is_enterprise",
    false
  );
});

add_task(async function test_telemetry_esr_win_eol() {
  Services.prefs
    .getDefaultBranch(null)
    .setCharPref("distribution.id", "mozilla-win-eol-esr115");
  await setupPolicyEngineWithJson({});
  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("parent"),
    "policies.is_enterprise",
    false
  );
});

add_task(async function test_telemetry_esr_distro() {
  Services.prefs
    .getDefaultBranch(null)
    .setCharPref("distribution.id", "any-other-distribution-id");
  await setupPolicyEngineWithJson({});
  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("parent"),
    "policies.is_enterprise",
    AppConstants.IS_ESR
  );
});
