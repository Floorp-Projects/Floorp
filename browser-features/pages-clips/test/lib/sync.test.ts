// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  baseFromClips,
  mergeClips,
  MAX_SYNC_BYTES,
  selectForSync,
} from "../../src/lib/sync.ts";
import type { Clip } from "../../src/types/clip.ts";
import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";

function clip(id: string, at: number, extra: Partial<Clip> = {}): Clip {
  return {
    id,
    kind: "text",
    text: id,
    createdAt: at,
    updatedAt: at,
    pinned: false,
    ...extra,
  };
}

function ids(clips: Clip[]): string {
  return clips.map((c) => c.id).sort().join(",");
}

function testNewOnEitherSideIsKept(): void {
  const local = [clip("a", 1)];
  const remote = [clip("b", 2)];
  assertEquals(ids(mergeClips(local, remote, {})), "a,b");
}

function testDeletionOnTheOtherSideIsRespected(): void {
  // Both sides had "a" at the last sync; it is gone from remote now.
  const local = [clip("a", 1)];
  const base = baseFromClips([clip("a", 1)]);
  const remote = [clip("b", 1)];
  assertEquals(ids(mergeClips(local, remote, base)), "b");
}

function testMissingBelowTheRemoteFloorIsNotADeletion(): void {
  // Remote only carries clips from t=100 on, because that is all that fitted.
  const local = [clip("old", 1), clip("new", 100)];
  const base = baseFromClips(local);
  const remote = [clip("new", 100)];
  assertEquals(ids(mergeClips(local, remote, base)), "new,old");
}

function testPinChangeWinsByTime(): void {
  const local = [clip("a", 1, { updatedAt: 5 })];
  const remote = [clip("a", 1, { updatedAt: 9, pinned: true })];
  const merged = mergeClips(local, remote, baseFromClips([clip("a", 1)]));
  assertEquals(merged.length, 1);
  assertEquals(merged[0].pinned, true);
}

function testLocalEditSurvivesARemoteDeletion(): void {
  const local = [clip("a", 1, { updatedAt: 50, pinned: true })];
  const base = { a: 1 };
  assertEquals(ids(mergeClips(local, [], base)), "a");
}

function testSelectionStaysUnderTheCap(): void {
  const big = "x".repeat(50 * 1024);
  const many = Array.from(
    { length: 40 },
    (_, i) => clip(`c${i}`, i, { text: big }),
  );
  const { payload, dropped } = selectForSync(many);
  const size = new TextEncoder().encode(JSON.stringify(payload)).length;
  assertEquals(size <= MAX_SYNC_BYTES, true);
  assertEquals(dropped > 0, true);
  assertEquals(payload.floor > 0, true);
}

function testPinnedClipsTravelFirst(): void {
  const big = "x".repeat(200 * 1024);
  const clips = [
    clip("pinned-old", 1, { text: big, pinned: true }),
    clip("plain-new", 9, { text: big }),
    clip("plain-newer", 10, { text: big }),
  ];
  const { payload } = selectForSync(clips);
  assertEquals(payload.clips.some((c) => c.id === "pinned-old"), true);
}

function testNothingDroppedMeansNoFloor(): void {
  const { payload, dropped } = selectForSync([clip("a", 1), clip("b", 2)]);
  assertEquals(dropped, 0);
  assertEquals(payload.floor, 0);
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "new on either side is kept", fn: testNewOnEitherSideIsKept },
    {
      name: "deletion on the other side is respected",
      fn: testDeletionOnTheOtherSideIsRespected,
    },
    {
      name: "missing below the remote floor is not a deletion",
      fn: testMissingBelowTheRemoteFloorIsNotADeletion,
    },
    { name: "pin change wins by time", fn: testPinChangeWinsByTime },
    {
      name: "local edit survives a remote deletion",
      fn: testLocalEditSurvivesARemoteDeletion,
    },
    { name: "selection stays under the cap", fn: testSelectionStaysUnderTheCap },
    { name: "pinned clips travel first", fn: testPinnedClipsTravelFirst },
    { name: "nothing dropped means no floor", fn: testNothingDroppedMeansNoFloor },
  ];

  await runTests("sync.test.ts", tests);
}
