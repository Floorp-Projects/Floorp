// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  emptyDragSession,
  reduceDragSession,
} from "../drag-session.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

function testDownStartsDragAnchored(): void {
  const result = reduceDragSession(emptyDragSession(), {
    type: "down",
    screenX: 10,
    screenY: 20,
  });
  assertEquals(result.type, "start", "chord down should start a drag");
  if (result.type !== "start") return;
  assert(result.state.active, "drag should be active after start");
  assertEquals(result.state.lastScreenX, 10, "anchor x recorded");
  assertEquals(result.state.lastScreenY, 20, "anchor y recorded");
}

function testMoveComputesDeltaAndAdvancesAnchor(): void {
  const started = reduceDragSession(emptyDragSession(), {
    type: "down",
    screenX: 10,
    screenY: 20,
  });
  if (started.type !== "start") return;
  const moved = reduceDragSession(started.state, {
    type: "move",
    screenX: 25,
    screenY: 35,
    chordHeld: true,
  });
  assertEquals(moved.type, "move", "move while active should move");
  if (moved.type !== "move") return;
  assertEquals(moved.deltaX, 15, "delta x computed from anchor");
  assertEquals(moved.deltaY, 15, "delta y computed from anchor");
  assertEquals(moved.state.lastScreenX, 25, "anchor advances on move");
  assertEquals(moved.state.lastScreenY, 35, "anchor advances on move");
}

function testChordReleaseCancelsDrag(): void {
  const started = reduceDragSession(emptyDragSession(), {
    type: "down",
    screenX: 0,
    screenY: 0,
  });
  if (started.type !== "start") return;
  const cancelled = reduceDragSession(started.state, {
    type: "move",
    screenX: 5,
    screenY: 5,
    chordHeld: false,
  });
  assertEquals(cancelled.type, "cancel", "released chord should cancel drag");
  if (cancelled.type !== "cancel") return;
  assert(!cancelled.state.active, "drag should be inactive after cancel");
}

function testMoveWithoutDragIsNoop(): void {
  const result = reduceDragSession(emptyDragSession(), {
    type: "move",
    screenX: 5,
    screenY: 5,
    chordHeld: true,
  });
  assertEquals(result.type, "noop", "move without active drag is a noop");
}

function testKeyUpAndBlurCancelDrag(): void {
  const started = reduceDragSession(emptyDragSession(), {
    type: "down",
    screenX: 0,
    screenY: 0,
  });
  if (started.type !== "start") return;

  const keyUp = reduceDragSession(started.state, { type: "cancel" });
  assertEquals(keyUp.type, "cancel", "key-up should cancel");
  if (keyUp.type !== "cancel") return;
  assert(!keyUp.state.active, "inactive after key-up");

  const restarted = reduceDragSession(emptyDragSession(), {
    type: "down",
    screenX: 0,
    screenY: 0,
  });
  if (restarted.type !== "start") return;
  const blur = reduceDragSession(restarted.state, { type: "end" });
  assertEquals(blur.type, "cancel", "mouseup/blur should cancel");
  assert(blur.type === "cancel" && !blur.state.active, "inactive after end");
}

function testEndThenMoveStaysInactive(): void {
  const started = reduceDragSession(emptyDragSession(), {
    type: "down",
    screenX: 0,
    screenY: 0,
  });
  if (started.type !== "start") return;
  const ended = reduceDragSession(started.state, { type: "end" });
  if (ended.type !== "cancel") return;
  const afterEnd = reduceDragSession(ended.state, {
    type: "move",
    screenX: 10,
    screenY: 10,
    chordHeld: true,
  });
  assertEquals(afterEnd.type, "noop", "no drag after end");
}

const tests: TestCase[] = [
  { name: "chord down starts a drag anchored at the point", fn: testDownStartsDragAnchored },
  { name: "move computes delta and advances anchor", fn: testMoveComputesDeltaAndAdvancesAnchor },
  { name: "released chord cancels the drag", fn: testChordReleaseCancelsDrag },
  { name: "move without active drag is a noop", fn: testMoveWithoutDragIsNoop },
  { name: "key-up and blur cancel the drag", fn: testKeyUpAndBlurCancelDrag },
  { name: "end then move stays inactive", fn: testEndThenMoveStaysInactive },
];

export async function runAllTests(): Promise<void> {
  await runTests("windowDragSession.test.ts", tests);
}
