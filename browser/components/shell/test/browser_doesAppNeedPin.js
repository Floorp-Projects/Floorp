/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);
const { NimbusFeatures } = ChromeUtils.import(
  "resource://nimbus/ExperimentAPI.jsm"
);

let defaultValue;
add_task(async function default_need() {
  defaultValue = await ShellService.doesAppNeedPin();
  Assert.ok(defaultValue !== undefined, "Got a default app need pin value");
});

add_task(async function remote_disable() {
  if (defaultValue === false) {
    info("Default pin already false, so nothing to test");
    return;
  }

  await ExperimentFakes.remoteDefaultsHelper({
    feature: NimbusFeatures.shellService,
    configuration: { variables: { disablePin: true } },
  });

  Assert.equal(
    await ShellService.doesAppNeedPin(),
    false,
    "Pinning disabled via nimbus"
  );
});

add_task(async function restore_default() {
  if (defaultValue === undefined) {
    info("No default pin value set, so nothing to test");
    return;
  }

  await ExperimentFakes.remoteDefaultsHelper({
    feature: NimbusFeatures.shellService,
    configuration: {},
  });

  Assert.equal(
    await ShellService.doesAppNeedPin(),
    defaultValue,
    "Pinning restored to original"
  );
});
