// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { findScrollableElement, VALID_SCROLL_DIRECTIONS } from "./NRMouseGestureScrollUtils.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../chrome/test/utils/test_harness.ts";

function requireDocument(): Document {
  if (!document) {
    throw new Error("document is unavailable in this test context");
  }
  return document;
}

function requireBody(doc: Document): HTMLElement {
  if (!doc.body) {
    throw new Error("document.body is unavailable in this test context");
  }
  return doc.body as HTMLElement;
}

function withFixture(run: (root: HTMLDivElement) => void): void {
  const doc = requireDocument();
  const root = doc.createElement("div");
  root.setAttribute("data-test-root", "scroll-utils");
  requireBody(doc).appendChild(root);
  try {
    run(root);
  } finally {
    root.remove();
  }
}

function testReturnsDocumentElementWhenNoScrollableContainer(): void {
  withFixture((root) => {
    const doc = requireDocument();
    const div = doc.createElement("div");
    div.style.setProperty("width", "200px");
    div.style.setProperty("height", "200px");
    root.appendChild(div);
    div.focus();

    const result = findScrollableElement(window, false);
    const expected = doc.scrollingElement || doc.documentElement;
    assertEquals(result, expected, "should return document element when no scrollable container exists");
  });
}

function testFindsHorizontallyScrollableContainer(): void {
  withFixture((root) => {
    const doc = requireDocument();
    const outer = doc.createElement("div");
    outer.style.setProperty("width", "300px");
    outer.style.setProperty("height", "300px");
    root.appendChild(outer);

    const inner = doc.createElement("div");
    inner.style.setProperty("overflow-x", "auto");
    inner.style.setProperty("width", "200px");
    inner.style.setProperty("height", "100px");

    const child = doc.createElement("div");
    child.style.setProperty("width", "500px");
    child.style.setProperty("height", "50px");
    inner.appendChild(child);
    outer.appendChild(inner);
    inner.setAttribute("tabindex", "-1");
    inner.focus();

    const result = findScrollableElement(window, true);
    assertEquals(result, inner, "should find horizontally scrollable container");
  });
}

function testFindsVerticallyScrollableContainer(): void {
  withFixture((root) => {
    const doc = requireDocument();
    const outer = doc.createElement("div");
    outer.style.setProperty("width", "300px");
    outer.style.setProperty("height", "300px");
    root.appendChild(outer);

    const inner = doc.createElement("div");
    inner.style.setProperty("overflow-y", "auto");
    inner.style.setProperty("width", "100px");
    inner.style.setProperty("height", "100px");

    const child = doc.createElement("div");
    child.style.setProperty("width", "50px");
    child.style.setProperty("height", "300px");
    inner.appendChild(child);
    outer.appendChild(inner);
    inner.setAttribute("tabindex", "-1");
    inner.focus();

    const result = findScrollableElement(window, false);
    assertEquals(result, inner, "should find vertically scrollable container");
  });
}

function testIgnoresNonScrollableOverflowVisible(): void {
  withFixture((root) => {
    const doc = requireDocument();
    const div = doc.createElement("div");
    div.style.setProperty("overflow", "visible");
    div.style.setProperty("width", "200px");
    div.style.setProperty("height", "200px");

    const child = doc.createElement("div");
    child.style.setProperty("width", "500px");
    child.style.setProperty("height", "500px");
    div.appendChild(child);
    root.appendChild(div);
    div.focus();

    const result = findScrollableElement(window, true);
    const docEl = doc.scrollingElement || doc.documentElement;
    assert(
      result !== div,
      "should not return div with overflow: visible",
    );
    assertEquals(
      result,
      docEl,
      "should return document element instead of overflow:visible div",
    );
  });
}

function testPrefersCloserScrollableAncestor(): void {
  withFixture((root) => {
    const doc = requireDocument();
    const outer = doc.createElement("div");
    outer.style.setProperty("overflow-x", "auto");
    outer.style.setProperty("width", "300px");
    outer.style.setProperty("height", "300px");

    const outerChild = doc.createElement("div");
    outerChild.style.setProperty("width", "600px");
    outerChild.style.setProperty("height", "50px");
    outer.appendChild(outerChild);

    const middle = doc.createElement("div");
    middle.style.setProperty("width", "250px");
    middle.style.setProperty("height", "250px");

    const inner = doc.createElement("div");
    inner.style.setProperty("overflow-x", "auto");
    inner.style.setProperty("width", "200px");
    inner.style.setProperty("height", "100px");

    const innerChild = doc.createElement("div");
    innerChild.style.setProperty("width", "400px");
    innerChild.style.setProperty("height", "50px");
    inner.appendChild(innerChild);
    middle.appendChild(inner);
    outer.appendChild(middle);
    root.appendChild(outer);
    inner.setAttribute("tabindex", "-1");
    inner.focus();

    const result = findScrollableElement(window, true);
    assertEquals(result, inner, "should prefer closer scrollable ancestor");
  });
}

function testValidScrollDirections(): void {
  assertEquals(VALID_SCROLL_DIRECTIONS.length, 8, "should have exactly 8 directions");
  const expected: readonly string[] = [
    "lineUp",
    "lineDown",
    "pageUp",
    "pageDown",
    "lineRight",
    "lineLeft",
    "toTop",
    "toBottom",
  ];
  for (const dir of expected) {
    assert(
      (VALID_SCROLL_DIRECTIONS as readonly string[]).includes(dir),
      `should include "${dir}"`,
    );
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "findScrollableElement returns document element when no scrollable container exists", fn: testReturnsDocumentElementWhenNoScrollableContainer },
    { name: "findScrollableElement finds horizontally scrollable container", fn: testFindsHorizontallyScrollableContainer },
    { name: "findScrollableElement finds vertically scrollable container", fn: testFindsVerticallyScrollableContainer },
    { name: "findScrollableElement ignores non-scrollable overflow visible", fn: testIgnoresNonScrollableOverflowVisible },
    { name: "findScrollableElement prefers closer scrollable ancestor", fn: testPrefersCloserScrollableAncestor },
    { name: "VALID_SCROLL_DIRECTIONS contains all expected directions", fn: testValidScrollDirections },
  ];
  await runTests("NRMouseGestureScrollUtils.test.ts", tests);
}
