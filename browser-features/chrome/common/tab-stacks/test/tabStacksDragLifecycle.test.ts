// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { ProxyDragLifecycle } from "../proxy-drag.ts";
import type { StackTab } from "../stack-bar.tsx";
import type { StackDragController } from "../types.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

function fixture() {
  const tab = document.createElement("div") as unknown as StackTab;
  const source = document.createElement("div");
  document.documentElement!.append(tab, source);
  tab._dragData = {};
  const calls: string[] = [];
  let active = false;
  let serial = 0;
  const timers = new Map<number, () => void>();
  const controller: StackDragController = {
    startTabDrag() {},
    getDropEffectForTabDrag: () => "move",
    handle_dragend() {
      calls.push("dragend");
      delete tab._dragData;
    },
    finishMoveTogetherSelectedTabs(value) {
      assertEquals(value, tab, "Finalizer receives the captured tab");
      calls.push("together");
    },
    finishAnimateTabMove() {
      calls.push("animation");
    },
    _resetTabsAfterDrop(value) {
      assertEquals(
        value,
        tab,
        "Firefox 156 reset takes a tab, not its document",
      );
      calls.push("reset");
    },
  };
  const lifecycle = new ProxyDragLifecycle({
    readSession: () => active ? {} : null,
    schedule: (callback) => {
      timers.set(++serial, callback);
      return serial;
    },
    cancel: (id) => {
      timers.delete(id);
    },
    finished: () => {
      calls.push("finished");
    },
  });
  lifecycle.begin(tab, source, controller);
  return {
    lifecycle,
    tab,
    source,
    calls,
    controller,
    timers,
    session(value: boolean) {
      active = value;
    },
    tick() {
      const entry = timers.entries().next().value;
      assert(entry, "Recovery timer is scheduled");
      timers.delete(entry[0]);
      entry[1]();
    },
    cleanup() {
      lifecycle.dispose();
      tab.remove();
      source.remove();
    },
  };
}

function testLostEndAfterDrop(): void {
  const f = fixture();
  try {
    f.lifecycle.noteDrop(f.tab);
    f.tick();
    assertEquals(
      f.calls.join(","),
      "together,animation,reset,finished",
      "Native cleanup precedes source UI release",
    );
    assertEquals(f.tab._dragData, undefined, "Captured payload is released");
    assertEquals(f.timers.size, 0, "No recovery timer remains");
    f.lifecycle.end(f.tab, new DragEvent("dragend"));
    assertEquals(
      f.calls.length,
      4,
      "Late dragend cannot detach or finalize twice",
    );
  } finally {
    f.cleanup();
  }
}

function testDisconnectedSourceAndTab(): void {
  const f = fixture();
  try {
    f.source.remove();
    f.tab.remove();
    f.tick();
    assertEquals(
      f.calls.join(","),
      "animation,reset,finished",
      "Cross-window adoption recovers through captured source controller",
    );
    assertEquals(
      f.tab._dragData,
      undefined,
      "Disconnected source payload is released",
    );
  } finally {
    f.cleanup();
  }
}

function testActiveSessionIsNotInterrupted(): void {
  const f = fixture();
  try {
    const data = f.tab._dragData;
    f.session(true);
    f.lifecycle.noteDrop(f.tab);
    f.tick();
    assertEquals(f.calls.length, 0, "Active platform drag is untouched");
    assertEquals(f.tab._dragData, data, "Active drag retains its payload");
    f.session(false);
    f.tick();
    assertEquals(
      f.calls.join(","),
      "together,animation,reset,finished",
      "Cleanup resumes after the platform drag ends",
    );
  } finally {
    f.cleanup();
  }
}

function testStartAndReplacementGuards(): void {
  const f = fixture();
  try {
    f.tick();
    assertEquals(
      f.calls.length,
      0,
      "The drag loop need not exist inside dragstart yet",
    );
    f.lifecycle.noteDrop(f.tab);
    const replacement = {};
    f.tab._dragData = replacement;
    f.tick();
    assertEquals(
      f.calls.join(","),
      "finished",
      "A new native payload is untouched",
    );
    assertEquals(f.tab._dragData, replacement, "Later payload is preserved");
    f.lifecycle.begin(f.tab, f.source, f.controller);
    f.lifecycle.end(f.tab, new DragEvent("dragend"));
    assertEquals(
      f.calls.join(","),
      "finished,dragend,finished",
      "Normal completion uses the native handler once",
    );
    assertEquals(f.timers.size, 0, "Normal dragend cancels recovery");
  } finally {
    f.cleanup();
  }
}

function testNativeTabReceivesEnd(): void {
  const f = fixture();
  try {
    // Firefox adds the real tab as the drag source, so its native listener may
    // consume dragend before the proxy listener sees it (including cancel).
    delete f.tab._dragData;
    f.tick();
    assertEquals(
      f.calls.join(","),
      "finished",
      "Native cleanup is not repeated",
    );
    assertEquals(f.timers.size, 0, "Native completion stops recovery polling");
  } finally {
    f.cleanup();
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("tabStacksDragLifecycle.test.ts", [
    {
      name: "Drop without dragend cleans native state exactly once",
      fn: testLostEndAfterDrop,
    },
    {
      name: "Removed proxy and adopted tab do not lose source recovery",
      fn: testDisconnectedSourceAndTab,
    },
    {
      name: "Recovery waits for the native drag session to finish",
      fn: testActiveSessionIsNotInterrupted,
    },
    {
      name: "Startup, replacement, and normal dragend guards",
      fn: testStartAndReplacementGuards,
    },
    {
      name: "Native real-tab dragend cancels proxy recovery polling",
      fn: testNativeTabReceivesEnd,
    },
  ]);
}
