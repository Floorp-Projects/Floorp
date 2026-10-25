// SPDX-License-Identifier: MPL-2.0

import type {
  NotesStorageService,
  NotesSyncContext,
} from "../../common/notes-storage-types.ts";
import { decodeNotes } from "../../common/notes-record.ts";

// Use the same profile ESM instance as every Notes page and actor, independent
// of bundler entry-point splitting.
const { FloorpNotesStorage } = ChromeUtils.importESModule(
  "resource://noraneko/modules/notes/FloorpNotesStorage.sys.mjs",
) as { FloorpNotesStorage: NotesStorageService };
const { PrefsEngine: NativePrefsEngine } = ChromeUtils.importESModule(
  "resource://services-sync/engines/prefs.sys.mjs",
);
const { Utils } = ChromeUtils.importESModule(
  "resource://services-sync/util.sys.mjs",
);
const NOTES_PREF = "floorp.browser.note.memos";
const CONTROL_PREF = `services.sync.prefs.sync.${NOTES_PREF}`;
interface NotesPrefRecord {
  id: string;
  value: Record<string, unknown>;
  cleartext: unknown;
  deleted?: boolean;
}

function enabled(): boolean {
  return Services.prefs.getBoolPref(CONTROL_PREF, false);
}
function validNotes(value: unknown): value is string {
  if (typeof value !== "string") return false;
  try {
    decodeNotes(value);
    return true;
  } catch {
    return false;
  }
}

class NotesPrefStore extends NativePrefsEngine.prototype._storeObj {
  declare engine: FloorpNotesPrefsEngine;

  async update(record: NotesPrefRecord): Promise<unknown> {
    if (!await this.itemExists(record.id)) return await super.update(record);
    const expected = FloorpNotesStorage.getSnapshot().epoch;
    if (Object.hasOwn(record.value, CONTROL_PREF)) {
      await super.update({
        id: record.id,
        value: { [CONTROL_PREF]: record.value[CONTROL_PREF] },
      });
    }
    if (!enabled()) return await super.update(record);

    const raw = record.value[NOTES_PREF];
    if (!validNotes(raw)) {
      const values = { ...record.value };
      delete values[NOTES_PREF];
      const result = await super.update({ id: record.id, value: values });
      if (validNotes(FloorpNotesStorage.getSnapshot().value)) {
        this.engine.notesChanged(record.id, expected);
      }
      return result;
    }
    let applied = false;
    try {
      const outcome = await FloorpNotesStorage.applyIncoming(
        raw,
        async (merged) => {
          applied = true;
          const result = await super.update({
            id: record.id,
            value: { ...record.value, [NOTES_PREF]: merged },
          });
          return result;
        },
        expected,
      );
      if (outcome.needsUpload) this.engine.notesChanged(record.id, expected);
      return outcome.result;
    } catch (error) {
      this.engine.notesChanged(record.id, expected);
      if (
        !applied && FloorpNotesStorage.getSnapshot().epoch === expected
      ) {
        const values = { ...record.value };
        delete values[NOTES_PREF];
        try {
          await super.update({ id: record.id, value: values });
        } catch (fallbackError) {
          console.error(
            "[Floorp Notes] Failed to apply unrelated preferences.",
            fallbackError,
          );
        }
      }
      throw error;
    } finally {
      this._incomingPrefs = record.value;
    }
  }
}

export class FloorpNotesPrefsEngine extends NativePrefsEngine {
  private notesPending = new Map<string, number>();
  private notesUpload: {
    context: NotesSyncContext;
    record?: { id: string; value: string };
  } | null = null;

  notesChanged(id: string, expected: number): void {
    if (FloorpNotesStorage.getSnapshot().epoch !== expected) return;
    this.notesPending.set(id, expected);
    this._tracker.modified = true;
  }

  async _processIncoming(...args: unknown[]): Promise<unknown> {
    try {
      return await super._processIncoming(...args);
    } finally {
      const currentEpoch = FloorpNotesStorage.getSnapshot().epoch;
      for (const [id, expected] of this.notesPending) {
        if (expected === currentEpoch) this._modified.set(id, 0);
      }
      this.notesPending.clear();
    }
  }

  async _reconcile(record: NotesPrefRecord): Promise<boolean> {
    const expected = FloorpNotesStorage.getSnapshot().epoch;
    const shouldApply = await super._reconcile(record);
    if (
      !shouldApply && !record.deleted && enabled() &&
      validNotes(record.value?.[NOTES_PREF]) &&
      await this._store.itemExists(record.id)
    ) {
      const localRecord = await this._createRecord(record.id);
      if (Utils.deepEquals(record.cleartext, localRecord.cleartext)) {
        try {
          const outcome = await FloorpNotesStorage.applyIncoming(
            record.value[NOTES_PREF] as string,
            () => Promise.resolve(undefined),
            expected,
          );
          if (outcome.needsUpload) this.notesChanged(record.id, expected);
        } catch (error) {
          this.notesChanged(record.id, expected);
          throw error;
        }
      }
    }
    return shouldApply;
  }

  async _uploadOutgoing(): Promise<unknown> {
    if (
      !enabled() || this._modified.count() === 0 ||
      !validNotes(FloorpNotesStorage.getSnapshot().value)
    ) return await super._uploadOutgoing();
    this.notesUpload = {
      context: await FloorpNotesStorage.prepareUpload(),
    };
    try {
      return await super._uploadOutgoing();
    } finally {
      this.notesUpload = null;
    }
  }

  async _createRecord(id: string): Promise<NotesPrefRecord> {
    const record = await super._createRecord(id);
    const value = record.value?.[NOTES_PREF];
    if (this.notesUpload && !record.deleted && enabled() && validNotes(value)) {
      this.notesUpload.record = { id: record.id, value };
    }
    return record;
  }

  async _onRecordsWritten(
    successful: string[],
    failed: string[],
    serverTime: number,
  ): Promise<void> {
    await super._onRecordsWritten(successful, failed, serverTime);
    const upload = this.notesUpload;
    if (!upload?.record || !successful.includes(upload.record.id)) return;
    try {
      await FloorpNotesStorage.acknowledgeUploaded(
        upload.record.value,
        upload.context,
      );
    } catch (error) {
      // Native Sync already removed successful IDs before invoking this hook.
      // Keep this acknowledgement failure retryable in memory and on disk.
      if (FloorpNotesStorage.getSnapshot().epoch === upload.context.epoch) {
        this._modified.set(upload.record.id, 0);
        this._tracker.modified = true;
      }
      throw error;
    }
  }
}
FloorpNotesPrefsEngine.prototype._storeObj = NotesPrefStore;
