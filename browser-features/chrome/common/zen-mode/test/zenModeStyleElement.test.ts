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
  ];

  await runTests("zenModeStyleElement.test.ts", tests);
}
