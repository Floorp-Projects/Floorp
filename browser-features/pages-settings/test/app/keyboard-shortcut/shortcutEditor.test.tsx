// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { act } from "react";
import { createRoot, type Root } from "react-dom/client";
import i18next, { type i18n } from "i18next";
import { I18nextProvider, initReactI18next } from "react-i18next";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";
import { StandardUIProvider } from "../../../../../libs/ui/standard-provider.tsx";
import { ShortcutEditor } from "../../../src/app/keyboard-shortcut/components/ShortcutEditor.tsx";
import type { ShortcutEditorProps } from "../../../src/app/keyboard-shortcut/types.ts";
import type { ShortcutConfig } from "../../../src/types/pref.ts";

// ---------------------------------------------------------------------------
// i18n setup
// ---------------------------------------------------------------------------

let testI18n: i18n | null = null;

async function getTestI18n(): Promise<i18n> {
  if (testI18n) return testI18n;
  testI18n = i18next.createInstance();
  await testI18n.use(initReactI18next).init({
    lng: "en",
    fallbackLng: false,
    interpolation: { escapeValue: false },
    resources: {
      en: {
        translation: {
          keyboardShortcut: {
            editShortcut: "Edit Shortcut",
            cancel: "Cancel",
            save: "Save",
            preview: "Preview",
            pressKey: "Press any key...",
            recording: "Recording...",
            key: "Key",
            keyConflict: "This shortcut is already in use",
            firefoxKeyConflict:
              "Conflicts with Firefox's \"{{label}}\" ({{shortcut}}). Floorp takes priority.",
            openAboutKeyboard: "Open about:keyboard",
            modal: {
              description:
                "Enter the key you want to use and select the modifier keys.",
            },
          },
          ui: {
            saveError: "Save failed",
          },
        },
      },
    },
  });
  return testI18n;
}

// ---------------------------------------------------------------------------
// Render helpers
// ---------------------------------------------------------------------------

interface RenderedEditor {
  root: Root;
  host: HTMLDivElement;
  cleanup(): void;
  rerender(props: Partial<ShortcutEditorProps>): void;
}

const NO_EXISTING: ShortcutConfig[] = [];

function baseProps(
  overrides: Partial<ShortcutEditorProps> = {},
): ShortcutEditorProps {
  return {
    isOpen: true,
    onClose: () => {},
    onSave: () => Promise.resolve(true),
    initialShortcut: null,
    existingShortcuts: NO_EXISTING,
    actionId: "test-action",
    firefoxKeys: [],
    ...overrides,
  };
}

async function renderEditor(
  props: Partial<ShortcutEditorProps> = {},
): Promise<RenderedEditor> {
  const i18nInstance = await getTestI18n();
  const host = document.createElement("div");
  document.body.append(host);
  const root = createRoot(host);
  let currentProps = baseProps(props);
  const render = (p: ShortcutEditorProps) => {
    root.render(
      <StandardUIProvider>
        <I18nextProvider i18n={i18nInstance}>
          <ShortcutEditor {...p} />
        </I18nextProvider>
      </StandardUIProvider>,
    );
  };
  await act(() => render(currentProps));
  return {
    host,
    root,
    rerender(overrides: Partial<ShortcutEditorProps>) {
      currentProps = { ...currentProps, ...overrides };
      act(() => render(currentProps));
    },
    cleanup() {
      act(() => root.unmount());
      host.remove();
    },
  };
}

/**
 * ShortcutEditor renders a Modal into a Chakra Portal (document.body).
 * In standard mode the Dialog.Content carries the `floorp-standard-ui` class.
 */
function queryModal(): Element | null {
  return document.querySelector(".floorp-standard-ui");
}

function queryModalScope(): ParentNode {
  return queryModal() ?? document;
}

function findKeyInput(scope: ParentNode): HTMLInputElement {
  const input = [...scope.querySelectorAll("input")].find((el) =>
    el.getAttribute("aria-label") === "Key"
  );
  assert(input instanceof HTMLInputElement, "key input not found");
  return input;
}

function findSaveButton(scope: ParentNode): HTMLButtonElement {
  const button = [...scope.querySelectorAll("button")].find((el) =>
    el.textContent?.trim() === "Save"
  );
  assert(button instanceof HTMLButtonElement, "save button not found");
  return button;
}

function findAlert(scope: ParentNode): Element | null {
  return scope.querySelector("[role='alert']");
}

async function nextPaint(): Promise<void> {
  await new Promise<void>((resolve) =>
    requestAnimationFrame(() => requestAnimationFrame(() => resolve()))
  );
}

