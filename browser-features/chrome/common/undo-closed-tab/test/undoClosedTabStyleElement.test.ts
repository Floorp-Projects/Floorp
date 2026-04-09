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

function testStyleElementReturnsJsxNode(): void {
  const node = StyleElement();
  assert(node !== null, "StyleElement should return JSX node");
  assertEquals(
    typeof node,
    "object",
    "StyleElement return should be object-like",
  );
}

function testStyleElementIsStableAcrossCalls(): void {
  const first = StyleElement();
  const second = StyleElement();
  assert(
    first !== null && second !== null,
    "multiple calls should return nodes",
  );
  assertEquals(typeof first, typeof second, "return type should remain stable");
}

function testRenderedStyleContainsUndoSelector(): void {
  const head = document?.head;
  assert(head !== null && head !== undefined, "document.head should exist");

  render(() => StyleElement(), head);
  const styleNodes = head.querySelectorAll("style");
  const latestStyle = styleNodes.item(styleNodes.length - 1);

  try {
    assert(latestStyle !== null, "render should insert a style element");
    assert(
      (latestStyle.textContent ?? "").includes("#undo-closed-tab"),
      "rendered style should include #undo-closed-tab selector",
    );
  } finally {
    latestStyle?.remove();
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "StyleElement returns JSX node",
      fn: testStyleElementReturnsJsxNode,
    },
    {
      name: "StyleElement stable across calls",
      fn: testStyleElementIsStableAcrossCalls,
    },
    {
      name: "rendered style contains undo selector",
      fn: testRenderedStyleContainsUndoSelector,
    },
  ];

  await runTests("undoClosedTabStyleElement.test.ts", tests);
}
