// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";
import { encodeNotes } from "../../../common/notes-record.ts";
import type {
  NotesPreferenceSnapshot,
  NotesStorageService,
} from "../../../common/notes-storage-types.ts";

function testSnapshotsDuringNativeNotification(): void {
  const pref = "floorp.browser.note.memos";
  const basePref = "floorp.browser.note.syncState";
  assert(!Services.prefs.prefIsLocked(pref), "The test Notes pref is writable");
  const hadUserValue = Services.prefs.prefHasUserValue(pref);
  assert(
    !hadUserValue ||
      Services.prefs.getPrefType(pref) === Services.prefs.PREF_STRING,
    "An existing Notes user value must be a string",
  );
  const previous = hadUserValue ? Services.prefs.getStringPref(pref) : null;
  const baseType = Services.prefs.getPrefType(basePref);
  const hadBaseUserValue = Services.prefs.prefHasUserValue(basePref);
  const previousBase = baseType === Services.prefs.PREF_STRING
    ? Services.prefs.getStringPref(basePref)
    : null;
  const { FloorpNotesStorage: store } = ChromeUtils.importESModule(
    "resource://noraneko/modules/notes/FloorpNotesStorage.sys.mjs",
  ) as { FloorpNotesStorage: NotesStorageService };
  const nonce = Services.uuid.generateUUID().toString();
  const welcome = {
    id: `notes-observer-welcome-${nonce}`,
    title: "Welcome",
    content: "",
    createdAt: 1,
    updatedAt: 1,
  };
  const created = {
    ...welcome,
    id: `notes-observer-new-${nonce}`,
    title: "New Note",
  };
  const titled = { ...created, title: "QA title", updatedAt: 2 };
  const body = { ...titled, content: "QA body", updatedAt: 3 };
  const observed: NotesPreferenceSnapshot[][] = [[], []];
  const callbackErrors: unknown[] = [];
  const consumers = observed.map((snapshots) => ({
    observe(_subject: unknown, topic: string, data: string) {
      if (topic !== "nsPref:changed" || data !== pref) return;
      // Native observer exceptions may be reported rather than propagated.
      // Capture first and assert outside the callback so failure cannot hide.
      try {
        snapshots.push(store.getSnapshot());
      } catch (error) {
        callbackErrors.push(error);
      }
    },
  }));
  const registered: typeof consumers = [];
  try {
    store.writeLocal(encodeNotes([welcome]));
    // Both consumers register after the actual singleton's storage observer.
    // On the locked Runtime they run first, through real native prefs.
    for (const consumer of consumers) {
      Services.prefs.addObserver(pref, consumer);
      registered.push(consumer);
    }
    for (const edited of [created, titled, body]) {
      const before = store.getSnapshot();
      const value = encodeNotes([welcome, edited]);
      const saved = store.writeLocal(value, undefined, before.value);
      assertEquals(callbackErrors.length, 0, "Consumer reads must not throw");
      for (const snapshots of observed) {
        const captured = snapshots.at(-1)!;
        assertEquals(captured.value, value, "The callback sees this save");
        assertEquals(captured.origin, "local", "Local provenance is retained");
        assertEquals(
          captured.baseNotesValue,
          before.value,
          "The base is this write's ancestor",
        );
        assertEquals(captured.epoch, before.epoch, "No account epoch changed");
        assertEquals(
          captured.revision,
          before.revision + 1,
          "One revision per changed pref",
        );
        assertEquals(
          captured.revision,
          saved.revision,
          "The late storage observer does not publish twice",
        );
        assertEquals(
          captured.value,
          saved.value,
          "The returned snapshot agrees",
        );
      }
      assertEquals(
        store.getSnapshot().revision,
        saved.revision,
        "Reads are idempotent",
      );
    }
    assertEquals(observed[0].length, 3, "First consumer received every save");
    assertEquals(observed[1].length, 3, "Second consumer received every save");
    assertEquals(
      Services.prefs.getPrefType(basePref),
      baseType,
      "No Sync base type changed",
    );
    assertEquals(
      Services.prefs.prefHasUserValue(basePref),
      hadBaseUserValue,
      "No Sync base was created",
    );
    if (previousBase !== null) {
      assertEquals(
        Services.prefs.getStringPref(basePref),
        previousBase,
        "Local writes do not acknowledge Sync",
      );
    }
  } finally {
    for (const consumer of registered) {
      Services.prefs.removeObserver(pref, consumer);
    }
    if (hadUserValue) Services.prefs.setStringPref(pref, previous!);
    else Services.prefs.clearUserPref(pref);
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("FloorpNotesStorage.test.mts", [{
    name: "native consumers observe current create, title and body snapshots",
    fn: testSnapshotsDuringNativeNotification,
  }]);
}
