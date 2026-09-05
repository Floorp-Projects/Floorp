// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
// The harness compares by ===, so lists are compared as joined strings.

import {
  CLIPS_PANEL_ID,
  CLIPS_PANEL_URL,
  withClipsPanel,
} from "../clips-panel-list.ts";
import type { Panel } from "../../panel-sidebar/utils/type.ts";
import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

const panel = (id: string, url: string, type: Panel["type"] = "static"): Panel => ({
  id,
  url,
  width: 0,
  type,
  icon: undefined,
  userContextId: undefined,
  zoomLevel: undefined,
  userAgent: undefined,
  extensionId: undefined,
});
const notes = panel("default-panel-notes", "floorp//notes");
const web = panel("web", "https://example.com", "web");
const clips = panel(CLIPS_PANEL_ID, CLIPS_PANEL_URL);

function testAddedAfterNotesOnce(): void {
  const added = withClipsPanel([notes, web], true);
  assertEquals(added.changed, true, "adds the panel when enabled");
  assertEquals(added.panels.map((p) => p.id).join(","), [notes.id, CLIPS_PANEL_ID, web.id].join(","), "lands right after Notes");
  assertEquals(withClipsPanel(added.panels, true).changed, false, "does not add it twice");
  assertEquals(withClipsPanel([web], true).panels.map((p) => p.id).join(","), [web.id, CLIPS_PANEL_ID].join(","), "goes last without Notes");
}

function testRemovedWhenDisabled(): void {
  const removed = withClipsPanel([notes, clips, web], false);
  assertEquals(removed.changed, true, "removes the panel when disabled");
  assertEquals(removed.panels.map((p) => p.id).join(","), [notes.id, web.id].join(","), "everything else stays");
  assertEquals(withClipsPanel([notes, web], false).changed, false, "nothing to remove, nothing changes");
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "panel is added after Notes, once", fn: testAddedAfterNotesOnce },
    { name: "panel is removed when disabled", fn: testRemovedWhenDisabled },
  ];
  await runTests("clips-panel-list.test.ts", tests);
}
