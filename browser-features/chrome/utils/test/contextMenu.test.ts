// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { ContextMenuUtils } from "../context-menu.tsx";

import {
  assert,
  assertEquals,
  runTests,
} from "../../test/utils/test_harness.ts";

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
    assertEquals(
      elem,
      null,
      "contentAreaContextMenu should return null when element is absent",
    );
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

async function testAddContextBoxCreatesMenuitem(): Promise<void> {
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

  let _functionCalled = false;
  let checkCount = 0;
  let dispose: (() => void) | undefined;

  try {
    dispose = ContextMenuUtils.addContextBox(
      "test-context-menuitem",
      "test.label",
      "test-context-render-marker",
      () => {
        _functionCalled = true;
      },
      "test-context-check-target",
      () => {
        checkCount++;
      },
    );
    // Verify the checked function was called during setup (contextMenuObserverFunc runs)
    assert(
      checkCount === 1,
      "checkedFunction should be called during addContextBox setup",
    );
    checkTarget.setAttribute("hidden", "true");
    await new Promise((resolve) => setTimeout(resolve, 0));
    assertEquals(
      checkCount,
      2,
      "target changes update visibility after insertion",
    );
    dispose?.();
    checkTarget.removeAttribute("hidden");
    await new Promise((resolve) => setTimeout(resolve, 0));
    assertEquals(checkCount, 2, "disposed menu stops observing its source");
    assert(
      !document.getElementById("test-context-menuitem"),
      "scope disposal removes the owned item",
    );
  } finally {
    dispose?.();
    checkTarget.remove();
    renderMarker.remove();
    document?.getElementById("test-context-menuitem")?.remove();
  }
}

function testAddToolbarContentMenuPopupSetDoesNotThrow(): void {
  let threw = false;
  try {
    ContextMenuUtils.addToolbarContentMenuPopupSet(() =>
      null as unknown as Element
    );
  } catch {
    threw = true;
  }
  assert(!threw, "addToolbarContentMenuPopupSet should not throw");
}

export async function runAllTests(): Promise<void> {
  await runTests("contextMenu.test.ts", [
    {
      name: "contentAreaContextMenu returns element or null",
      fn: testContentAreaContextMenuReturnsElement,
    },
    {
      name: "onPopupShowing does not throw",
      fn: testOnPopupShowingDoesNotThrow,
    },
    {
      name: "addContextBox creates menuitem",
      fn: testAddContextBoxCreatesMenuitem,
    },
    {
      name: "addToolbarContentMenuPopupSet does not throw",
      fn: testAddToolbarContentMenuPopupSetDoesNotThrow,
    },
  ]);
}
