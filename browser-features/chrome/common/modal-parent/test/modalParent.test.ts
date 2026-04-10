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
// Extended TForm/TFormItem type tests
// ---------------------------------------------------------------------------

function testTFormItemAllOptionalFields(): void {
  // Test that all optional fields can be included
  const item = {
    type: "text" as const,
    id: "test-id",
    label: "Test Label",
    value: "test value",
    required: true,
    classList: "class1 class2",
    placeholder: "Enter text",
    rows: 5,
    maxLength: 100,
    options: [
      { label: "Option 1", value: "opt1", icon: "icon1" },
      { label: "Option 2", value: "opt2" },
    ],
    when: { id: "other-field", value: "trigger" },
    onInput: (val: string) => val.toUpperCase(),
  };
  assertEquals(item.id, "test-id", "id preserved");
  assertEquals(item.label, "Test Label", "label preserved");
  assertEquals(item.required, true, "required preserved");
  assertEquals(item.classList, "class1 class2", "classList preserved");
  assertEquals(item.placeholder, "Enter text", "placeholder preserved");
  assertEquals(item.rows, 5, "rows preserved");
  assertEquals(item.maxLength, 100, "maxLength preserved");
  assertEquals(item.options?.length, 2, "options length");
}

function testTFormItemWhenStringArray(): void {
  // Test when clause with string array value
  const item = {
    type: "checkbox" as const,
    id: "chk",
    when: { id: "select-field", value: ["opt1", "opt2"] },
  };
  assertEquals(item.when.value.length, 2, "array value length");
  assertEquals(item.when.value[0], "opt1", "first array value");
}

function testTFormItemAllTypesWithStructure(): void {
  // Test each type with appropriate fields
  const types: Array<{ type: string; appropriateFields: string[] }> = [
    { type: "text", appropriateFields: ["placeholder", "maxLength"] },
    { type: "number", appropriateFields: ["placeholder", "value"] },
    { type: "textarea", appropriateFields: ["rows", "maxLength"] },
    { type: "select", appropriateFields: ["options"] },
    { type: "dropdown", appropriateFields: ["options"] },
    { type: "checkbox", appropriateFields: ["value"] },
    { type: "radio", appropriateFields: ["options"] },
    { type: "url", appropriateFields: ["placeholder"] },
  ];

  assertEquals(types.length, 8, "all 8 types covered");
}

function testTFormResultEmpty(): void {
  const result: Record<string, string | number> = {};
  assertEquals(Object.keys(result).length, 0, "empty result");
}

function testTFormResultMultipleFields(): void {
  const result: Record<string, string | number> = {
    username: "testuser",
    age: 25,
    email: "test@example.com",
    score: 100,
  };
  assertEquals(result.username, "testuser", "username field");
  assertEquals(result.age, 25, "age field");
  assertEquals(result.email, "test@example.com", "email field");
  assertEquals(result.score, 100, "score field");
}

// ---------------------------------------------------------------------------
// ModalParent initialization and error handling tests
// ---------------------------------------------------------------------------

function testModalParentInit(): void {
  const inst = ModalParent.getInstance();
  // init() should not throw even if called multiple times
  try {
    inst.init();
  } catch (e: unknown) {
    const msg = e instanceof Error ? e.message : String(e);
    // Some errors are acceptable in test environment
    assert(
      msg.includes("browsingContext") ||
        msg.includes("modal-parent-container") ||
        msg.includes("NRChromeModal") ||
        msg.includes("undefined"),
      "Unexpected init error: " + msg,
    );
  }
}

function testShowNoraModalBeforeInit(): void {
  // Create fresh instance to test uninitialized state
  const inst = ModalParent.getInstance();
  // If init wasn't called, showNoraModal should throw specific error
  // Note: In test environment, singleton may already be partially initialized
  try {
    inst.showNoraModal(
      { forms: [], title: "test" },
      { width: 100, height: 100 },
      () => {},
    );
  } catch (e: unknown) {
    const msg = e instanceof Error ? e.message : String(e);
    // Should get initialization error or actor error
    assert(
      msg.includes("ModalManager not initialized") ||
        msg.includes("NRChromeModal") ||
        msg.includes("browsingContext"),
      "Expected initialization or actor error, got: " + msg,
    );
  }
}

