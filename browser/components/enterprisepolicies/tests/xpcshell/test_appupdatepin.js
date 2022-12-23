/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

/**
 * Note that these tests only ensure that the pin is properly added to the
 * update URL and to the telemetry. They do not test that the update applied
 * will be of the correct version. This is because we are not attempting to have
 * Firefox check if the update provided is valid given the pin, we are leaving
 * it to the update server (Balrog) to find and serve the correct version.
 */

async function test_update_pin(pinString, pinIsValid = true) {
  await setupPolicyEngineWithJson({
    policies: {
      AppUpdateURL: "https://www.example.com/update.xml",
      AppUpdatePin: pinString,
    },
  });
  Services.telemetry.clearScalars();

  equal(
    Services.policies.status,
    Ci.nsIEnterprisePolicies.ACTIVE,
    "Engine is active"
  );

  let policies = Services.policies.getActivePolicies();
  equal(
    "AppUpdatePin" in policies,
    pinIsValid,
    "AppUpdatePin policy should only be active if the pin was valid."
  );

  let checker = Cc["@mozilla.org/updates/update-checker;1"].getService(
    Ci.nsIUpdateChecker
  );
  let updateURL = await checker.getUpdateURL(checker.BACKGROUND_CHECK);

  let expected = pinIsValid
    ? `https://www.example.com/update.xml?pin=${pinString}`
    : "https://www.example.com/update.xml";

  equal(updateURL, expected, "App Update URL should match expected URL.");

  let scalars = TelemetryTestUtils.getProcessScalars("parent", false, true);
  if (pinIsValid) {
    TelemetryTestUtils.assertScalar(
      scalars,
      "update.version_pin",
      pinString,
      "Update pin telemetry should be set"
    );
  } else {
    TelemetryTestUtils.assertScalarUnset(scalars, "update.version_pin");
  }
}

add_task(async function test_app_update_pin() {
  await test_update_pin("102.");
  await test_update_pin("102.0.");
  await test_update_pin("102.1.");
  await test_update_pin("102.1.1", false);
  await test_update_pin("102.1.1.", false);
  await test_update_pin("102", false);
  await test_update_pin("foobar", false);
  await test_update_pin("-102.1.", false);
  await test_update_pin("102.-1.", false);
  await test_update_pin("102a.1.", false);
  await test_update_pin("102.1a.", false);
  await test_update_pin("0102.1.", false);
  // Should not accept version numbers that will never be in Balrog's pinning
  // table (i.e. versions before 102.0).
  await test_update_pin("101.1.", false);
});
