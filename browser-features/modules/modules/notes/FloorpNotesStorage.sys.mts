// SPDX-License-Identifier: MPL-2.0

import type {
  NotesPreferenceSnapshot,
  NotesSyncContext,
  NotesSyncStateRead,
} from "../../common/notes-storage-types.ts";
import {
  decodeNotes,
  decodeNotesBase,
  encodeNotes,
  notesBase,
} from "../../common/notes-record.ts";
import { mergeNotesThreeWay } from "../../../pages-notes/src/lib/merge.ts";

const NOTES_PREF = "floorp.browser.note.memos";
const BASE_PREF = "floorp.browser.note.syncState";
const CONTEXT_CHANGED = "floorp-notes:context-changed";
const ACCOUNT_TOPICS = [
  "weave:service:start-over",
  "fxaccounts:onlogout",
  "fxaccounts:onlogin",
];
let epoch = 0;
let revision = 0;
let accountUid: string | null | undefined;
let accountLookup: Promise<string | null> | null = null;
let localWrite: { baseValue?: string | null } | null = null;
let incoming: {
  value: string;
  merged: string;
  baseValue: string | null;
  accountUid: string;
  delivered: boolean;
} | null = null;

function readNotes(): string | null {
  return Services.prefs.getPrefType(NOTES_PREF) === Services.prefs.PREF_STRING
    ? Services.prefs.getStringPref(NOTES_PREF)
    : null;
}
function localNotes(value: string | null) {
  return value
    ? decodeNotes(
      value,
      true,
      () => Services.uuid.generateUUID().toString().replace(/[{}]/g, ""),
    )
    : [];
}
let currentSnapshot: NotesPreferenceSnapshot = {
  value: readNotes(),
  epoch,
  revision,
  origin: "local",
};

function assertEpoch(expected: number): void {
  if (expected !== epoch) throw new Error("Notes account context changed");
}
function assertContext(context: NotesSyncContext): void {
  assertEpoch(context.epoch);
  if (
    context.accountUid !== null && accountUid !== undefined &&
    context.accountUid !== accountUid
  ) {
    throw new Error("Notes account identity changed");
  }
}
function invalidateAccount(): void {
  ++epoch;
  accountUid = undefined;
  accountLookup = null;
  if (Services.prefs.prefHasUserValue(BASE_PREF)) {
    Services.prefs.clearUserPref(BASE_PREF);
  }
  currentSnapshot = {
    value: readNotes(),
    epoch,
    revision: ++revision,
    origin: "local",
  };
  Services.obs.notifyObservers(Services.prefs, CONTEXT_CHANGED);
}
async function readAccount(expected: number): Promise<string | null> {
  if (!accountLookup) {
    const lookup = (async () => {
      const { getFxAccountsSingleton } = ChromeUtils.importESModule(
        "resource://gre/modules/FxAccounts.sys.mjs",
      ) as {
        getFxAccountsSingleton(): {
          getSignedInUser(): Promise<{ uid?: string } | null>;
        };
      };
      const user = await getFxAccountsSingleton().getSignedInUser();
      assertEpoch(expected);
      const uid = user?.uid ?? null;
      if (accountUid !== undefined && accountUid !== uid) {
        invalidateAccount();
        throw new Error("Notes account identity changed");
      }
      accountUid = uid;
      return uid;
    })();
    accountLookup = lookup;
    lookup.finally(() => {
      if (accountLookup === lookup) accountLookup = null;
    }).catch(() => {});
  }
  return await accountLookup;
}
function boundBase(uid: string | null): string | null {
  if (!uid) return null;
  const raw = Services.prefs.getStringPref(BASE_PREF, "");
  try {
    const parsed: unknown = raw ? JSON.parse(raw) : null;
    if (
      typeof parsed === "object" && parsed !== null && "accountUid" in parsed &&
      parsed.accountUid === uid
    ) return raw;
  } catch { /* An unreadable base cannot authorize deletion. */ }
  if (raw && Services.prefs.prefHasUserValue(BASE_PREF)) {
    Services.prefs.clearUserPref(BASE_PREF);
  }
  return null;
}
function acknowledge(value: string, uid: string): void {
  const intended = JSON.stringify({
    accountUid: uid,
    lastSyncedSnapshots: notesBase(decodeNotes(value)),
    lastSyncTime: Date.now(),
  });
  Services.prefs.setStringPref(BASE_PREF, intended);
  // Locked native prefs may accept a hidden user value while retaining an old
  // effective value. A setter return alone is not a durable acknowledgement.
  if (Services.prefs.getStringPref(BASE_PREF, "") !== intended) {
    throw new Error("Notes sync acknowledgement did not persist");
  }
}
function syncSnapshot(
  value: string | null,
  source: NonNullable<typeof incoming>,
): NotesPreferenceSnapshot {
  return {
    value,
    epoch,
    revision: ++revision,
    origin: "sync",
    incomingValue: source.value,
    baseValue: source.baseValue,
    accountUid: source.accountUid,
  };
}
const observer = {
  observe(_subject: unknown, topic: string, data: string) {
    if (topic === "nsPref:changed" && data === NOTES_PREF) {
      const value = readNotes();
      if (localWrite) {
        currentSnapshot = {
          value,
          epoch,
          revision: ++revision,
          origin: "local",
          baseNotesValue: localWrite.baseValue,
        };
      } else if (incoming && value === incoming.merged) {
        incoming.delivered = true;
        currentSnapshot = syncSnapshot(value, incoming);
      } else {
        currentSnapshot = {
          value,
          epoch,
          revision: ++revision,
          origin: "external",
        };
      }
    } else if (ACCOUNT_TOPICS.includes(topic)) invalidateAccount();
    else if (topic === "profile-before-change") {
      Services.prefs.removeObserver(NOTES_PREF, observer);
      for (const accountTopic of ACCOUNT_TOPICS) {
        Services.obs.removeObserver(observer, accountTopic);
      }
      Services.obs.removeObserver(observer, "profile-before-change");
    }
  },
};
Services.prefs.addObserver(NOTES_PREF, observer);
for (const topic of [...ACCOUNT_TOPICS, "profile-before-change"]) {
  Services.obs.addObserver(observer, topic);
}

