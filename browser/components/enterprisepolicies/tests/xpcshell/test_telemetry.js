/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
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
