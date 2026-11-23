export interface NotesPreferenceSnapshot {
  value: string | null;
  revision: number;
  epoch: number;
  origin: "local" | "external" | "sync";
  incomingValue?: string;
  baseValue?: string | null;
  baseNotesValue?: string | null;
  accountUid?: string;
}

export interface NotesSyncContext {
  epoch: number;
  revision: number;
  accountUid: string | null;
  origin: NotesPreferenceSnapshot["origin"];
}

export interface NotesSyncStateRead {
  value: string | null;
  context: NotesSyncContext;
}

export interface NotesStorageFunctions {
  getNotesSnapshot(): Promise<NotesPreferenceSnapshot>;
  readNotesSyncState(
    snapshot: NotesPreferenceSnapshot,
  ): Promise<NotesSyncStateRead>;
  writeNotesPref(
    value: string,
    context?: NotesSyncContext,
    baseValue?: string | null,
  ): Promise<NotesPreferenceSnapshot>;
}

export interface NotesStorageService {
  getSnapshot(): NotesPreferenceSnapshot;
  readSyncState(snapshot: NotesPreferenceSnapshot): Promise<NotesSyncStateRead>;
  writeLocal(
    value: string,
    context?: NotesSyncContext,
    baseValue?: string | null,
  ): NotesPreferenceSnapshot;
  applyIncoming(
    value: string,
    apply: (merged: string) => Promise<unknown>,
    expectedEpoch?: number,
  ): Promise<{ result: unknown; needsUpload: boolean }>;
  prepareUpload(): Promise<NotesSyncContext>;
  acknowledgeUploaded(value: string, context: NotesSyncContext): Promise<void>;
}
