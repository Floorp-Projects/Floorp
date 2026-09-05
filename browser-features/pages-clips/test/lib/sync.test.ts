// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  baseFromClips,
  baseOf,
  GONE_KEPT_MS,
  MAX_SYNC_BYTES,
  mergeClips,
  nextSyncState,
  parsePayload,
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
  assertEquals(ids(mergeClips(local, remote, {}, [])), "a,b", "both are kept");
}

function testDeletionOnTheOtherSideIsRespected(): void {
  // Both sides had "a" at the last sync; it is gone from remote now.
  const local = [clip("a", 1)];
  const base = baseFromClips([clip("a", 1)]);
  const remote = [clip("b", 1)];
  assertEquals(
    ids(mergeClips(local, remote, base, [])),
    "b",
    "the deleted one is let go",
  );
}

function testAClipNamedAsStayedHomeIsNotADeletion(): void {
  // Remote could not carry "old", and says so by name.
  const local = [clip("old", 1), clip("new", 100)];
  const base = baseFromClips(local);
  const remote = [clip("new", 100)];
  assertEquals(
    ids(mergeClips(local, remote, base, ["old"])),
    "new,old",
    "the one it could not carry stays",
  );
}

function testPinChangeWinsByTime(): void {
  const local = [clip("a", 1, { updatedAt: 5 })];
  const remote = [clip("a", 1, { updatedAt: 9, pinned: true })];
  const merged = mergeClips(local, remote, baseFromClips([clip("a", 1)]), []);
  assertEquals(merged.length, 1, "still one clip");
  assertEquals(merged[0].pinned, true, "the later pin wins");
}

function testLocalEditSurvivesARemoteDeletion(): void {
  const local = [clip("a", 1, { updatedAt: 50, pinned: true })];
  const base = { a: 1 };
  assertEquals(
    ids(mergeClips(local, [], base, [])),
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
    ids(mergeClips([clip("b", 2)], published, baseOf(state), [])),
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
    ids(mergeClips([], remote, baseOf(state), [])),
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

function testAnOlderDeletionIsBelievedWhenNothingStayedHome(): void {
  // Remote carries every clip it has and names nothing as stayed home, so the
  // old one's absence is a deletion.
  const local = [clip("old", 1), clip("new", 100)];
  const base = baseFromClips(local);
  const remote = [clip("new", 100)];
  assertEquals(
    ids(mergeClips(local, remote, base, [])),
    "new",
    "the old one is let go",
  );
}

function testADeletionIsBelievedWhileSomethingElseStayedHome(): void {
  // The other device is holding a clip too big to ever travel, and separately
  // deleted an older shared one. Naming what stayed home says exactly that;
  // a water line drawn above the big one would have buried the deletion.
  const local = [clip("old", 1), clip("huge", 500), clip("new", 900)];
  const base = baseFromClips(local);
  const remote = [clip("new", 900)];
  assertEquals(
    ids(mergeClips(local, remote, base, ["huge"])),
    "huge,new",
    "the big one stays and the deletion is believed",
  );
}

function testAPayloadThatDoesNotSayIsNotReadAsDeletions(): void {
  const read = parsePayload(
    JSON.stringify({ clips: [clip("new", 100)], floor: 0 }),
  );
  assertEquals(read?.stayed, null, "it does not say");
  const local = [clip("old", 1), clip("new", 100)];
  assertEquals(
    ids(mergeClips(local, read!.clips, baseFromClips(local), read!.stayed)),
    "new,old",
    "nothing is let go on a payload that does not say",
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

function testTooBigIsSteppedOver(): void {
  // A pinned clip too big to travel at all sorts first. It must not keep the
  // rest at home with it.
  const huge = clip("huge", 1, {
    text: "x".repeat(MAX_SYNC_BYTES + 1),
    pinned: true,
  });
  const { payload, dropped } = selectForSync([huge, clip("small", 2)]);
  assertEquals(ids(payload.clips), "small", "the small one still travels");
  assertEquals(dropped, 1, "only the big one stayed");
  assertEquals(payload.stayed.join(","), "huge", "and it is named");
}

function testNothingFitsWhenEveryClipIsTooBig(): void {
  // The case the store guards: the payload comes back empty although clips
  // are still held, and publishing it would read as "all deleted".
  const tooBig = (id: string, at: number) =>
    clip(id, at, { text: "x".repeat(MAX_SYNC_BYTES + 1) });
  const { payload, dropped } = selectForSync([tooBig("a", 1), tooBig("b", 2)]);
  assertEquals(payload.clips.length, 0, "nothing fitted");
  assertEquals(dropped, 2, "both stayed home");
}

function testAClipThatCouldNotTravelIsNotADeletion(): void {
  // A pinned old note travels because pinned clips go first; a newer clip does
  // not fit behind it. The floor has to speak for the one that stayed home,
  // not for the oldest one that travelled — otherwise the other device reads
  // the absence as a deletion and lets a clip go that nobody deleted.
  const big = "x".repeat(300 * 1024);
  const { payload } = selectForSync([
    clip("pinned-old", 1, { text: big, pinned: true }),
    clip("recent", 500, { text: big }),
  ]);
  assertEquals(ids(payload.clips), "pinned-old", "only the pinned one fitted");
  assertEquals(payload.stayed.join(","), "recent", "and the other is named");

  const local = [clip("recent", 500)];
  const base = baseFromClips(local);
  assertEquals(
    ids(mergeClips(local, payload.clips, base, payload.stayed)),
    "pinned-old,recent",
    "the one that could not travel stays",
  );
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
  assertEquals(payload.stayed.length, dropped, "what stayed is all named");
  assertEquals(dropped > 0, true, "some clips did not fit");
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

function testNothingDroppedMeansNothingNamed(): void {
  const { payload, dropped } = selectForSync([clip("a", 1), clip("b", 2)]);
  assertEquals(dropped, 0, "nothing dropped");
  assertEquals(payload.stayed.length, 0, "and nothing named");
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "new on either side is kept", fn: testNewOnEitherSideIsKept },
    {
      name: "deletion on the other side is respected",
      fn: testDeletionOnTheOtherSideIsRespected,
    },
    {
      name: "a clip named as stayed home is not a deletion",
      fn: testAClipNamedAsStayedHomeIsNotADeletion,
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
      name: "an older deletion is believed when nothing stayed home",
      fn: testAnOlderDeletionIsBelievedWhenNothingStayedHome,
    },
    {
      name: "a deletion is believed while something else stayed home",
      fn: testADeletionIsBelievedWhileSomethingElseStayedHome,
    },
    {
      name: "a payload that does not say is not read as deletions",
      fn: testAPayloadThatDoesNotSayIsNotReadAsDeletions,
    },
    { name: "not fitting is not leaving", fn: testNotFittingIsNotLeaving },
    { name: "too big is stepped over", fn: testTooBigIsSteppedOver },
    {
      name: "nothing fits when every clip is too big",
      fn: testNothingFitsWhenEveryClipIsTooBig,
    },
    {
      name: "a clip that could not travel is not a deletion",
      fn: testAClipThatCouldNotTravelIsNotADeletion,
    },
    {
      name: "selection stays under the cap",
      fn: testSelectionStaysUnderTheCap,
    },
    { name: "pinned clips travel first", fn: testPinnedClipsTravelFirst },
    {
      name: "nothing dropped means nothing named",
      fn: testNothingDroppedMeansNothingNamed,
    },
  ];

  await runTests("sync.test.ts", tests);
}
