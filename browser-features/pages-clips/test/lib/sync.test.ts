// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  baseFromClips,
  baseOf,
  GONE_KEPT_MS,
  MAX_SYNC_BYTES,
  mergeClips,
  nextSyncState,
  parseSyncState,
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
  assertEquals(ids(mergeClips(local, remote, {}, 0)), "a,b", "both are kept");
}

function testDeletionOnTheOtherSideIsRespected(): void {
  // Both sides had "a" at the last sync; it is gone from remote now.
  const local = [clip("a", 1)];
  const base = baseFromClips([clip("a", 1)]);
  const remote = [clip("b", 1)];
  assertEquals(
    ids(mergeClips(local, remote, base, 0)),
    "b",
    "the deleted one is let go",
  );
}

function testMissingBelowTheRemoteFloorIsNotADeletion(): void {
  // Remote only carries clips from t=100 on, because that is all that fitted,
  // and says so in its floor.
  const local = [clip("old", 1), clip("new", 100)];
  const base = baseFromClips(local);
  const remote = [clip("new", 100)];
  assertEquals(
    ids(mergeClips(local, remote, base, 100)),
    "new,old",
    "the older one stays",
  );
}

function testPinChangeWinsByTime(): void {
  const local = [clip("a", 1, { updatedAt: 5 })];
  const remote = [clip("a", 1, { updatedAt: 9, pinned: true })];
  const merged = mergeClips(local, remote, baseFromClips([clip("a", 1)]), 0);
  assertEquals(merged.length, 1, "still one clip");
  assertEquals(merged[0].pinned, true, "the later pin wins");
}

function testLocalEditSurvivesARemoteDeletion(): void {
  const local = [clip("a", 1, { updatedAt: 50, pinned: true })];
  const base = { a: 1 };
  assertEquals(
    ids(mergeClips(local, [], base, 0)),
    "a",
    "the edit keeps it here",
  );
}

function testADeletedClipDoesNotComeBack(): void {
  // We published a and b, then deleted a. The other device had not heard yet
  // and sends both back: without the record of a leaving, a looks new.
  const published = [clip("a", 1), clip("b", 2)];
  const state = nextSyncState(
    { clips: baseFromClips(published), gone: {} },
    [clip("b", 2)],
    [clip("b", 2)],
    100,
  );
  assertEquals(state.gone.a, 100, "a is remembered as having left");
  assertEquals(
    ids(mergeClips([clip("b", 2)], published, baseOf(state), 0)),
    "b",
    "a does not come home",
  );
}

function testTheOtherDeviceEditingWinsOverOurDeletion(): void {
  // Same deletion, but there the clip was pinned after we let it go.
  const published = [clip("a", 1)];
  const state = nextSyncState(
    { clips: baseFromClips(published), gone: {} },
    [],
    [],
    100,
  );
  const remote = [clip("a", 1, { updatedAt: 200, pinned: true })];
  assertEquals(
    ids(mergeClips([], remote, baseOf(state), 0)),
    "a",
    "the edit brings it back",
  );
}

function testARecordOfLeavingIsLetGoEventually(): void {
  const state = nextSyncState(
    { clips: {}, gone: { a: 1 } },
    [],
    [],
    1 + GONE_KEPT_MS + 1,
  );
  assertEquals("a" in state.gone, false, "the record is let go");
}

function testAStateWrittenBeforeThoseRecordsStillReads(): void {
  const state = parseSyncState(JSON.stringify({ a: 5 }));
  assertEquals(state.clips.a, 5, "the bare map reads as published clips");
  assertEquals(
    Object.keys(state.gone).length,
    0,
    "nothing is remembered as having left",
  );
}

