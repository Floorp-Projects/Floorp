// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../chrome/test/utils/test_harness.ts";
import { resolveContainerLabel } from "./container-label.ts";

// Gecko 159: built-in identities have neither `name` nor `l10nId`.
const serviceLabels: Record<number, string> = {
  1: "Personal",
  2: "Work",
  7: "Custom QA",
};
const getLabel = (id: number): string => serviceLabels[id] ?? "";

function testBuiltInWithoutNameOrL10nIdUsesService(): void {
  assertEquals(
    resolveContainerLabel({ userContextId: 2 }, getLabel),
    "Work",
    "built-in container must be labelled by getUserContextLabel",
  );
}

function testUserCreatedUsesService(): void {
  assertEquals(
    resolveContainerLabel({ userContextId: 7, name: "Custom QA" }, getLabel),
    "Custom QA",
    "user-created container",
  );
}

function testFallsBackToStoredName(): void {
  assertEquals(
    resolveContainerLabel(
      { userContextId: 6, name: "Private Container" },
      () => "",
    ),
    "Private Container",
    "stored name when the service returns nothing",
  );
}

function testEmptyWhenNothingKnown(): void {
  assertEquals(
    resolveContainerLabel({ userContextId: 9 }, () => ""),
    "",
    "empty when neither label nor name exists",
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "built-in container without name/l10nId uses the service label",
      fn: testBuiltInWithoutNameOrL10nIdUsesService,
    },
    { name: "user-created container", fn: testUserCreatedUsesService },
    { name: "falls back to stored name", fn: testFallsBackToStoredName },
    { name: "empty when nothing is known", fn: testEmptyWhenNothingKnown },
  ];
  await runTests("container-label.test.ts", tests);
}
