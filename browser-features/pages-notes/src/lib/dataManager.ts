import { rpc } from "./rpc/rpc.ts";
import type { NoteSnapshot } from "./merge.ts";
import type { Note } from "../types/note.ts";
import type {
  NotesPreferenceSnapshot,
  NotesSyncContext,
} from "../../../modules/common/notes-storage-types.ts";

export {
  canonicalData,
  mergeNotesThreeWay,
  type MergeResult,
  type NoteSnapshot,
  precedes,
} from "./merge.ts";

export const NOTES_PREF_NAME = "floorp.browser.note.memos";

export interface NotesData {
  ids?: string[];
  titles: string[];
  contents: string[];
  createdAts?: number[];
  updatedAts?: number[];
}

function getDefaultNotes(t: (key: string) => string): NotesData {
  const now = Date.now();
  return {
    ids: [crypto.randomUUID()],
    titles: [t("notes.welcome")],
    contents: [t("notes.welcomeContent")],
    createdAts: [now],
    updatedAts: [now],
  };
}

export async function getNotes(
  t: (key: string) => string,
  value?: string | null,
): Promise<NotesData> {
  try {
    const notesStr = value === undefined
      ? await rpc.getStringPref(NOTES_PREF_NAME)
      : value;
    if (!notesStr) {
      return getDefaultNotes(t);
    }

    const parsedNotes = JSON.parse(notesStr) as NotesData;

    // Migrate legacy data
    const len = parsedNotes.titles.length;
    if (!parsedNotes.ids) {
      parsedNotes.ids = Array.from({ length: len }, () => crypto.randomUUID());
    }
    if (!parsedNotes.createdAts) {
      parsedNotes.createdAts = Array.from({ length: len }, () => Date.now());
    }
    if (!parsedNotes.updatedAts) {
      parsedNotes.updatedAts = Array.from({ length: len }, () => Date.now());
    }

    return parsedNotes;
  } catch (e) {
    console.error("Failed to load note data:", e);
    return getDefaultNotes(t);
  }
}

export async function saveNotes(
  data: NotesData,
  context?: NotesSyncContext,
  baseValue?: string | null,
) {
  try {
    return await rpc.writeNotesPref(JSON.stringify(data), context, baseValue);
  } catch (e) {
    console.error("Failed to save note data:", e);
    throw e;
  }
}

// ──────────────────────────────────────────────────────────
// Three-Way Merge for Sync
// ──────────────────────────────────────────────────────────

/** Persistent sync state used as the "base" for three-way merge. */
export interface SyncState {
  /** Snapshots keyed by note ID. */
  lastSyncedSnapshots: Record<string, NoteSnapshot>;
  /** Timestamp (ms since epoch) of the last successful sync. */
  lastSyncTime: number;
}

/** Creates an empty sync state (for first sync). */
function emptySyncState(): SyncState {
  return { lastSyncedSnapshots: {}, lastSyncTime: 0 };
}

/** Creates a SyncState from an array of merged notes. */
export function syncStateFromNotes(notes: Note[]): SyncState {
  const snapshots: Record<string, NoteSnapshot> = {};
  for (const note of notes) {
    snapshots[note.id] = {
      id: note.id,
      title: note.title,
      content: note.content,
      updatedAt: note.updatedAt,
    };
  }
  return { lastSyncedSnapshots: snapshots, lastSyncTime: Date.now() };
}

/** Loads the last sync state from preferences. */
export async function loadSyncState(
  snapshot: NotesPreferenceSnapshot,
): Promise<{ state: SyncState; context: NotesSyncContext }> {
  const read = await rpc.readNotesSyncState(snapshot);
  try {
    const raw = read.value;
    if (!raw) return { state: emptySyncState(), context: read.context };
    const parsed = JSON.parse(raw) as unknown;
    // Validate shape before trusting it
    if (
      typeof parsed === "object" && parsed !== null &&
      "lastSyncedSnapshots" in parsed && "lastSyncTime" in parsed &&
      typeof parsed.lastSyncTime === "number"
    ) {
      return { state: parsed as SyncState, context: read.context };
    }
    console.warn(
      "[Floorp Notes] Sync state pref has unexpected shape, resetting to empty.",
    );
    return { state: emptySyncState(), context: read.context };
  } catch (e) {
    console.warn("[Floorp Notes] Failed to parse sync state, resetting:", e);
    return { state: emptySyncState(), context: read.context };
  }
}

/**
 * Three-way merge of local and remote notes using the last-synced state as base.
 *
 * Algorithm:
 * 1. Build lookup maps for local, remote, and base (last synced) notes
 * 2. For each unique note ID, determine what changed:
 *    - Only local changed → keep local
 *    - Only remote changed → keep remote
 *    - Both unchanged → keep either (same content)
 *    - Both changed → CONFLICT: keep newer, create "(conflict)" copy of older
 * 3. Handle deletions:
 *    - Deleted locally + unchanged remotely → respect local deletion
 *    - Deleted remotely + unchanged locally → respect remote deletion
 *    - Deleted + changed on other side → keep the changed version (edit wins over delete)
 * 4. New notes (not in base) are always kept from whichever side has them
 *
 * @param local  - Current notes in local storage
 * @param remote - Notes from the sync server
 * @param base   - Snapshots from the last successful sync
 * @returns Merge result with merged notes and conflict metadata
 */
// ──────────────────────────────────────────────────────────
// Conversion helpers (NotesData ↔ Note[])
// ──────────────────────────────────────────────────────────

/** Convert parallel-array `NotesData` to a `Note[]`. */
export function notesDataToNotes(data: NotesData): Note[] {
  return data.titles.map((title, i) => ({
    id: data.ids?.[i] ?? crypto.randomUUID(),
    title: title || "",
    content: data.contents[i] || "",
    createdAt: data.createdAts?.[i] ?? Date.now(),
    updatedAt: data.updatedAts?.[i] ?? Date.now(),
  }));
}

/** Convert a `Note[]` to parallel-array `NotesData`. */
export function notesToNotesData(notes: Note[]): NotesData {
  return {
    ids: notes.map((n) => n.id),
    titles: notes.map((n) => n.title),
    contents: notes.map((n) => n.content),
    createdAts: notes.map((n) => n.createdAt),
    updatedAts: notes.map((n) => n.updatedAt),
  };
}
