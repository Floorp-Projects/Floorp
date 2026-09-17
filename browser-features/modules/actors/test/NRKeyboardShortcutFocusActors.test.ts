// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";
import {
  isKeyboardShortcutEditableFocusEvent,
  isKeyboardShortcutEditableTarget,
} from "../NRKeyboardShortcutFocusChild.sys.mts";
import { applyKeyboardShortcutFocusUpdate } from "../NRKeyboardShortcutFocusParent.sys.mts";
import { NRKeyboardShortcutFocusStore } from "../../common/NRKeyboardShortcutFocusStore.ts";

function requireDocument(): Document {
  if (!document) {
    throw new Error("document is unavailable in this test context");
  }
  return document;
}

function testChildRecognizesInputTextareaAndContenteditable(): void {
  const doc = requireDocument();
  const input = doc.createElement("input");
  const textarea = doc.createElement("textarea");
  const editable = doc.createElement("div");
  editable.setAttribute("contenteditable", "true");
  const nested = doc.createElement("span");
  editable.appendChild(nested);

  for (const target of [input, textarea, editable, nested]) {
    assertEquals(
      isKeyboardShortcutEditableTarget(target, "off"),
      true,
      "input, textarea, and inherited contenteditable should be editable",
    );
  }
}

function testContenteditableFalseStopsInheritance(): void {
  const doc = requireDocument();
  const editable = doc.createElement("div");
  editable.setAttribute("contenteditable", "true");
  const disabled = doc.createElement("span");
  disabled.setAttribute("contenteditable", "false");
  editable.appendChild(disabled);
  assertEquals(
    isKeyboardShortcutEditableTarget(disabled, "off"),
    false,
    "contenteditable=false should stop inherited editability",
  );
}

function testDesignModeIsEditable(): void {
  const doc = requireDocument();
  assertEquals(
    isKeyboardShortcutEditableTarget(doc.body, "on"),
    true,
    "designMode=on should mark the focused document editable",
  );
}

function testComposedFocusPathFindsShadowEditable(): void {
  const doc = requireDocument();
  const input = doc.createElement("input");
  const event = {
    composedPath: () => [input],
  } as unknown as Event;
  assertEquals(
    isKeyboardShortcutEditableFocusEvent(event, doc),
    true,
    "the composed focus path should expose a shadow-tree editable",
  );
}

function testParentAggregatesAndClearsFrameMessages(): void {
  const store = new NRKeyboardShortcutFocusStore();
  const browser = {};
  const frame = {};

  applyKeyboardShortcutFocusUpdate(store, frame, browser, { editable: true });
  assertEquals(
    store.isEditableFocused(browser),
    true,
    "a true child update should mark the top browser editable",
  );
  applyKeyboardShortcutFocusUpdate(store, frame, browser, { editable: false });
  assertEquals(
    store.isEditableFocused(browser),
    false,
    "a false child update should remove the frame",
  );
}

function testMissingEmbedderAndMalformedMessagesFailOpen(): void {
  const store = new NRKeyboardShortcutFocusStore();
  const browser = {};
  const frame = {};

  applyKeyboardShortcutFocusUpdate(store, frame, browser, { editable: true });
  applyKeyboardShortcutFocusUpdate(store, frame, null, { editable: true });
  assertEquals(
    store.isEditableFocused(browser),
    false,
    "a missing top embedder should remove stale frame state",
  );

  applyKeyboardShortcutFocusUpdate(store, frame, browser, { editable: true });
  applyKeyboardShortcutFocusUpdate(store, frame, browser, { editable: "yes" });
  assertEquals(
    store.isEditableFocused(browser),
    false,
    "malformed actor data should remove stale state",
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "child recognizes input textarea and contenteditable",
      fn: testChildRecognizesInputTextareaAndContenteditable,
    },
    {
      name: "contenteditable false stops inheritance",
      fn: testContenteditableFalseStopsInheritance,
    },
    { name: "designMode focus is editable", fn: testDesignModeIsEditable },
    {
      name: "composed path finds shadow editable",
      fn: testComposedFocusPathFindsShadowEditable,
    },
    {
      name: "parent aggregates and clears frame messages",
      fn: testParentAggregatesAndClearsFrameMessages,
    },
    {
      name: "missing embedder and malformed messages fail open",
      fn: testMissingEmbedderAndMalformedMessagesFailOpen,
    },
  ];
  await runTests("NRKeyboardShortcutFocusActors.test.ts", tests);
}