export const FloorpNotesStorage = {
  getSnapshot(): NotesPreferenceSnapshot {
    const value = readNotes();
    if (value) {
      try {
        const normalized = encodeNotes(localNotes(value));
        if (normalized !== value) this.writeLocal(normalized);
      } catch { /* Preserve unreadable local data for recovery. */ }
    }
    return { ...currentSnapshot };
  },

  writeLocal(
    value: string,
    context?: NotesSyncContext,
    baseValue?: string | null,
  ): NotesPreferenceSnapshot {
    if (context) assertContext(context);
    const next = decodeNotes(value);
    const current = readNotes();
    const merged = baseValue !== undefined && current !== baseValue
      ? mergeNotesThreeWay(
        next,
        localNotes(current),
        notesBase(localNotes(baseValue)),
      ).merged
      : next;
    const previous = localWrite;
    localWrite = { baseValue };
    try {
      Services.prefs.setStringPref(NOTES_PREF, encodeNotes(merged));
    } finally {
      localWrite = previous;
    }
    return { ...currentSnapshot };
  },

  async readSyncState(
    snapshot: NotesPreferenceSnapshot,
  ): Promise<NotesSyncStateRead> {
    assertEpoch(snapshot.epoch);
    let uid: string | null;
    try {
      uid = await readAccount(snapshot.epoch);
    } catch (error) {
      assertEpoch(snapshot.epoch);
      console.warn(
        "[Floorp Notes] Account unavailable; retaining local notes.",
        error,
      );
      uid = null;
    }
    assertEpoch(snapshot.epoch);
    const context: NotesSyncContext = {
      epoch,
      revision: snapshot.revision,
      accountUid: uid,
      origin: snapshot.origin,
    };
    if (snapshot.origin === "sync") {
      if (!uid || snapshot.accountUid !== uid) {
        throw new Error("Notes incoming account changed");
      }
      return { value: snapshot.baseValue ?? null, context };
    }
    if (snapshot.origin === "local" && snapshot.baseNotesValue !== undefined) {
      return {
        value: JSON.stringify({
          lastSyncedSnapshots: notesBase(localNotes(snapshot.baseNotesValue)),
          lastSyncTime: 0,
        }),
        context,
      };
    }
    return { value: boundBase(uid), context };
  },

  async applyIncoming(
    value: string,
    apply: (merged: string) => Promise<unknown>,
    expectedEpoch = epoch,
  ): Promise<{ result: unknown; needsUpload: boolean }> {
    assertEpoch(expectedEpoch);
    const remote = decodeNotes(value);
    const expected = expectedEpoch;
    const uid = await readAccount(expected);
    assertEpoch(expected);
    if (!uid) throw new Error("Notes Sync requires a signed-in account");
    const baseValue = boundBase(uid);
    const localValue = readNotes();
    const local = localNotes(localValue);
    const mergedNotes =
      mergeNotesThreeWay(local, remote, decodeNotesBase(baseValue)).merged;
    const encoded = encodeNotes(mergedNotes);
    const merged = localValue && encodeNotes(local) === encoded
      ? localValue
      : encoded;
    const source: NonNullable<typeof incoming> = {
      value,
      merged,
      baseValue,
      accountUid: uid,
      delivered: false,
    };
    incoming = source;
    try {
      const applying = apply(merged);
      if (!source.delivered && readNotes() !== merged) {
        await applying;
        throw new Error("Notes incoming preference did not persist");
      }
      if (!source.delivered) {
        currentSnapshot = syncSnapshot(readNotes(), source);
        Services.obs.notifyObservers(Services.prefs, CONTEXT_CHANGED);
      }
      const result = await applying;
      assertEpoch(expected);
      if (accountUid !== uid) throw new Error("Notes incoming account changed");
      acknowledge(value, uid);
      return {
        result,
        needsUpload:
          encodeNotes(localNotes(readNotes())) !== encodeNotes(remote),
      };
    } finally {
      if (incoming === source) incoming = null;
    }
  },

  async prepareUpload(): Promise<NotesSyncContext> {
    const expected = epoch;
    const uid = await readAccount(expected);
    assertEpoch(expected);
    if (!uid) throw new Error("Notes Sync requires a signed-in account");
    return { epoch, revision, accountUid: uid, origin: "local" };
  },

  async acknowledgeUploaded(
    value: string,
    context: NotesSyncContext,
  ): Promise<void> {
    assertContext(context);
    const uid = await readAccount(context.epoch);
    assertEpoch(context.epoch);
    if (!uid || uid !== context.accountUid) {
      throw new Error("Notes upload account changed");
    }
    // This is the exact PrefRec payload accepted by the server, which may be
    // older than a local edit made while the upload was in flight.
    acknowledge(value, uid);
  },
};
