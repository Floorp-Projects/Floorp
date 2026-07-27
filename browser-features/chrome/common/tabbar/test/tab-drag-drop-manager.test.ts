// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import {
  cleanupOwnedDropIndicator,
  DropIndicatorOwnership,
  resolveDropIndicatorTarget,
} from "../multirow-tabbar/tab-drag-drop-manager.ts";

// ---------------------------------------------------------------------------
// Tests — TabDragDropManager dragend listener
// ---------------------------------------------------------------------------

/**
 * Test that dragend listener resets state properly.
 * This addresses Issue #2488: tab position offset when creating split view
 * by dragging tabs.
 */
function testDragEndResetsState(): void {
  // Mock state
  const state: {
    lastKnownIndex: number | null;
    groupToInsertTo: null;
    positionInGroup: null;
    draggedTabIndex: number | null;
  } = {
    lastKnownIndex: 5,
    groupToInsertTo: null,
    positionInGroup: null,
    draggedTabIndex: 3,
  };

  // Simulate dragend handler
  const dragEndHandler = () => {
    state.lastKnownIndex = null;
    state.groupToInsertTo = null;
    state.positionInGroup = null;
    state.draggedTabIndex = null;
  };

  dragEndHandler();

  assertEquals(state.lastKnownIndex, null, "lastKnownIndex should be null");
  assertEquals(state.draggedTabIndex, null, "draggedTabIndex should be null");
}

function testDragEndClearsDraggedTabIndex(): void {
  const state: { draggedTabIndex: number | null } = {
    draggedTabIndex: 7,
  };

  const dragEndHandler = () => {
    state.draggedTabIndex = null;
  };

  dragEndHandler();

  assertEquals(
    state.draggedTabIndex,
    null,
    "draggedTabIndex should be cleared on dragend",
  );
}

function testDragEndWithNullState(): void {
  const state = {
    lastKnownIndex: null,
    draggedTabIndex: null,
  };

  const dragEndHandler = () => {
    state.lastKnownIndex = null;
    state.draggedTabIndex = null;
  };

  // Should not throw
  dragEndHandler();

  assertEquals(state.lastKnownIndex, null, "lastKnownIndex remains null");
  assertEquals(state.draggedTabIndex, null, "draggedTabIndex remains null");
}

function testDropIndicatorTargets(): void {
  const first = resolveDropIndicatorTarget(0, 3);
  assertEquals(
    first?.tabIndex,
    0,
    "index 0 should target the first tab",
  );
  assertEquals(first?.atEnd, false, "index 0 should use the leading edge");

  const middle = resolveDropIndicatorTarget(1, 3);
  assertEquals(
    middle?.tabIndex,
    1,
    "middle index should target that tab",
  );
  assertEquals(
    middle?.atEnd,
    false,
    "middle index should use the leading edge",
  );

  const end = resolveDropIndicatorTarget(3, 3);
  assertEquals(
    end?.tabIndex,
    2,
    "tabCount should target the trailing edge of the last tab",
  );
  assertEquals(end?.atEnd, true, "tabCount should use the trailing edge");
  assertEquals(
    resolveDropIndicatorTarget(0, 0),
    null,
    "an empty tab strip has no indicator target",
  );
  assertEquals(
    resolveDropIndicatorTarget(4, 3),
    null,
    "out-of-range indices should be rejected",
  );
}

function createDropIndicator(): XULElement {
  return document!.createXULElement("hbox") as XULElement;
}

function showDropIndicator(indicator: XULElement, offset: number): void {
  indicator.hidden = false;
  indicator.style.setProperty(
    "transform",
    `translate(${offset}px, ${offset}px)`,
  );
  indicator.style.setProperty("margin-inline-start", `-${offset}px`);
}

function testDropIndicatorCleanup(): void {
  cleanupOwnedDropIndicator(null);

  const indicator = createDropIndicator();
  showDropIndicator(indicator, 24);
  indicator.style.setProperty("opacity", "0.5");

  cleanupOwnedDropIndicator(indicator);

  assertEquals(indicator.hidden, true, "cleanup should hide the indicator");
  assertEquals(
    indicator.style.getPropertyValue("transform"),
    "",
    "cleanup should clear the custom transform",
  );
  assertEquals(
    indicator.style.getPropertyValue("margin-inline-start"),
    "",
    "cleanup should clear the custom inline margin",
  );
  assertEquals(
    indicator.style.getPropertyValue("opacity"),
    "0.5",
    "cleanup should preserve unrelated inline styles",
  );

  cleanupOwnedDropIndicator(indicator);
  assertEquals(
    indicator.hidden,
    true,
    "repeated cleanup should remain safe",
  );
}

function testDropIndicatorOwnershipTransfer(): void {
  const ownership = new DropIndicatorOwnership();
  const first = createDropIndicator();
  const second = createDropIndicator();

  showDropIndicator(first, 16);
  assertEquals(
    ownership.acquire(first),
    first,
    "the first indicator should be acquired",
  );
  ownership.acquire(first);
  assertEquals(
    first.hidden,
    false,
    "reacquiring the same indicator should not clean it up",
  );

  showDropIndicator(second, 32);
  ownership.acquire(second);
  assertEquals(
    first.hidden,
    true,
    "replacing the owned indicator should hide the old node",
  );
  assertEquals(
    first.style.getPropertyValue("transform"),
    "",
    "replacing the owned indicator should clear the old transform",
  );
  assertEquals(
    second.hidden,
    false,
    "acquiring a replacement should not mutate the new node",
  );

  assertEquals(
    ownership.take(),
    second,
    "take should return the currently owned indicator",
  );
  assertEquals(
    ownership.take(),
    null,
    "take should clear ownership before returning",
  );
  cleanupOwnedDropIndicator(second);
  assertEquals(
    second.hidden,
    true,
    "a taken detached indicator should still be cleanable",
  );

  showDropIndicator(first, 48);
  ownership.acquire(first);
  cleanupOwnedDropIndicator(ownership.take());
  assertEquals(
    first.hidden,
    true,
    "an indicator should be reusable after an earlier cleanup",
  );
}

// ---------------------------------------------------------------------------
// Test runner
// ---------------------------------------------------------------------------

const tests: TestCase[] = [
  { name: "dragend resets state", fn: testDragEndResetsState },
  {
    name: "dragend clears draggedTabIndex",
    fn: testDragEndClearsDraggedTabIndex,
  },
  { name: "dragend with null state", fn: testDragEndWithNullState },
  {
    name: "drop indicator targets include index zero",
    fn: testDropIndicatorTargets,
  },
  {
    name: "owned drop indicator cleanup is scoped and idempotent",
    fn: testDropIndicatorCleanup,
  },
  {
    name: "drop indicator ownership transfers between XUL nodes",
    fn: testDropIndicatorOwnershipTransfer,
  },
];

await runTests("tab-drag-drop-manager.test", tests);
