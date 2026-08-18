// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { StyleElement } from "../styleElem.tsx";
import {
  attachZenModeToWindow,
  destroyZenModeForWindow,
  ZEN_MODE_STYLE_ID,
} from "../zen-mode.tsx";
import { render } from "@nora/solid-xul";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

function createDisposableWindow(): Window {
  const doc = document.implementation.createHTMLDocument(
    "Zen style ownership test",
  );
  const eventTarget = new EventTarget();
  const toolbox = doc.createElement("div");
  toolbox.id = "navigator-toolbox";
  doc.body!.appendChild(toolbox);

  return {
    document: doc,
    closed: false,
    innerWidth: 1000,
    innerHeight: 800,
    MutationObserver,
    ResizeObserver,
    requestAnimationFrame: globalThis.requestAnimationFrame.bind(globalThis),
    cancelAnimationFrame: globalThis.cancelAnimationFrame.bind(globalThis),
    setTimeout: globalThis.setTimeout.bind(globalThis),
    clearTimeout: globalThis.clearTimeout.bind(globalThis),
    addEventListener: eventTarget.addEventListener.bind(eventTarget),
    removeEventListener: eventTarget.removeEventListener.bind(eventTarget),
    gNavToolbox: toolbox,
  } as unknown as Window;
}

function testStyleElementReturnsNode(): void {
  const node = StyleElement();
  assert(node !== null, "StyleElement should return a JSX node");
  assertEquals(
    typeof node,
    "object",
    "StyleElement result should be object-like",
  );
}

function testRenderedStyleContainsZenModeSelector(): void {
  const head = document?.head;
  assert(head !== null && head !== undefined, "document.head should exist");

  render(() => StyleElement(), head);
  const styleNodes = head.querySelectorAll("style");
  const latestStyle = styleNodes.item(styleNodes.length - 1);

  try {
    assert(latestStyle !== null, "render should insert a style element");
    assert(
      (latestStyle.textContent ?? "").includes("#zen-mode-button"),
      "rendered style should include #zen-mode-button selector",
    );
  } finally {
    latestStyle?.remove();
  }
}

function testStyleElementContainsValidSVGIcon(): void {
  const head = document?.head;
  assert(head !== null && head !== undefined, "document.head should exist");

  render(() => StyleElement(), head);
  const styleNodes = head.querySelectorAll("style");
  const latestStyle = styleNodes.item(styleNodes.length - 1);

  try {
    const content = latestStyle?.textContent ?? "";
    assert(
      content.includes("data:image/svg+xml"),
      "style should contain SVG data URI",
    );
    assert(
      content.includes("xmlns='http://www.w3.org/2000/svg"),
      "SVG should have valid xmlns attribute",
    );
    assert(
      content.includes("viewBox='0 0 16 16'"),
      "SVG should have correct viewBox",
    );
  } finally {
    latestStyle?.remove();
  }
}

function testStyleElementTargetsCorrectButtonId(): void {
  const head = document?.head;
  assert(head !== null && head !== undefined, "document.head should exist");

  render(() => StyleElement(), head);
  const styleNodes = head.querySelectorAll("style");
  const latestStyle = styleNodes.item(styleNodes.length - 1);

  try {
    const content = latestStyle?.textContent ?? "";
    assert(
      content.includes("#zen-mode-button"),
      "style should target #zen-mode-button selector",
    );
    assert(
      content.includes("list-style-image"),
      "style should include list-style-image property",
    );
  } finally {
    latestStyle?.remove();
  }
}

function testControllerOwnsSingleZenStylePerWindow(): void {
  const testWindow = createDisposableWindow();
  const first = attachZenModeToWindow(testWindow);

  try {
    const second = attachZenModeToWindow(testWindow);
    assert(first !== null, "the test window should have a Zen controller");
    assertEquals(
      second,
      first,
      "the controller registry should be idempotent",
    );
    assertEquals(
      testWindow.document!.querySelectorAll(`#${ZEN_MODE_STYLE_ID}`).length,
      1,
      "Zen behavior CSS should be owned once per window, independent of menu rendering",
    );
  } finally {
    destroyZenModeForWindow(testWindow, first ?? undefined);
  }
}

function testStyleElementSVGContainsZenModeIcon(): void {
  const head = document?.head;
  assert(head !== null && head !== undefined, "document.head should exist");

  render(() => StyleElement(), head);
  const styleNodes = head.querySelectorAll("style");
  const latestStyle = styleNodes.item(styleNodes.length - 1);

  try {
    const content = latestStyle?.textContent ?? "";
    // The icon is URL-encoded in CSS (e.g. %3Cpath instead of <path>)
    assert(
      content.includes("%3Cpath") || content.includes("<path"),
      "SVG should contain path elements (encoded or decoded)",
    );
    assert(
      content.includes("d=") || content.includes("d%3D"),
      "SVG paths should have d attribute",
    );
    assert(
      content.includes("fill='context-fill'"),
      "SVG should use context-fill for theming",
    );
  } finally {
    latestStyle?.remove();
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "StyleElement returns node", fn: testStyleElementReturnsNode },
    {
      name: "rendered style contains zen mode selector",
      fn: testRenderedStyleContainsZenModeSelector,
    },
    {
      name: "StyleElement contains valid SVG icon",
      fn: testStyleElementContainsValidSVGIcon,
    },
    {
      name: "StyleElement targets correct button ID",
      fn: testStyleElementTargetsCorrectButtonId,
    },
    {
      name: "controller owns one Zen style per window",
      fn: testControllerOwnsSingleZenStylePerWindow,
    },
    {
      name: "StyleElement SVG contains zen mode icon",
      fn: testStyleElementSVGContainsZenModeIcon,
    },
  ];

  await runTests("zenModeStyleElement.test.ts", tests);
}
