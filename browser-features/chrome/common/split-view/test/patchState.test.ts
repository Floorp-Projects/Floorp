// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { createPatchState } from "../patches/patch-state.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

function testCreatePatchStateDefaults(): void {
  const state = createPatchState();

  assertEquals(
    state.inSplitViewPanelsSet,
    false,
    "inSplitViewPanelsSet should default to false",
  );
  assertEquals(
    state.inShowSplitViewPanels,
    false,
    "inShowSplitViewPanels should default to false",
  );
  assertEquals(state.lastPanelIds, "", "lastPanelIds should default to empty");
  assertEquals(
    state.multibarSetBySplitView,
    false,
    "multibarSetBySplitView should default to false",
  );
}

function testCreatePatchStateReturnsIndependentObjects(): void {
  const first = createPatchState();
  const second = createPatchState();

  first.lastPanelIds = "panel-a,panel-b";
  first.inSplitViewPanelsSet = true;

  assert(first !== second, "createPatchState should create a new object");
  assertEquals(
    second.lastPanelIds,
    "",
    "mutating first object should not affect second",
  );
  assertEquals(
    second.inSplitViewPanelsSet,
    false,
    "flags should not leak between instances",
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "createPatchState defaults", fn: testCreatePatchStateDefaults },
    {
      name: "createPatchState returns independent objects",
      fn: testCreatePatchStateReturnsIndependentObjects,
    },
  ];

  await runTests("patchState.test.ts", tests);
}
