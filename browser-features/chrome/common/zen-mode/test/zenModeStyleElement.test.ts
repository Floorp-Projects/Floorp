// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { StyleElement } from "../styleElem.tsx";
import { render } from "@nora/solid-xul";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

function testStyleElementReturnsNode(): void {
  const node = StyleElement();
  assert(node !== null, "StyleElement should return a JSX node");
  assertEquals(
    typeof node,
    "object",
    "StyleElement result should be object-like",
  );
}

function testStyleElementCanBeCalledRepeatedly(): void {
  const nodes = [StyleElement(), StyleElement(), StyleElement()];
  assert(
    nodes.every((node) => node !== null),
    "repeated calls should continue returning nodes",
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

function testStyleElementCanBeRenderedMultipleTimes(): void {
  const head = document?.head;
  assert(head !== null && head !== undefined, "document.head should exist");

  const initialCount = head.querySelectorAll("style").length;

  // Render multiple times
  render(() => StyleElement(), head);
  render(() => StyleElement(), head);
  render(() => StyleElement(), head);

  const styleNodes = head.querySelectorAll("style");

  try {
    assert(
      styleNodes.length >= initialCount + 3,
      "should add at least 3 style elements after rendering 3 times",
    );

    // Verify only newly rendered style nodes have content
    for (let i = initialCount; i < styleNodes.length; i++) {
      const style = styleNodes.item(i);
      assert(
        (style.textContent ?? "").length > 0,
        "each rendered style should have content",
      );
    }
  } finally {
    // Cleanup only styles rendered by this test
    for (let i = initialCount; i < styleNodes.length; i++) {
      styleNodes.item(i)?.remove();
    }
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
      name: "StyleElement can be called repeatedly",
      fn: testStyleElementCanBeCalledRepeatedly,
    },
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
      name: "StyleElement can be rendered multiple times",
      fn: testStyleElementCanBeRenderedMultipleTimes,
    },
    {
      name: "StyleElement SVG contains zen mode icon",
      fn: testStyleElementSVGContainsZenModeIcon,
    },
  ];

  await runTests("zenModeStyleElement.test.ts", tests);
}
