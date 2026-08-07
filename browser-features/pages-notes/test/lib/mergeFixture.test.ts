// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

/**
 * Runs the approved cross-client Floorp Notes merge fixture
 * (sync-fixtures/floorp-notes/floorp-notes-merge-v1.json, digest
 * 2597e5311c7c4ea4bb9d6a806ffa183aae3b3bd7380893b664b02ac829d665fd) through
 * the Desktop three-way merge so iOS and Desktop derive identical winners,
 * conflict copies, and ordering.
 */

import {
  assertEquals,
  assert,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";
import {
  mergeNotesThreeWay,
  type Note,
  type NoteSnapshot,
} from "../../src/lib/merge.ts";

interface FixtureNote {
  id: string;
  title: string;
  content: string;
  createdAt: number;
  updatedAt: number;
}

interface MergeCase {
  name: string;
  base: FixtureNote[];
  local: FixtureNote[];
  remote: FixtureNote[];
  expectedNotes: FixtureNote[];
  expectedConflicts: { originalNoteID: string; conflictCopyID: string }[];
}

interface SequenceCase {
  name: string;
  steps: MergeCase[];
}

interface Fixture {
  requiredCaseNames: string[];
  mergeCases: MergeCase[];
  sequenceCases: SequenceCase[];
  errorCases: { name: string }[];
}

const fixtureUrl = new URL(
  "../fixtures/floorp-notes-merge-v1.json",
  import.meta.url,
);
const fixture: Fixture = JSON.parse(await Deno.readTextFile(fixtureUrl));

const EXPECTED_DIGEST =
  "2597e5311c7c4ea4bb9d6a806ffa183aae3b3bd7380893b664b02ac829d665fd";

function toNotes(notes: FixtureNote[]): Note[] {
  return notes.map((note) => ({
    id: note.id,
    title: note.title,
    content: note.content,
    createdAt: note.createdAt,
    updatedAt: note.updatedAt,
  }));
}

function toSnapshots(notes: FixtureNote[]): Record<string, NoteSnapshot> {
  const snapshots: Record<string, NoteSnapshot> = {};
  for (const note of notes) {
    snapshots[note.id] = {
      id: note.id,
      title: note.title,
      content: note.content,
      updatedAt: note.updatedAt,
    };
  }
  return snapshots;
}

function noteKey(note: FixtureNote): string {
  return [
    note.id,
    note.title,
    note.content,
    note.createdAt,
    note.updatedAt,
  ].join("\u0000");
}

const tests: TestCase[] = [];

for (const mergeCase of fixture.mergeCases) {
  tests.push({
    name: `merge: ${mergeCase.name}`,
    fn: () => {
      const result = mergeNotesThreeWay(
        toNotes(mergeCase.local),
        toNotes(mergeCase.remote),
        toSnapshots(mergeCase.base),
      );
      const expectedKeys = mergeCase.expectedNotes.map(noteKey).sort();
      const actualKeys = result.merged.map(noteKey).sort();
      assertEquals(actualKeys, expectedKeys, `notes for ${mergeCase.name}`);

      for (const conflict of mergeCase.expectedConflicts) {
        const original = result.merged.find((n) => n.id === conflict.originalNoteID);
        const copy = result.merged.find((n) => n.id === conflict.conflictCopyID);
        assert(original, `original ${conflict.originalNoteID} missing`);
        assert(copy, `conflict copy ${conflict.conflictCopyID} missing`);
      }
      assertEquals(
        result.conflictCount,
        mergeCase.expectedConflicts.length,
        `conflict count for ${mergeCase.name}`,
      );
    },
  });
}

for (const sequenceCase of fixture.sequenceCases ?? []) {
  for (const step of sequenceCase.steps ?? []) {
    tests.push({
      name: `sequence ${sequenceCase.name}/${step.name}`,
      fn: () => {
        const result = mergeNotesThreeWay(
          toNotes(step.local),
          toNotes(step.remote),
          toSnapshots(step.base),
        );
        const expectedKeys = step.expectedNotes.map(noteKey).sort();
        const actualKeys = result.merged.map(noteKey).sort();
        assertEquals(actualKeys, expectedKeys, `notes for ${step.name}`);
        assertEquals(
          result.conflictCount,
          step.expectedConflicts.length,
          `conflict count for ${step.name}`,
        );
      },
    });
  }
}

tests.push({
  name: "fixture digest matches the approved contract",
  fn: async () => {
    const bytes = await Deno.readFile(fixtureUrl);
    const digest = await crypto.subtle.digest("SHA-256", bytes);
    const hex = [...new Uint8Array(digest)]
      .map((b) => b.toString(16).padStart(2, "0"))
      .join("");
    assertEquals(hex, EXPECTED_DIGEST, "fixture digest drift");
  },
});

tests.push({
  name: "all required case names are covered",
  fn: () => {
    const covered = new Set(fixture.mergeCases.map((c) => c.name));
    for (const required of fixture.requiredCaseNames) {
      assert(covered.has(required), `required case missing: ${required}`);
    }
  },
});

export async function runAllTests(): Promise<void> {
  await runTests("mergeFixture.test.ts", tests);
}