function testAnOlderDeletionIsBelievedWhenNothingWasDropped(): void {
  // Remote carries every clip it has — floor 0 — and the old one is not among
  // them, so it was deleted. Reading the oldest arrival as the floor would
  // have kept it here forever.
  const local = [clip("old", 1), clip("new", 100)];
  const base = baseFromClips(local);
  const remote = [clip("new", 100)];
  assertEquals(
    ids(mergeClips(local, remote, base, 0)),
    "new",
    "the old one is let go",
  );
}

function testNotFittingIsNotLeaving(): void {
  // b did not fit this time, but it is still here and the other device still
  // has what we sent it. Only a is really gone.
  const published = [clip("a", 1), clip("b", 2)];
  const state = nextSyncState(
    { clips: baseFromClips(published), gone: {} },
    [],
    [clip("b", 2)],
    100,
  );
  assertEquals(state.gone.a, 100, "a is remembered as having left");
  assertEquals("b" in state.gone, false, "b is not");
  assertEquals(state.clips.b, 2, "b is still something they have of ours");
}

function testNothingFitsWhenTheFirstIsTooBig(): void {
  // The case the store guards: a pinned clip too big to travel sorts first,
  // and the selection comes back empty although clips are still held.
  const huge = clip("huge", 1, {
    text: "x".repeat(MAX_SYNC_BYTES + 1),
    pinned: true,
  });
  const { payload, dropped } = selectForSync([huge, clip("small", 2)]);
  assertEquals(payload.clips.length, 0, "nothing fitted");
  assertEquals(dropped, 2, "both stayed home");
}

function testSelectionStaysUnderTheCap(): void {
  const big = "x".repeat(50 * 1024);
  const many = Array.from(
    { length: 40 },
    (_, i) => clip(`c${i}`, i, { text: big }),
  );
  const { payload, dropped } = selectForSync(many);
  const size = new TextEncoder().encode(JSON.stringify(payload)).length;
  assertEquals(size <= MAX_SYNC_BYTES, true, "the payload fits");
  assertEquals(dropped > 0, true, "some clips did not fit");
  assertEquals(
    payload.floor > 0,
    true,
    "a floor is set once something is dropped",
  );
}

function testPinnedClipsTravelFirst(): void {
  const big = "x".repeat(200 * 1024);
  const clips = [
    clip("pinned-old", 1, { text: big, pinned: true }),
    clip("plain-new", 9, { text: big }),
    clip("plain-newer", 10, { text: big }),
  ];
  const { payload } = selectForSync(clips);
  assertEquals(
    payload.clips.some((c) => c.id === "pinned-old"),
    true,
    "the pinned clip travels",
  );
}

function testNothingDroppedMeansNoFloor(): void {
  const { payload, dropped } = selectForSync([clip("a", 1), clip("b", 2)]);
  assertEquals(dropped, 0, "nothing dropped");
  assertEquals(payload.floor, 0, "no floor");
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
    {
      name: "a deleted clip does not come back",
      fn: testADeletedClipDoesNotComeBack,
    },
    {
      name: "the other device editing wins over our deletion",
      fn: testTheOtherDeviceEditingWinsOverOurDeletion,
    },
    {
      name: "a record of leaving is let go eventually",
      fn: testARecordOfLeavingIsLetGoEventually,
    },
    {
      name: "a state written before those records still reads",
      fn: testAStateWrittenBeforeThoseRecordsStillReads,
    },
    {
      name: "an older deletion is believed when nothing was dropped",
      fn: testAnOlderDeletionIsBelievedWhenNothingWasDropped,
    },
    { name: "not fitting is not leaving", fn: testNotFittingIsNotLeaving },
    {
      name: "nothing fits when the first is too big",
      fn: testNothingFitsWhenTheFirstIsTooBig,
    },
    {
      name: "selection stays under the cap",
      fn: testSelectionStaysUnderTheCap,
    },
    { name: "pinned clips travel first", fn: testPinnedClipsTravelFirst },
    {
      name: "nothing dropped means no floor",
      fn: testNothingDroppedMeansNoFloor,
    },
  ];

  await runTests("sync.test.ts", tests);
}
