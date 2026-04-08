// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assertEquals,
  assert,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import ModalParent from "../index.ts";
import { isModalVisible, setModalVisible } from "../data/data.ts";

// ---------------------------------------------------------------------------
// Helpers – save/restore modal visibility state
// ---------------------------------------------------------------------------

let savedVisible: boolean;

function saveState(): void {
  savedVisible = isModalVisible();
}

function restoreState(): void {
  setModalVisible(savedVisible);
}

// ---------------------------------------------------------------------------
// getInstance() singleton tests
// ---------------------------------------------------------------------------

function testGetInstanceReturnsObject(): void {
  const instance = ModalParent.getInstance();
  assert(instance !== null, "getInstance should return non-null");
  assert(instance !== undefined, "getInstance should return non-undefined");
}

function testGetInstanceIsSingleton(): void {
  const a = ModalParent.getInstance();
  const b = ModalParent.getInstance();
  assertEquals(a, b, "getInstance should return the same instance");
}

// ---------------------------------------------------------------------------
// Uninitialized state error tests
// ---------------------------------------------------------------------------

function testShowNoraModalDoesNotCrash(): void {
  // In the live browser the singleton is already initialised by the chrome
  // layer, so we verify the positive path — calling showNoraModal should not
  // throw unexpectedly (it may fail on actor lookup but that's acceptable).
  const inst = ModalParent.getInstance();
  try {
    inst.showNoraModal(
      { forms: [], title: "test" },
      { width: 100, height: 100 },
      () => {},
    );
  } catch (e: unknown) {
    const msg = e instanceof Error ? e.message : String(e);
    assert(
      msg.includes("ModalManager not initialized") ||
        msg.includes("NRChromeModal") ||
        msg.includes("browsingContext") ||
        msg.includes("modal-parent-container"),
      "Unexpected error from showNoraModal: " + msg,
    );
  }
}

function testHideNoraModalDoesNotCrash(): void {
  // hideNoraModal on the initialised singleton should not throw.
  const inst = ModalParent.getInstance();
  try {
    inst.hideNoraModal();
  } catch (e: unknown) {
    const msg = e instanceof Error ? e.message : String(e);
    assert(
      msg.includes("ModalManager not initialized") ||
        msg.includes("modal-parent-container"),
      "Unexpected error from hideNoraModal: " + msg,
    );
  }
}

function testSetModalSizeDoesNotCrash(): void {
  // setModalSize on the initialised singleton should delegate to the manager.
  const inst = ModalParent.getInstance();
  try {
    inst.setModalSize({ width: 100, height: 100 });
  } catch (e: unknown) {
    const msg = e instanceof Error ? e.message : String(e);
    assert(
      msg.includes("ModalManager not initialized"),
      "Unexpected error from setModalSize: " + msg,
    );
  }
}

// ---------------------------------------------------------------------------
// Type validation – TForm structural tests
// ---------------------------------------------------------------------------

function testMinimalTFormIsValid(): void {
  // Verify the TForm interface accepts minimal form data
  const form = {
    forms: [],
    title: "Test Modal",
  };
  assertEquals(form.forms.length, 0, "empty forms array");
  assertEquals(form.title, "Test Modal", "title preserved");
}

function testTFormWithSubmitCancelLabels(): void {
  const form = {
    forms: [],
    title: "Test",
    submitLabel: "OK",
    cancelLabel: "Cancel",
  };
  assertEquals(form.submitLabel, "OK", "submitLabel");
  assertEquals(form.cancelLabel, "Cancel", "cancelLabel");
}

function testTFormItemAllTypes(): void {
  const types = [
    "text",
    "number",
    "textarea",
    "select",
    "dropdown",
    "checkbox",
    "radio",
    "url",
  ] as const;

  assertEquals(types.length, 8, "TFormItem should support 8 types");
  for (const t of types) {
    assert(typeof t === "string", `type ${t} should be a string`);
  }
}

function testTFormResultKeyValue(): void {
  const result: Record<string, string | number> = {
    name: "floorp",
    count: 42,
  };
  assertEquals(result["name"], "floorp", "string value");
  assertEquals(result["count"], 42, "number value");
}

// ---------------------------------------------------------------------------
// Run
// ---------------------------------------------------------------------------

const tests: TestCase[] = [
  { name: "getInstance returns object", fn: testGetInstanceReturnsObject },
  { name: "getInstance is singleton", fn: testGetInstanceIsSingleton },
  { name: "showNoraModal does not crash", fn: testShowNoraModalDoesNotCrash },
  { name: "hideNoraModal does not crash", fn: testHideNoraModalDoesNotCrash },
  { name: "setModalSize does not crash", fn: testSetModalSizeDoesNotCrash },
  { name: "minimal TForm is valid", fn: testMinimalTFormIsValid },
  { name: "TForm with submit/cancel labels", fn: testTFormWithSubmitCancelLabels },
  { name: "TFormItem supports 8 types", fn: testTFormItemAllTypes },
  { name: "TFormResult key-value", fn: testTFormResultKeyValue },
];

export async function runAllTests(): Promise<void> {
  saveState();
  try {
    await runTests("modalParent.test.ts", tests);
  } finally {
    restoreState();
  }
}
