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
  const container = document.createElement("div");
  const child0 = document.createElement("span");
  const child1 = document.createElement("span");
  container.appendChild(child0);
  container.appendChild(child1);
  assertEquals(findChildIndex(container, child0), 0, "first child → 0");
}

function testFindChildIndexSecond(): void {
  const container = document.createElement("div");
  const child0 = document.createElement("span");
  const child1 = document.createElement("span");
  container.appendChild(child0);
  container.appendChild(child1);
  assertEquals(findChildIndex(container, child1), 1, "second child → 1");
}

function testFindChildIndexNotFound(): void {
  const container = document.createElement("div");
  const child = document.createElement("span");
  container.appendChild(document.createElement("span"));
  assertEquals(findChildIndex(container, child), -1, "not a child → -1");
}

function testFindChildIndexEmpty(): void {
  const container = document.createElement("div");
  const child = document.createElement("span");
  assertEquals(findChildIndex(container, child), -1, "empty container → -1");
}

function testFindChildIndexMiddle(): void {
  const container = document.createElement("div");
  const children = Array.from({ length: 5 }, () =>
    document.createElement("span"),
  );
  for (const c of children) container.appendChild(c);
  assertEquals(findChildIndex(container, children[2]), 2, "middle child → 2");
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
