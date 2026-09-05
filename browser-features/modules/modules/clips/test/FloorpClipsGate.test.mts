// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  applyFloorpClipsPref,
  FLOORP_CLIPS_ENABLED_PREF,
  FLOORP_CLIPS_EXPERIMENT,
  isFloorpClipsVariantEnabled,
  readFloorpClipsVariant,
  resolveFloorpClipsEnabled,
} from "../FloorpClipsGate.sys.mts";

import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";

function testOnlyEnabledVariantEnablesClips(): void {
  assertEquals(isFloorpClipsVariantEnabled("enabled"), true, "enabled variant turns Clips on");
  assertEquals(isFloorpClipsVariantEnabled("control"), false, "control keeps Clips off");
  assertEquals(isFloorpClipsVariantEnabled(null), false, "no variant is not enabled");
}

function testNoAssignmentKeepsCurrentValue(): void {
  assertEquals(resolveFloorpClipsEnabled(null, true), true, "unassigned: a hand-set true survives");
  assertEquals(resolveFloorpClipsEnabled(null, false), false, "unassigned: default false survives");
  assertEquals(resolveFloorpClipsEnabled("control", true), false, "control wins over a hand-set true");
  assertEquals(resolveFloorpClipsEnabled("enabled", false), true, "enabled wins over the default");
}

function testReadsExpectedFlascoId(): void {
  let asked: string | null = null;
  const variant = readFloorpClipsVariant({
    getVariant(id) {
      asked = id;
      return "enabled";
    },
  });
  assertEquals(asked, FLOORP_CLIPS_EXPERIMENT, "asks for the floorp_clips Flasco");
  assertEquals(variant, "enabled", "hands the variant back");
}

function testFailsClosedWhenExperimentReadFails(): void {
  const variant = readFloorpClipsVariant({
    getVariant() {
      throw new Error("boom");
    },
  });
  assertEquals(variant, null, "a failing read counts as no assignment");
}

function testWritesThePref(): void {
  const bools: Record<string, boolean> = {};
  const prefs = {
    getBoolPref: (n: string, d: boolean) => bools[n] ?? d,
    setBoolPref: (n: string, v: boolean) => {
      bools[n] = v;
    },
  };
  applyFloorpClipsPref(prefs, true);
  assertEquals(bools[FLOORP_CLIPS_ENABLED_PREF], true, "writes true");
  applyFloorpClipsPref(prefs, false);
  assertEquals(bools[FLOORP_CLIPS_ENABLED_PREF], false, "writes false");
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "only enabled variant enables Clips", fn: testOnlyEnabledVariantEnablesClips },
    { name: "no assignment keeps the current value", fn: testNoAssignmentKeepsCurrentValue },
    { name: "reads expected Flasco id", fn: testReadsExpectedFlascoId },
    { name: "fails closed when experiment read fails", fn: testFailsClosedWhenExperimentReadFails },
    { name: "writes the pref", fn: testWritesThePref },
  ];
  await runTests("FloorpClipsGate.test.mts", tests);
}