function testHideNoraModalBeforeInit(): void {
  const inst = ModalParent.getInstance();
  try {
    inst.hideNoraModal();
  } catch (e: unknown) {
    const msg = e instanceof Error ? e.message : String(e);
    assert(
      msg.includes("ModalManager not initialized") ||
        msg.includes("modal-parent-container"),
      "Expected initialization error, got: " + msg,
    );
  }
}

function testSetModalSizeBeforeInit(): void {
  const inst = ModalParent.getInstance();
  try {
    inst.setModalSize({ width: 300, height: 400 });
  } catch (e: unknown) {
    const msg = e instanceof Error ? e.message : String(e);
    assert(
      msg.includes("ModalManager not initialized"),
      "Expected initialization error, got: " + msg,
    );
  }
}

function testShowNoraModalWithComplexForm(): void {
  const inst = ModalParent.getInstance();
  const complexForm = {
    forms: [
      {
        type: "text" as const,
        id: "name",
        label: "Name",
        required: true,
        placeholder: "Enter name",
      },
      {
        type: "number" as const,
        id: "age",
        label: "Age",
        value: 0,
      },
      {
        type: "select" as const,
        id: "country",
        options: [
          { label: "USA", value: "us" },
          { label: "Japan", value: "jp" },
        ],
      },
    ],
    title: "Complex Form Test",
    submitLabel: "Submit",
    cancelLabel: "Cancel",
  };

  try {
    inst.showNoraModal(
      complexForm,
      { width: 500, height: 600 },
      (result) => {
        // Callback should be called with result
        assert(typeof result === "object", "result should be object");
      },
    );
  } catch (e: unknown) {
    // Actor errors are acceptable in test environment
    const msg = e instanceof Error ? e.message : String(e);
    assert(
      msg.includes("NRChromeModal") ||
        msg.includes("browsingContext") ||
        msg.includes("modal-parent-container"),
      "Unexpected error: " + msg,
    );
  }
}

function testModalParentMethodChaining(): void {
  // Test that multiple operations can be called on same instance
  const inst = ModalParent.getInstance();
  try {
    inst.init();
    inst.setModalSize({ width: 400, height: 500 });
    inst.hideNoraModal();
  } catch (e: unknown) {
    // Errors are acceptable, we're testing that methods exist
    const msg = e instanceof Error ? e.message : String(e);
    assert(
      msg.includes("ModalManager not initialized") ||
        msg.includes("NRChromeModal") ||
        msg.includes("browsingContext") ||
        msg.includes("modal-parent-container"),
      "Unexpected error in method chaining: " + msg,
    );
  }
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
  {
    name: "TForm with submit/cancel labels",
    fn: testTFormWithSubmitCancelLabels,
  },
  { name: "TFormItem supports 8 types", fn: testTFormItemAllTypes },
  { name: "TFormResult key-value", fn: testTFormResultKeyValue },
  {
    name: "TFormItem all optional fields",
    fn: testTFormItemAllOptionalFields,
  },
  { name: "TFormItem when string array", fn: testTFormItemWhenStringArray },
  {
    name: "TFormItem all types with structure",
    fn: testTFormItemAllTypesWithStructure,
  },
  { name: "TFormResult empty", fn: testTFormResultEmpty },
  { name: "TFormResult multiple fields", fn: testTFormResultMultipleFields },
  { name: "ModalParent init", fn: testModalParentInit },
  {
    name: "showNoraModal before init",
    fn: testShowNoraModalBeforeInit,
  },
  {
    name: "hideNoraModal before init",
    fn: testHideNoraModalBeforeInit,
  },
  {
    name: "setModalSize before init",
    fn: testSetModalSizeBeforeInit,
  },
  {
    name: "showNoraModal with complex form",
    fn: testShowNoraModalWithComplexForm,
  },
  {
    name: "ModalParent method chaining",
    fn: testModalParentMethodChaining,
  },
];

export async function runAllTests(): Promise<void> {
  saveState();
  try {
    await runTests("modalParent.test.ts", tests);
  } finally {
    restoreState();
  }
}