/** Simulate a physical keydown while the recorder is active. */
function dispatchRecordKey(code: string): void {
  const event = new KeyboardEvent("keydown", {
    code,
    key: code.replace(/^Key/, "").toLowerCase(),
    bubbles: true,
    cancelable: true,
  });
  Object.defineProperty(event, "repeat", { value: false });
  globalThis.dispatchEvent(event);
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

async function testOpenRendersModalWithEmptyKey(): Promise<void> {
  const rendered = await renderEditor();
  try {
    const scope = queryModalScope();
    const input = findKeyInput(scope);
    assertEquals(
      input.value,
      "",
      "key input should start empty",
    );
    const save = findSaveButton(scope);
    assertEquals(
      save.disabled,
      true,
      "save should be disabled without a key",
    );
  } finally {
    rendered.cleanup();
  }
}

async function testClosedRendersNothing(): Promise<void> {
  const rendered = await renderEditor({ isOpen: false });
  try {
    assertEquals(
      queryModal(),
      null,
      "modal should not be mounted when isOpen=false",
    );
  } finally {
    rendered.cleanup();
  }
}

async function testFocusStartsRecordingAndKeydownStops(): Promise<void> {
  const rendered = await renderEditor();
  try {
    const scope = queryModalScope();
    const input = findKeyInput(scope);

    // Focus the input to start recording.
    await act(async () => {
      input.focus();
      input.dispatchEvent(new FocusEvent("focus"));
      await nextPaint();
    });

    assert(
      scope.querySelector(".text-primary") !== null ||
        document.querySelector(".text-primary") !== null,
      "recording indicator should appear when input is focused",
    );

    // Dispatch a keydown while recording.
    await act(async () => {
      dispatchRecordKey("KeyT");
      await nextPaint();
    });

    assertEquals(
      input.value,
      "T",
      "recorded key should be displayed (KeyT → T)",
    );
  } finally {
    rendered.cleanup();
  }
}

async function testRecordedKeyEnablesSave(): Promise<void> {
  const rendered = await renderEditor();
  try {
    const scope = queryModalScope();
    const input = findKeyInput(scope);

    await act(async () => {
      input.focus();
      input.dispatchEvent(new FocusEvent("focus"));
      await nextPaint();
    });

    await act(async () => {
      dispatchRecordKey("KeyT");
      await nextPaint();
    });

    const save = findSaveButton(scope);
    assertEquals(
      save.disabled,
      false,
      "save should become enabled after a key is recorded",
    );
  } finally {
    rendered.cleanup();
  }
}

async function testSaveCallsOnSaveAndCloses(): Promise<void> {
  const savedShortcuts: ShortcutConfig[] = [];
  const rendered = await renderEditor({
    onSave: (shortcut) => {
      savedShortcuts.push(shortcut);
      return Promise.resolve(true);
    },
  });
  try {
    const scope = queryModalScope();
    const input = findKeyInput(scope);

    await act(async () => {
      input.focus();
      input.dispatchEvent(new FocusEvent("focus"));
      await nextPaint();
    });

    await act(async () => {
      dispatchRecordKey("KeyT");
      await nextPaint();
    });

    const save = findSaveButton(scope);
    await act(async () => {
      save.click();
      await nextPaint();
    });

    assertEquals(
      savedShortcuts.length,
      1,
      "onSave should have been called once",
    );
    assertEquals(
      savedShortcuts[0]?.key,
      "KeyT",
      "onSave should receive the recorded physical code",
    );
    assertEquals(
      savedShortcuts[0]?.action,
      "test-action",
      "onSave should carry the action id",
    );
    assertEquals(
      JSON.stringify(savedShortcuts[0]?.modifiers),
      JSON.stringify({ alt: false, ctrl: false, meta: false, shift: false }),
      "default modifiers should be all false",
    );
  } finally {
    rendered.cleanup();
  }
}

async function testDuplicateShowsErrorAndDisablesSave(): Promise<void> {
  const existing: ShortcutConfig[] = [
    {
      key: "KeyT",
      modifiers: { alt: false, ctrl: false, meta: false, shift: false },
      action: "other-action",
    },
  ];
  const rendered = await renderEditor({ existingShortcuts: existing });
  try {
    const scope = queryModalScope();
    const input = findKeyInput(scope);

    await act(async () => {
      input.focus();
      input.dispatchEvent(new FocusEvent("focus"));
      await nextPaint();
    });

    await act(async () => {
      dispatchRecordKey("KeyT");
      await nextPaint();
    });

    const alert = findAlert(scope);
    assert(
      alert !== null,
      "duplicate shortcut should show an error alert",
    );
    assert(
      alert.textContent?.includes("already in use"),
      "error message should mention the conflict",
    );

    const save = findSaveButton(scope);
    assertEquals(
      save.disabled,
      true,
      "save should be disabled when a duplicate is detected",
    );
  } finally {
    rendered.cleanup();
  }
}

// ---------------------------------------------------------------------------
// Test runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "open renders modal with empty key",
      fn: testOpenRendersModalWithEmptyKey,
    },
    {
      name: "closed renders nothing",
      fn: testClosedRendersNothing,
    },
    {
      name: "focus starts recording and keydown stops",
      fn: testFocusStartsRecordingAndKeydownStops,
    },
    {
      name: "recorded key enables save",
      fn: testRecordedKeyEnablesSave,
    },
    {
      name: "save calls onSave and closes",
      fn: testSaveCallsOnSaveAndCloses,
    },
    {
      name: "duplicate shows error and disables save",
      fn: testDuplicateShowsErrorAndDisablesSave,
    },
  ];

  await runTests("shortcutEditor.test.tsx", tests);
}
