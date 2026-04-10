// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  findChildIndex,
  getTabsToolbar,
  resolveTabsContainer,
} from "../multirow-tabbar/dom-utils.ts";
import {
  type TestCase,
  assert,
  assertEquals,
} from "../../../test/utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Tests — findChildIndex (pure logic, uses created DOM elements)
// ---------------------------------------------------------------------------

function testFindChildIndexFirst(): void {
  const container = document!.createElement("div");
  const child0 = document!.createElement("span");
  const child1 = document!.createElement("span");
  container.appendChild(child0);
  container.appendChild(child1);
  assertEquals(findChildIndex(container, child0), 0, "first child → 0");
}

function testFindChildIndexSecond(): void {
  const container = document!.createElement("div");
  const child0 = document!.createElement("span");
  const child1 = document!.createElement("span");
  container.appendChild(child0);
  container.appendChild(child1);
  assertEquals(findChildIndex(container, child1), 1, "second child → 1");
}

function testFindChildIndexNotFound(): void {
  const container = document!.createElement("div");
  const child = document!.createElement("span");
  container.appendChild(document!.createElement("span"));
  assertEquals(findChildIndex(container, child), -1, "not a child → -1");
}

function testFindChildIndexEmpty(): void {
  const container = document!.createElement("div");
  const child = document!.createElement("span");
  assertEquals(findChildIndex(container, child), -1, "empty container → -1");
}

function testFindChildIndexMiddle(): void {
  const container = document!.createElement("div");
  const children = Array.from({ length: 5 }, () =>
    document!.createElement("span"),
  );
  for (const c of children) container.appendChild(c);
  assertEquals(findChildIndex(container, children[2]), 2, "middle child → 2");
}

function testFindChildIndexLast(): void {
  const container = document!.createElement("div");
  const children = Array.from({ length: 3 }, () =>
    document!.createElement("span"),
  );
  for (const c of children) container.appendChild(c);
  assertEquals(findChildIndex(container, children[2]), 2, "last child → 2");
}

function testFindChildIndexWithTextNodes(): void {
  const container = document!.createElement("div");
  const child1 = document!.createElement("span");
  const textNode = document!.createTextNode("text");
  const child2 = document!.createElement("span");
  container.appendChild(child1);
  container.appendChild(textNode);
  container.appendChild(child2);
  assertEquals(findChildIndex(container, child2), 2, "child after text node → 2");
}

function testFindChildIndexWithComments(): void {
  const container = document!.createElement("div");
  const child1 = document!.createElement("span");
  const comment = document!.createComment("comment");
  const child2 = document!.createElement("span");
  container.appendChild(child1);
  container.appendChild(comment);
  container.appendChild(child2);
  assertEquals(findChildIndex(container, child2), 2, "child after comment → 2");
}

function testFindChildIndexWithMixedContent(): void {
  const container = document!.createElement("div");
  const text1 = document!.createTextNode("text1");
  const child1 = document!.createElement("span");
  const text2 = document!.createTextNode("text2");
  const child2 = document!.createElement("span");
  container.appendChild(text1);
  container.appendChild(child1);
  container.appendChild(text2);
  container.appendChild(child2);
  assertEquals(findChildIndex(container, child1), 1, "element with text nodes → 1");
  assertEquals(findChildIndex(container, child2), 3, "second element with text nodes → 3");
}

function testFindChildIndexLargeContainer(): void {
  const container = document!.createElement("div");
  const children = Array.from({ length: 100 }, () =>
    document!.createElement("span"),
  );
  for (const c of children) container.appendChild(c);
  assertEquals(findChildIndex(container, children[50]), 50, "child at index 50 → 50");
  assertEquals(findChildIndex(container, children[99]), 99, "child at index 99 → 99");
}

// ---------------------------------------------------------------------------
// Tests — getTabsToolbar (browser environment)
// ---------------------------------------------------------------------------

function testGetTabsToolbar(): void {
  const toolbar = getTabsToolbar();
  // In actual browser context, TabsToolbar should exist
  assert(toolbar !== null, "TabsToolbar should exist in browser chrome");
  assertEquals(toolbar.id, "TabsToolbar", "should have correct id");
}

// ---------------------------------------------------------------------------
// Tests — resolveTabsContainer (browser environment)
// ---------------------------------------------------------------------------

function testResolveTabsContainer(): void {
  const container = resolveTabsContainer();
  assert(container !== null, "tabs container should exist in browser chrome");
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "findChildIndex first", fn: testFindChildIndexFirst },
    { name: "findChildIndex second", fn: testFindChildIndexSecond },
    { name: "findChildIndex not found", fn: testFindChildIndexNotFound },
    { name: "findChildIndex empty", fn: testFindChildIndexEmpty },
    { name: "findChildIndex middle", fn: testFindChildIndexMiddle },
    { name: "findChildIndex last", fn: testFindChildIndexLast },
    { name: "findChildIndex with text nodes", fn: testFindChildIndexWithTextNodes },
    { name: "findChildIndex with comments", fn: testFindChildIndexWithComments },
    { name: "findChildIndex with mixed content", fn: testFindChildIndexWithMixedContent },
    { name: "findChildIndex large container", fn: testFindChildIndexLargeContainer },
    { name: "getTabsToolbar exists", fn: testGetTabsToolbar },
    { name: "resolveTabsContainer exists", fn: testResolveTabsContainer },
  ];

  const failures: string[] = [];

  for (const test of tests) {
    try {
      test.fn();
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      failures.push(`${test.name}: ${message}`);
    }
  }

  if (failures.length > 0) {
    throw new Error(`tabbarDomUtils.test.ts failures: ${failures.join(" | ")}`);
  }
}
