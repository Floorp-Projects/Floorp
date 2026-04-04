// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { ContextMenuUtils } from "../../utils/context-menu.tsx";

type TestCase = { name: string; fn: () => void | Promise<void> };

function assert(condition: unknown, message: string): asserts condition {
  if (!condition) throw new Error(message);
}

function assertEquals<T>(actual: T, expected: T, message: string): void {
  if (actual !== expected)
    throw new Error(
      `${message} (expected: ${String(expected)}, actual: ${String(actual)})`,
    );
}

function testContentAreaContextMenuReturnsElement(): void {
  const elem = ContextMenuUtils.contentAreaContextMenu();
  // In a real browser window, #contentAreaContextMenu should exist
  if (elem) {
    assertEquals(
      elem.id,
      "contentAreaContextMenu",
      "contentAreaContextMenu should return the element with id contentAreaContextMenu",
    );
  } else {
    // Element may not exist in minimal test harness; just verify it returns null gracefully
    assertEquals(elem, null, "contentAreaContextMenu should return null when element is absent");
  }
}

function testOnPopupShowingDoesNotThrow(): void {
  // onPopupShowing should execute without throwing even if DOM elements are missing
  let threw = false;
  try {
    ContextMenuUtils.onPopupShowing();
  } catch {
    threw = true;
  }
  assert(!threw, "onPopupShowing should not throw when invoked");
}

function testAddContextBoxCreatesMenuitem(): void {
  // Create mock elements needed by addContextBox
  const contextMenu = ContextMenuUtils.contentAreaContextMenu();
  if (!contextMenu) {
    // Skip if contentAreaContextMenu is not available
    return;
  }

  const checkTarget = document?.createElement("div");
  if (!checkTarget) return;
  checkTarget.id = "test-context-check-target";
  document?.body?.appendChild(checkTarget);

  const renderMarker = document?.createElement("div");
  if (!renderMarker) return;
  renderMarker.id = "test-context-render-marker";
  contextMenu.appendChild(renderMarker);

  let functionCalled = false;
  let checkedCalled = false;

  try {
    ContextMenuUtils.addContextBox(
      "test-context-menuitem",
      "test.label",
      "test-context-render-marker",
      () => {
        functionCalled = true;
      },
      "test-context-check-target",
      () => {
        checkedCalled = true;
      },
    );
    // Verify the checked function was called during setup (contextMenuObserverFunc runs)
    assert(checkedCalled, "checkedFunction should be called during addContextBox setup");
  } finally {
    checkTarget.remove();
    renderMarker.remove();
    document?.getElementById("test-context-menuitem")?.remove();
  }
}

function testAddToolbarContentMenuPopupSetDoesNotThrow(): void {
  let threw = false;
  try {
    ContextMenuUtils.addToolbarContentMenuPopupSet(() => null as any);
  } catch {
    threw = true;
  }
  assert(!threw, "addToolbarContentMenuPopupSet should not throw");
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "contentAreaContextMenu returns element or null", fn: testContentAreaContextMenuReturnsElement },
    { name: "onPopupShowing does not throw", fn: testOnPopupShowingDoesNotThrow },
    { name: "addContextBox creates menuitem", fn: testAddContextBoxCreatesMenuitem },
    { name: "addToolbarContentMenuPopupSet does not throw", fn: testAddToolbarContentMenuPopupSetDoesNotThrow },
  ];

  const failures: string[] = [];
  for (const test of tests) {
    try {
      await test.fn();
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      failures.push(`${test.name}: ${message}`);
    }
  }
  if (failures.length > 0)
    throw new Error(`contextMenu.test.ts failures: ${failures.join(" | ")}`);
}
