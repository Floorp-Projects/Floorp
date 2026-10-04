// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  applyFloorpIPProtectionPref,
  FIREFOX_IP_PROTECTION_BLOCK_CALLOUTS_PREF,
  FIREFOX_IP_PROTECTION_ENABLED_PREF,
  FLOORP_IP_PROTECTION_EXPERIMENT,
  isFloorpIPProtectionVariantEnabled,
  resolveFloorpIPProtectionEnabled,
  shouldEnableFloorpIPProtection,
} from "../FloorpIPProtectionGate.sys.mts";

import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";

function testOnlyEnabledVariantEnablesFeature(): void {
  assertEquals(
    isFloorpIPProtectionVariantEnabled("enabled"),
    true,
    "enabled variant should enable IP Protection",
  );
  assertEquals(
    isFloorpIPProtectionVariantEnabled("control"),
    false,
    "control variant should disable IP Protection",
  );
  assertEquals(
    isFloorpIPProtectionVariantEnabled("disabled"),
    false,
    "unknown variants should disable IP Protection",
  );
  assertEquals(
    isFloorpIPProtectionVariantEnabled(null),
    false,
    "missing assignment should disable IP Protection",
  );
}

function testReadsExpectedFlascoId(): void {
  const requestedIds: string[] = [];
  const enabled = shouldEnableFloorpIPProtection({
    getVariant(experimentId: string): string | null {
      requestedIds.push(experimentId);
      return "enabled";
    },
  });

  assertEquals(enabled, true, "enabled Flasco should enable IP Protection");
  assertEquals(
    JSON.stringify(requestedIds),
    JSON.stringify([FLOORP_IP_PROTECTION_EXPERIMENT]),
    "gate should read the expected Flasco id",
  );
}

function testFailsClosedWhenExperimentReadFails(): void {
  const enabled = shouldEnableFloorpIPProtection({
    getVariant(): string | null {
      throw new Error("manifest unavailable");
    },
  });

  assertEquals(
    enabled,
    false,
    "experiment read failures should disable IP Protection",
  );
}

function testAppliesFirefoxIpProtectionPref(): void {
  const writes: Array<[string, boolean]> = [];
  applyFloorpIPProtectionPref(
    {
      setBoolPref(prefName: string, value: boolean): void {
        writes.push([prefName, value]);
      },
    },
    false,
  );

  assertEquals(
    JSON.stringify(writes),
    JSON.stringify([
      [FIREFOX_IP_PROTECTION_ENABLED_PREF, false],
      [FIREFOX_IP_PROTECTION_BLOCK_CALLOUTS_PREF, true],
    ]),
    "gate should write the Firefox IP Protection and callout prefs",
  );
}

function testRuntimeFailureDisablesFeature(): void {
  assertEquals(
    resolveFloorpIPProtectionEnabled(true, true),
    true,
    "enabled Flasco and a ready runtime should enable IP Protection",
  );
  assertEquals(
    resolveFloorpIPProtectionEnabled(true, false),
    false,
    "runtime adapter failure should fail closed",
  );
  assertEquals(
    resolveFloorpIPProtectionEnabled(false, true),
    false,
    "a ready runtime should not override a disabled Flasco",
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "only enabled variant enables feature",
      fn: testOnlyEnabledVariantEnablesFeature,
    },
    { name: "reads expected Flasco id", fn: testReadsExpectedFlascoId },
    {
      name: "fails closed when experiment read fails",
      fn: testFailsClosedWhenExperimentReadFails,
    },
    {
      name: "applies Firefox IP Protection pref",
      fn: testAppliesFirefoxIpProtectionPref,
    },
    {
      name: "runtime adapter failures disable the feature",
      fn: testRuntimeFailureDisablesFeature,
    },
  ];

  await runTests("FloorpIPProtectionGate.test.mts", tests);
}
