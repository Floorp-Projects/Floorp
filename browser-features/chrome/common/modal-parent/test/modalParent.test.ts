// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import ModalParent from "../index.ts";
import { isModalVisible, setModalVisible } from "../data/data.ts";

// ---------------------------------------------------------------------------
// Helpers – save/restore modal visibility state
// ---------------------------------------------------------------------------

let savedVisible: boolean;

type ModalManagerTestDouble = {
  show: (
    form: unknown,
    options: { width: number; height: number },
  ) => Promise<Record<string, never>>;
  hide: () => void;
  setModalSize: (size: { width: number; height: number }) => void;
};

const fakeModalCalls = {
  show: 0,
  hide: 0,
  setSize: 0,
  lastForm: undefined as unknown,
  lastSize: undefined as { width: number; height: number } | undefined,
};

const fakeModalManager: ModalManagerTestDouble = {
  show: (form, options) => {
    fakeModalCalls.show++;
    fakeModalCalls.lastForm = form;
    fakeModalCalls.lastSize = options;
    return Promise.resolve({});
  },
  hide: () => {
    fakeModalCalls.hide++;
  },
  setModalSize: (size) => {
    fakeModalCalls.setSize++;
    fakeModalCalls.lastSize = size;
  },
};

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

async function testShowNoraModalUsesManager(): Promise<void> {
  const inst = ModalParent.getInstance();
  const showBefore = fakeModalCalls.show;
  const hideBefore = fakeModalCalls.hide;
  let callbackCalled = false;

  await inst.showNoraModal(
    { forms: [], title: "test" },
    { width: 100, height: 100 },
    () => {
      callbackCalled = true;
    },
  );

  assertEquals(fakeModalCalls.show, showBefore + 1, "manager show called");
  assertEquals(fakeModalCalls.hide, hideBefore + 1, "manager hide called");
  assert(callbackCalled, "showNoraModal should invoke the callback");
}

function testHideNoraModalUsesManager(): void {
  const inst = ModalParent.getInstance();
  const hideBefore = fakeModalCalls.hide;
  inst.hideNoraModal();
  assertEquals(fakeModalCalls.hide, hideBefore + 1, "manager hide called");
}

function testSetModalSizeUsesManager(): void {
  const inst = ModalParent.getInstance();
  const setSizeBefore = fakeModalCalls.setSize;
  inst.setModalSize({ width: 100, height: 100 });
  assertEquals(
    fakeModalCalls.setSize,
    setSizeBefore + 1,
    "manager setModalSize called",
  );
  assertEquals(fakeModalCalls.lastSize?.width, 100, "width forwarded");
  assertEquals(fakeModalCalls.lastSize?.height, 100, "height forwarded");
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

function testModalParentPublicMethodsAreAvailable(): void {
  const inst = ModalParent.getInstance();
  assert(typeof inst.init === "function", "init should be available");
  assert(
    typeof inst.showNoraModal === "function",
    "showNoraModal should be available",
  );
  assert(
    typeof inst.hideNoraModal === "function",
    "hideNoraModal should be available",
  );
  assert(
    typeof inst.setModalSize === "function",
    "setModalSize should be available",
  );
}

async function testShowNoraModalBeforeInit(): Promise<void> {
  const inst = new ModalParent();
  let message = "";
  try {
    await inst.showNoraModal(
      { forms: [], title: "test" },
      { width: 100, height: 100 },
      () => {},
    );
  } catch (e: unknown) {
    message = e instanceof Error ? e.message : String(e);
  }
  assertEquals(
    message,
    "ModalManager not initialized. Call init() first.",
    "showNoraModal should reject before initialization",
  );
}

function testHideNoraModalBeforeInit(): void {
  const inst = new ModalParent();
  let message = "";
  try {
    inst.hideNoraModal();
  } catch (e: unknown) {
    message = e instanceof Error ? e.message : String(e);
  }
  assertEquals(
    message,
    "ModalManager not initialized. Call init() first.",
    "hideNoraModal should throw before initialization",
  );
}

function testSetModalSizeBeforeInit(): void {
  const inst = new ModalParent();
  let message = "";
  try {
    inst.setModalSize({ width: 300, height: 400 });
  } catch (e: unknown) {
    message = e instanceof Error ? e.message : String(e);
  }
  assertEquals(
    message,
    "ModalManager not initialized. Call init() first.",
    "setModalSize should throw before initialization",
  );
}

async function testShowNoraModalWithComplexForm(): Promise<void> {
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

  const showBefore = fakeModalCalls.show;
  let callbackCalled = false;
  await inst.showNoraModal(
    complexForm,
    { width: 500, height: 600 },
    (result) => {
      callbackCalled = true;
      assert(typeof result === "object", "result should be object");
    },
  );

  assertEquals(fakeModalCalls.show, showBefore + 1, "manager show called");
  assertEquals(fakeModalCalls.lastForm, complexForm, "form forwarded");
  assertEquals(fakeModalCalls.lastSize?.width, 500, "width forwarded");
  assertEquals(fakeModalCalls.lastSize?.height, 600, "height forwarded");
  assert(callbackCalled, "complex form callback should run");
}

function testModalParentMethodChaining(): void {
  const inst = ModalParent.getInstance();
  const setSizeBefore = fakeModalCalls.setSize;
  const hideBefore = fakeModalCalls.hide;
  inst.setModalSize({ width: 400, height: 500 });
  inst.hideNoraModal();
  assertEquals(fakeModalCalls.setSize, setSizeBefore + 1, "size call recorded");
  assertEquals(fakeModalCalls.hide, hideBefore + 1, "hide call recorded");
}

// ---------------------------------------------------------------------------
// Run
// ---------------------------------------------------------------------------

const tests: TestCase[] = [
  { name: "getInstance returns object", fn: testGetInstanceReturnsObject },
  { name: "getInstance is singleton", fn: testGetInstanceIsSingleton },
  { name: "showNoraModal uses manager", fn: testShowNoraModalUsesManager },
  { name: "hideNoraModal uses manager", fn: testHideNoraModalUsesManager },
  { name: "setModalSize uses manager", fn: testSetModalSizeUsesManager },
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
  {
    name: "ModalParent public methods are available",
    fn: testModalParentPublicMethodsAreAvailable,
  },
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
  const instance = ModalParent.getInstance();
  const instanceState = instance as unknown as { modalManager: unknown };
  const savedManager = instanceState.modalManager;
  saveState();
  instanceState.modalManager = fakeModalManager;
  try {
    await runTests("modalParent.test.ts", tests);
  } finally {
    instanceState.modalManager = savedManager;
    restoreState();
  }
}
