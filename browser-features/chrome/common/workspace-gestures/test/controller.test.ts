// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  adjacentWorkspace,
  WorkspaceGestureController,
} from "../controller.ts";
import type {
  WorkspaceGestureInput,
  WorkspaceGesturePreview,
  WorkspaceSwitchSnapshot,
  WorkspaceSwitchTarget,
} from "../types.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

function snapshot(currentId = "b"): WorkspaceSwitchSnapshot {
  return {
    currentId,
    order: ["a", "b", "c"].map((id) => ({
      id,
      name: id.toUpperCase(),
      userContextId: id === "c" ? 2 : 1,
      sessionName: id === "c" ? "Personal" : "Work",
    })),
  };
}

function setup(currentId = "b") {
  let current: WorkspaceSwitchSnapshot | null = snapshot(currentId);
  const commits: string[] = [];
  const announcements: WorkspaceSwitchTarget[] = [];
  const previews: Array<WorkspaceGesturePreview | null> = [];
  const controller = new WorkspaceGestureController({
    getSnapshot: () => current,
    commit: (id) => {
      commits.push(id);
      current = current ? { ...current, currentId: id } : null;
    },
    preview: (value) => previews.push(value),
    announce: (target) => announcements.push(target),
  });
  return {
    controller,
    commits,
    announcements,
    previews,
    replace: (value: WorkspaceSwitchSnapshot | null) => {
      current = value;
    },
  };
}

function input(
  phase: WorkspaceGestureInput["phase"],
  x: number,
  time: number,
  overrides: Partial<WorkspaceGestureInput> = {},
): WorkspaceGestureInput {
  return {
    source: "native",
    fingerCount: 3,
    operationId: "1",
    phase,
    displacementX: x,
    displacementY: 0,
    time,
    ...overrides,
  };
}

const tests: TestCase[] = [
  {
    name: "preview preserves current workspace until end and commits once",
    fn() {
      const s = setup();
      s.controller.handle(input("start", 0, 0));
      assert(
        s.controller.handle(input("update", -120, 100)),
        "horizontal intent is accepted",
      );
      assertEquals(
        s.commits.length,
        0,
        "preview does not switch or load a destination",
      );
      assertEquals(
        s.announcements.length,
        0,
        "preview is not repeatedly announced",
      );
      assertEquals(
        s.previews.at(-1)?.target.id,
        "c",
        "left finger motion previews right neighbor",
      );
      s.controller.handle(input("end", -120, 110));
      assertEquals(s.commits.join(","), "c", "end commits exactly once");
      assertEquals(
        s.announcements[0]?.sessionName,
        "Personal",
        "commit announces session name",
      );
      s.controller.handle(input("end", -160, 130));
      s.controller.handle(input("update", -300, 150, { inertia: true }));
      assertEquals(
        s.commits.length,
        1,
        "duplicate completion and inertia do not move again",
      );
    },
  },
  {
    name: "reversing back cancels even if reverse velocity is fast",
    fn() {
      const s = setup();
      s.controller.handle(input("start", 0, 0));
      s.controller.handle(input("update", -130, 100));
      s.controller.handle(input("end", -40, 120));
      assertEquals(
        s.commits.length,
        0,
        "a fast reverse is not a forward flick",
      );
      assertEquals(s.previews.at(-1), null, "preview is removed");
    },
  },
  {
    name: "explicit cancellation suppresses late updates and completion",
    fn() {
      const s = setup();
      s.controller.handle(input("start", 0, 0));
      s.controller.handle(input("update", -100, 100));
      s.controller.cancel();
      s.controller.handle(input("update", -200, 150));
      s.controller.handle(input("end", -200, 180));
      assertEquals(
        s.commits.length,
        0,
        "blur/Esc/drag cancellation remains final",
      );
    },
  },
  {
    name: "start and end at edges never wrap",
    fn() {
      const s = setup("a");
      s.controller.handle(input("start", 0, 0));
      s.controller.handle(input("update", 150, 120));
      assertEquals(s.previews.at(-1)?.edge, true, "edge feedback is shown");
      s.controller.handle(input("end", 150, 130));
      assertEquals(s.commits.length, 0, "left edge stops");
      assertEquals(
        s.controller.switchByOffset(-1),
        false,
        "key mapping stops at edge too",
      );
      s.replace(snapshot("c"));
      assertEquals(s.controller.switchByOffset(1), false, "right edge stops");
      assertEquals(
        adjacentWorkspace(snapshot("b"), 1)?.id,
        "c",
        "order defines direction",
      );
    },
  },
  {
    name:
      "reorder, default-session changes, selection and disable invalidate preview",
    fn() {
      const variants: Array<WorkspaceSwitchSnapshot | null> = [
        { ...snapshot(), order: [...snapshot().order].reverse() },
        {
          ...snapshot(),
          order: snapshot().order.map((entry) =>
            entry.id === "c" ? { ...entry, userContextId: 3 } : entry
          ),
        },
        snapshot("a"),
        null,
      ];
      for (const changed of variants) {
        const s = setup();
        s.controller.handle(input("start", 0, 0));
        s.controller.handle(input("update", -110, 100));
        s.replace(changed);
        s.controller.validate();
        s.controller.handle(input("end", -150, 130));
        assertEquals(s.commits.length, 0, "changed target cannot be committed");
      }
    },
  },
  {
    name: "unknown finger counts and vertical movement preserve ordinary input",
    fn() {
      const s = setup();
      for (const fingerCount of [undefined, 0, 2, 4]) {
        assertEquals(
          s.controller.handle(input("start", 0, 0, { fingerCount })),
          false,
          "non-three-finger start is ignored",
        );
      }
      s.controller.handle(input("start", 0, 0));
      assertEquals(
        s.controller.handle(input("update", 10, 100, { displacementY: 70 })),
        false,
        "vertical intent is not consumed",
      );
      s.controller.handle(input("end", 200, 200));
      assertEquals(
        s.commits.length,
        0,
        "cancelled vertical operation cannot later turn horizontal",
      );
    },
  },
  {
    name: "controller ownership separates windows and input sources",
    fn() {
      const first = setup();
      const second = setup();
      first.controller.handle(input("start", 0, 0));
      assertEquals(
        first.controller.handle(
          input("start", 0, 10, { source: "sidebar", operationId: "wheel" }),
        ),
        false,
        "a second adapter cannot own one controller concurrently",
      );
      first.controller.handle(input("end", -120, 100));
      assertEquals(first.commits[0], "c", "origin window switches");
      assertEquals(second.commits.length, 0, "other window retains selection");
    },
  },
  {
    name: "one workspace does not consume gestures or actions",
    fn() {
      const s = setup();
      s.replace({ currentId: "a", order: [snapshot().order[0]] });
      assertEquals(
        s.controller.handle(input("start", 0, 0)),
        false,
        "single workspace start is ignored",
      );
      assertEquals(
        s.controller.switchByOffset(1),
        false,
        "no extra action is consumed",
      );
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("workspaceGestureController.test.ts", tests);
}
