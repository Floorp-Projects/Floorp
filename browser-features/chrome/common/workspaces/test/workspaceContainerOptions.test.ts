// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { createWorkspaceContainerOptions } from "../utils/container-options.ts";
import type { TWorkspaceContainerIdentity } from "../utils/type.ts";
import {
  assertEquals,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

const texts = {
  noContainer: "No Container",
  unavailableContainer: (id: number) => `Unavailable container (ID ${id})`,
};

// Gecko 159 identities: built-in containers carry neither `name` nor `l10nId`.
const geckoIdentities: TWorkspaceContainerIdentity[] = [
  { userContextId: 1 },
  { userContextId: 2 },
  { userContextId: 4 },
  { userContextId: 6, name: "Private Container" },
];

const geckoLabels: Record<number, string> = {
  1: "Personal",
  2: "Work",
  4: "Shopping",
  6: "Private Container",
};

const labelOf = (id: number): string => geckoLabels[id] ?? "";

function optionPairs(
  containers: readonly TWorkspaceContainerIdentity[],
  current: number,
  getLabel: (id: number) => string = labelOf,
): string[] {
  return createWorkspaceContainerOptions(containers, current, getLabel, texts)
    .map((option) => `${option.value}=${option.label}`);
}

function testBuiltInContainersUseServiceLabels(): void {
  assertEquals(
    optionPairs(geckoIdentities, 2).join("|"),
    "0=No Container|1=Personal|2=Work|4=Shopping|6=Private Container",
    "built-in containers without name/l10nId must be labelled by getUserContextLabel",
  );
}

function testNoOptionHasEmptyLabel(): void {
  const options = createWorkspaceContainerOptions(
    geckoIdentities,
    0,
    () => "",
    texts,
  );
  for (const option of options) {
    assertEquals(
      option.label.length > 0,
      true,
      `empty label for ${option.value}`,
    );
  }
  assertEquals(
    options.find((option) => option.value === "6")?.label,
    "Private Container",
    "falls back to the stored name when the service returns no label",
  );
  assertEquals(
    options.find((option) => option.value === "1")?.label,
    "Unavailable container (ID 1)",
    "falls back to the unavailable label when there is no name either",
  );
}

function testRemovedContainerReferenceIsKeptAsUnavailable(): void {
  assertEquals(
    optionPairs(geckoIdentities, 3).join("|"),
    "0=No Container|1=Personal|2=Work|4=Shopping|6=Private Container|3=Unavailable container (ID 3)",
    "a workspace pointing at a removed container keeps a labelled option for its value",
  );
}

function testExistingSelectionDoesNotAddExtraOption(): void {
  assertEquals(optionPairs(geckoIdentities, 0).length, 5, "no container");
  assertEquals(optionPairs(geckoIdentities, 6).length, 5, "existing container");
}

export function runAllTests(): void {
  const tests: TestCase[] = [
    {
      name: "built-in containers use service labels",
      fn: testBuiltInContainersUseServiceLabels,
    },
    { name: "no option has an empty label", fn: testNoOptionHasEmptyLabel },
    {
      name: "removed container reference is kept as unavailable",
      fn: testRemovedContainerReferenceIsKeptAsUnavailable,
    },
    {
      name: "existing selection does not add an extra option",
      fn: testExistingSelectionDoesNotAddExtraOption,
    },
  ];

  const failures: string[] = [];
  for (const test of tests) {
    try {
      test.fn();
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      failures.push(`${test.name}: ${message}`);
    }
  }
  if (failures.length > 0) {
    throw new Error(
      `workspaceContainerOptions.test.ts failures: ${failures.join(" | ")}`,
    );
  }
}
