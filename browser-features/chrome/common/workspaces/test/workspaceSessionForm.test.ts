// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { createWorkspaceSessionFormItem } from "../workspace-modal.tsx";
import type {
  WorkspaceSessionFormLabels,
  WorkspaceSessionOption,
} from "../utils/workspace-modal-types.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

const labels: WorkspaceSessionFormLabels = {
  label: "Login environment",
  standard: "Standard",
  shareCurrent: "Share Work",
  newSession: "Create another account environment",
  chooseSession: "Choose an available environment",
};

const sessions: readonly WorkspaceSessionOption[] = [
  { name: "Work", userContextId: 4 },
  { name: "Personal", userContextId: 9 },
];

function testCreationSharesProvidedDefault(): void {
  const form = createWorkspaceSessionFormItem(4, sessions, labels, "create");
  assertEquals(form.value, "4", "creation shares the supplied default");
  assertEquals(form.options?.[0].value, "4", "shared choice is first");
  assertEquals(
    form.options?.[0].label,
    "Share Work",
    "shared choice names the captured environment",
  );
  assertEquals(
    form.options?.filter((option) => option.value === "4").length,
    1,
    "shared session is not duplicated among other choices",
  );
  assert(
    form.options?.some((option) => option.value === "new"),
    "separate account creation remains an explicit choice",
  );
}

function testStandardRemainsAnExplicitSelection(): void {
  const form = createWorkspaceSessionFormItem(0, sessions, {
    ...labels,
    shareCurrent: "Share Standard",
  }, "create");
  assertEquals(form.value, "0", "standard is not an empty selection");
  assertEquals(form.options?.[0].value, "0", "standard sharing is first");
  assertEquals(form.required, true, "an environment must be selected");
}

function testMissingSessionRequiresSelection(): void {
  for (const mode of ["create", "edit"] as const) {
    const form = createWorkspaceSessionFormItem(77, sessions, labels, mode);
    assertEquals(form.value, "", `${mode}: no silent standard fallback`);
    assertEquals(
      form.options?.[0].value,
      "",
      `${mode}: prompt remains the selected option`,
    );
    assertEquals(form.required, true, `${mode}: choosing is required`);
    assert(
      !form.options?.some((option) => option.value === "77"),
      `${mode}: missing environment is not selectable`,
    );
    assert(
      form.options?.some((option) => option.value === "0"),
      `${mode}: standard is still available by explicit choice`,
    );
  }
}

function testEditingKeepsCurrentAndOffersNew(): void {
  const form = createWorkspaceSessionFormItem(9, sessions, labels, "edit");
  assertEquals(form.value, "9", "editing keeps the existing environment");
  assertEquals(
    form.options?.find((option) => option.value === "9")?.label,
    "Personal",
    "editing uses the session name",
  );
  assert(
    form.options?.some((option) => option.value === "new"),
    "editing can explicitly create another account environment",
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "creation shares the supplied default",
      fn: testCreationSharesProvidedDefault,
    },
    {
      name: "standard remains explicit",
      fn: testStandardRemainsAnExplicitSelection,
    },
    {
      name: "missing environment requires selection",
      fn: testMissingSessionRequiresSelection,
    },
    {
      name: "editing keeps current and offers new",
      fn: testEditingKeepsCurrentAndOffersNew,
    },
  ];
  await runTests("workspaceSessionForm.test.ts", tests);
}
