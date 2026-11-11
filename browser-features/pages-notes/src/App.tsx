import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { NoteList } from "./components/notes/NoteList.tsx";
import { RichTextEditor } from "./components/editor/RichTextEditor.tsx";
import type { JSONContent } from "@tiptap/react";
import {
  getNotes,
  loadSyncState,
  mergeNotesThreeWay,
  NOTES_PREF_NAME,
  type NotesData,
  notesDataToNotes,
  notesToNotesData,
  saveNotes,
  syncStateFromNotes,
} from "./lib/dataManager.ts";
import { addPrefObserver, removePrefObserver, rpc } from "./lib/rpc/rpc.ts";
import { useTranslation } from "react-i18next";
import { ConfirmModal } from "./components/common/ConfirmModal.tsx";
import { SaveStatus } from "./components/common/SaveStatus.tsx";
import { NoteSearch } from "./components/notes/NoteSearch.tsx";
import type { Note } from "./types/note.ts";
import { extractPlainText } from "./lib/extractText.ts";
import { AlertCircle, Download } from "lucide-react";
import { noteToMarkdown } from "./lib/exportMarkdown.ts";
import { toExportFilename } from "./lib/exportFilename.ts";
import { saveTextFile } from "./lib/fileSink.ts";
import type {
  NotesPreferenceSnapshot,
  NotesSyncContext,
} from "../../modules/common/notes-storage-types.ts";

type SaveStatusType = "idle" | "saving" | "saved" | "error";

function App() {
  const { t } = useTranslation();
  const [notes, setNotes] = useState<Note[]>([]);
  const [selectedNote, setSelectedNote] = useState<Note | null>(null);
  const [title, setTitle] = useState("");
  const [isReorderMode, setIsReorderMode] = useState(false);
  const [isLoading, setIsLoading] = useState(true);
  const [saveStatus, setSaveStatus] = useState<SaveStatusType>("idle");
  const [deleteTarget, setDeleteTarget] = useState<string | null>(null);
  const [searchQuery, setSearchQuery] = useState("");
  const [exportError, setExportError] = useState(false);
  const saveTimeoutRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const notesRef = useRef<Note[]>([]);
  const acknowledgedValueRef = useRef<string | null>(null);
  const selectedNoteRef = useRef<Note | null>(null);
  const hasLoadedRef = useRef(false);
  const isMountedRef = useRef(true);
  const persistenceQueueRef = useRef<Promise<void>>(Promise.resolve());
  const ownWriteValuesRef = useRef(new Map<string, number>());
  const titleInputRef = useRef<HTMLInputElement>(null);
  const createButtonRef = useRef<HTMLButtonElement>(null);

  useEffect(() => {
    isMountedRef.current = true;
    return () => {
      isMountedRef.current = false;
    };
  }, []);

  const updateNotes = useCallback((update: (current: Note[]) => Note[]) => {
    const next = update(notesRef.current);
    notesRef.current = next;
    setNotes(next);
  }, []);

  const selectCurrentNote = useCallback((note: Note | null) => {
    selectedNoteRef.current = note;
    setSelectedNote(note);
    setTitle(note?.title ?? "");
  }, []);

  const enqueuePersistence = useCallback((operation: () => Promise<void>) => {
    const task = persistenceQueueRef.current.then(operation);
    persistenceQueueRef.current = task.catch(() => {});
    return task;
  }, []);

  const writeNotesPref = useCallback(
    async (
      notesToSave: Note[],
      context?: NotesSyncContext,
      baseValue = acknowledgedValueRef.current,
    ) => {
      const data = notesToNotesData(notesToSave);
      const value = JSON.stringify(data);
      const ownWrites = ownWriteValuesRef.current;
      ownWrites.set(value, (ownWrites.get(value) ?? 0) + 1);
      try {
        await saveNotes(data, context, baseValue);
        // CAS may persist changes from another window which notesRef has not
        // observed yet. Only the submitted UI state can be our next ancestor;
        // otherwise a later flush would treat those unseen notes as deletions.
        acknowledgedValueRef.current = value;
      } finally {
        const remaining = (ownWrites.get(value) ?? 1) - 1;
        if (remaining === 0) ownWrites.delete(value);
        else ownWrites.set(value, remaining);
      }
    },
    [],
  );

  // Auto-reset "saved" status after 2 seconds
  useEffect(() => {
    if (saveStatus === "saved") {
      const timer = setTimeout(() => setSaveStatus("idle"), 2000);
      return () => clearTimeout(timer);
    }
  }, [saveStatus]);

  // Auto-clear the export error after 4 seconds
  useEffect(() => {
    if (!exportError) return;
    const timer = setTimeout(() => setExportError(false), 4000);
    return () => clearTimeout(timer);
  }, [exportError]);

  useEffect(() => {
    let disposed = false;
    const loadNotes = async () => {
      try {
        setIsLoading(true);
        const snapshot = await rpc.getNotesSnapshot();
        const notesData = await getNotes(t, snapshot.value);
        if (disposed) return;
        acknowledgedValueRef.current = snapshot.value;

        const convertedNotes: Note[] = notesData.titles.map((
          noteTitle,
          index,
        ) => ({
          id: notesData.ids?.[index] ?? crypto.randomUUID(),
          title: noteTitle || "",
          content: notesData.contents[index] || "",
          createdAt: notesData.createdAts?.[index] ?? Date.now(),
          updatedAt: notesData.updatedAts?.[index] ?? Date.now(),
        }));

        notesRef.current = convertedNotes;
        setNotes(convertedNotes);
        hasLoadedRef.current = true;

        if (convertedNotes.length > 0) {
          selectCurrentNote(convertedNotes[0]);
        }
      } catch (error) {
        console.error("Failed to load note data:", error);
      } finally {
        if (!disposed) setIsLoading(false);
      }
    };

    loadNotes();
    return () => {
      disposed = true;
    };
  }, [selectCurrentNote]);

  // ──────────────────────────────────────────────────────────
  // Sync observer: detect external pref changes (Firefox Sync)
  // and apply three-way merge to resolve conflicts.
  // ──────────────────────────────────────────────────────────
  useEffect(() => {
    if (isLoading) return;
    let disposed = false;

    const applyExternalPrefChange = async (
      prefName: string,
      snapshot: NotesPreferenceSnapshot,
    ) => {
      console.info(
        `[Floorp Notes] Detected external change to pref "${prefName}", running three-way merge…`,
      );

      try {
        // Read the externally observed preference snapshot.
        if (!snapshot.value) return; // Empty or null — nothing to merge
        const remoteNotes = notesDataToNotes(
          JSON.parse(snapshot.incomingValue ?? snapshot.value) as NotesData,
        );

        // Read the current local state after the asynchronous base lookup.
        const { state: syncState, context } = await loadSyncState(snapshot);
        if (disposed) return;
        const localNotes = notesRef.current;

        const base = snapshot.origin === "sync" &&
            snapshot.incomingValue === acknowledgedValueRef.current
          ? syncStateFromNotes(
            notesDataToNotes(JSON.parse(snapshot.incomingValue) as NotesData),
          ).lastSyncedSnapshots
          : syncState.lastSyncedSnapshots;
        const result = mergeNotesThreeWay(
          localNotes,
          remoteNotes,
          base,
        );

        // Publish before awaiting writes so later edits build on this merge.
        notesRef.current = result.merged;
        setNotes(result.merged);

        // Preserve selected note if it still exists, otherwise select first.
        const currentSelectedId = selectedNoteRef.current?.id;
        const newSelected = currentSelectedId
          ? result.merged.find((n) => n.id === currentSelectedId)
          : result.merged[0];
        selectCurrentNote(newSelected ?? result.merged[0] ?? null);

        await writeNotesPref(result.merged, context, snapshot.value);
        if (disposed) return;

        if (result.hadConflicts) {
          console.warn(
            `[Floorp Notes] Sync merge completed with ${result.conflictCount} conflict(s). Conflict copies created.`,
          );
        } else {
          console.info("[Floorp Notes] Sync merge completed — no conflicts.");
        }
      } catch (err) {
        console.error("[Floorp Notes] Failed to apply sync merge:", err);
      }
    };

    // Capture each immutable observed snapshot before any async work.
    let syncMergeQueue: Promise<void> = Promise.resolve();
    let lastSnapshotKey = "";
    const onPrefChanged = (
      prefName: string,
      snapshot: NotesPreferenceSnapshot,
    ) => {
      const key = `${snapshot.epoch}:${snapshot.revision}`;
      if (key === lastSnapshotKey) return;
      lastSnapshotKey = key;
      const ownWriteValues = new Set(ownWriteValuesRef.current.keys());

      syncMergeQueue = Promise.all([
        syncMergeQueue,
        Promise.resolve(snapshot),
      ])
        .then(([, captured]) => {
          if (
            disposed ||
            (captured.value !== null && ownWriteValues.has(captured.value))
          ) {
            return;
          }
          return enqueuePersistence(() =>
            applyExternalPrefChange(prefName, captured)
          );
        })
        .catch((err) => {
          console.error("[Floorp Notes] Failed to queue sync merge:", err);
        });
    };

    console.debug("[Floorp Notes] Registering sync pref observer…");
    addPrefObserver(NOTES_PREF_NAME, onPrefChanged);

    return () => {
      disposed = true;
      console.debug("[Floorp Notes] Removing sync pref observer.");
      removePrefObserver(onPrefChanged);
    };
  }, [isLoading, enqueuePersistence, selectCurrentNote, writeNotesPref]);

  const saveNotesToStorage = useCallback(async () => {
    try {
      await enqueuePersistence(async () => {
        if (!isMountedRef.current) return;
        setSaveStatus("saving");
        await writeNotesPref(notesRef.current);
        if (isMountedRef.current) setSaveStatus("saved");
      });
    } catch (error) {
      console.error("Failed to save note data:", error);
      if (isMountedRef.current) setSaveStatus("error");
    }
  }, [enqueuePersistence, writeNotesPref]);

  const debouncedSave = useCallback(() => {
    if (saveTimeoutRef.current) {
      clearTimeout(saveTimeoutRef.current);
    }
    saveTimeoutRef.current = setTimeout(() => {
      saveNotesToStorage();
    }, 500);
  }, [saveNotesToStorage]);

  useEffect(() => {
    if (!isLoading && hasLoadedRef.current) {
      debouncedSave();
    }
  }, [notes, isLoading, debouncedSave]);

  // Flush pending save on unmount and beforeunload
  useEffect(() => {
    const flushSave = () => {
      if (saveTimeoutRef.current) {
        clearTimeout(saveTimeoutRef.current);
      }
      if (hasLoadedRef.current) {
        writeNotesPref(notesRef.current).catch(() => {});
      }
    };

    globalThis.addEventListener("beforeunload", flushSave);
    return () => {
      globalThis.removeEventListener("beforeunload", flushSave);
      flushSave();
    };
  }, [writeNotesPref]);

  const filteredNotes = useMemo(() => {
    if (!searchQuery.trim()) return notes;
    const query = searchQuery.toLowerCase();
    return notes.filter((note) => {
      if (note.title.toLowerCase().includes(query)) return true;
      const text = extractPlainText(note.content).toLowerCase();
      return text.includes(query);
    });
  }, [notes, searchQuery]);

  const createNewNote = useCallback(() => {
    const now = Date.now();
    const newNote: Note = {
      id: crypto.randomUUID(),
      title: t("notes.new"),
      content: "",
      createdAt: now,
      updatedAt: now,
    };
    updateNotes((prev) => [newNote, ...prev]);
    selectCurrentNote(newNote);
    requestAnimationFrame(() => {
      titleInputRef.current?.focus();
      titleInputRef.current?.select();
    });
  }, [t, updateNotes, selectCurrentNote]);

  const updateCurrentNote = useCallback((content: string) => {
    const current = selectedNoteRef.current;
    if (!current) return;
    const updatedNote = { ...current, content, updatedAt: Date.now() };
    selectCurrentNote(updatedNote);
    updateNotes((prevNotes) =>
      prevNotes.map((note) => note.id === current.id ? updatedNote : note)
    );
  }, [selectCurrentNote, updateNotes]);

  const updateCurrentTitle = useCallback((nextTitle: string) => {
    const current = selectedNoteRef.current;
    if (!current || current.title === nextTitle) return;
    const updatedNote = { ...current, title: nextTitle, updatedAt: Date.now() };
    selectCurrentNote(updatedNote);
    updateNotes((currentNotes) =>
      currentNotes.map((note) => note.id === current.id ? updatedNote : note)
    );
  }, [selectCurrentNote, updateNotes]);

  const requestDeleteNote = useCallback((id: string) => {
    setDeleteTarget(id);
  }, []);

  const confirmDeleteNote = useCallback(() => {
    if (!deleteTarget) return;

    updateNotes((prevNotes) => {
      const currentIndex = prevNotes.findIndex((n) => n.id === deleteTarget);
      const remaining = prevNotes.filter((note) => note.id !== deleteTarget);

      if (selectedNoteRef.current?.id === deleteTarget) {
        const nextIndex = Math.min(currentIndex, remaining.length - 1);
        const nextNote = remaining[nextIndex] ?? null;
        selectCurrentNote(nextNote);

        if (!nextNote) {
          requestAnimationFrame(() => createButtonRef.current?.focus());
        }
      }

      return remaining;
    });

    setDeleteTarget(null);
  }, [deleteTarget, updateNotes, selectCurrentNote]);

  const selectNote = useCallback((note: Note) => {
    selectCurrentNote(note);
  }, [selectCurrentNote]);

  const handleExport = useCallback(async () => {
    if (!selectedNote) {
      return;
    }
    const note = { title, content: selectedNote.content };
    setExportError(false);
    try {
      // The return value says whether the user cancelled. There is deliberately
      // no success indicator: in production the OS dialog is the confirmation,
      // and in dev the browser's own download UI is.
      await saveTextFile({
        text: noteToMarkdown(note),
        suggestedName: toExportFilename(note.title, t("notes.emptyTitle")),
        defaultExtension: "md",
        filterLabel: "Markdown",
        filterPattern: "*.md",
        dialogTitle: t("notes.export"),
      });
    } catch (error) {
      console.error("[Floorp Notes] Failed to export note:", error);
      setExportError(true);
    }
  }, [selectedNote, title, t]);

  const handleEditorChange = useCallback((json: JSONContent) => {
    updateCurrentNote(JSON.stringify(json));
  }, [updateCurrentNote]);

  return (
    <div className="flex flex-col h-screen bg-base-100 text-base-content">
      {/* Prevent any element outside the editor from stealing focus on mousedown */}
      <header
        className="bg-base-200 px-2 py-1.5 flex justify-between items-center"
        onMouseDown={(e) => e.preventDefault()}
      >
        <div className="flex items-center gap-1.5">
          <h1 className="text-sm font-bold text-base-content">
            {t("title.default")}
          </h1>
          <SaveStatus status={saveStatus} />
          {exportError && (
            <div
              className="flex items-center gap-1 text-xs text-error"
              role="status"
              aria-live="polite"
            >
              <AlertCircle className="h-3 w-3" />
              <span>{t("error.exportFailed")}</span>
            </div>
          )}
        </div>
        <div className="flex gap-1">
          <button
            type="button"
            className="btn btn-xs btn-ghost"
            data-testid="notes-export"
            title={t("notes.export")}
            aria-label={t("notes.export")}
            disabled={!selectedNote}
            onClick={handleExport}
          >
            <Download size={14} />
          </button>
          <button
            type="button"
            className={`btn btn-xs ${
              isReorderMode ? "btn-secondary" : "btn-ghost"
            }`}
            onClick={() => setIsReorderMode(!isReorderMode)}
          >
            {isReorderMode ? t("notes.done") : t("notes.reorder")}
          </button>
          <button
            ref={createButtonRef}
            type="button"
            className="btn btn-xs btn-primary"
            data-testid="notes-add"
            onClick={createNewNote}
          >
            {t("notes.new")}
          </button>
        </div>
      </header>

      <div className="flex flex-col flex-1 overflow-hidden">
        {isLoading
          ? (
            <div
              className="flex flex-col items-center justify-center h-full"
              role="status"
              aria-live="polite"
            >
              <span className="loading loading-spinner loading-lg text-primary">
              </span>
              <p className="mt-4">{t("notes.loading")}</p>
            </div>
          )
          : (
            <>
              <NoteSearch value={searchQuery} onChange={setSearchQuery} />
              <NoteList
                notes={filteredNotes}
                selectedNote={selectedNote}
                onSelectNote={selectNote}
                onDeleteNote={requestDeleteNote}
                onReorderNotes={(reorderedSubset) => {
                  updateNotes((prevNotes) => {
                    // Build an id→index map from the reordered subset
                    const orderMap = new Map(
                      reorderedSubset.map((n, i) => [n.id, i]),
                    );
                    // Separate notes into those in the subset and those not
                    const inSubset = prevNotes.filter((n) =>
                      orderMap.has(n.id)
                    );
                    // Sort the subset according to the new order
                    inSubset.sort((a, b) =>
                      (orderMap.get(a.id) ?? 0) - (orderMap.get(b.id) ?? 0)
                    );
                    // Rebuild: place reordered items back at their original positions
                    const result: Note[] = [];
                    let subIdx = 0;
                    for (const n of prevNotes) {
                      if (orderMap.has(n.id)) {
                        result.push(inSubset[subIdx++]);
                      } else {
                        result.push(n);
                      }
                    }
                    return result;
                  });
                }}
                isReorderMode={isReorderMode}
                emptyMessage={searchQuery
                  ? t("notes.noSearchResults")
                  : undefined}
              />

              <div className="flex-1 flex flex-col p-2 overflow-hidden border-t border-base-content/20">
                {selectedNote
                  ? (
                    <div className="flex flex-1 gap-4 overflow-y-auto">
                      <div className="flex-1 flex flex-col">
                        <input
                          ref={titleInputRef}
                          type="text"
                          className="input input-sm input-bordered w-full mb-1.5 focus:outline-none focus:border-primary/30"
                          data-testid="notes-title"
                          placeholder={t("notes.titlePlaceholder")}
                          aria-label={t("notes.titlePlaceholder")}
                          value={title}
                          onChange={(e) => updateCurrentTitle(e.target.value)}
                        />
                        <div className="flex-1 flex flex-col rounded-lg overflow-hidden">
                          <RichTextEditor
                            key={selectedNote.id}
                            onChange={handleEditorChange}
                            initialContent={selectedNote.content}
                          />
                        </div>
                      </div>
                    </div>
                  )
                  : (
                    <div className="flex flex-col items-center justify-center h-full text-base-content/70">
                      <p className="mb-4">{t("notes.noNotesSelected")}</p>
                      <button
                        ref={createButtonRef}
                        type="button"
                        className="btn btn-sm btn-primary"
                        onClick={createNewNote}
                      >
                        {t("notes.createNew")}
                      </button>
                    </div>
                  )}
              </div>
            </>
          )}
      </div>

      <ConfirmModal
        isOpen={deleteTarget !== null}
        onClose={() => setDeleteTarget(null)}
        onConfirm={confirmDeleteNote}
        title={t("notes.deleteConfirmTitle")}
        confirmText={t("notes.deleteConfirm")}
        cancelText={t("notes.deleteCancel")}
        confirmVariant="btn-error"
      >
        <p>{t("notes.deleteConfirmMessage")}</p>
      </ConfirmModal>
    </div>
  );
}

export default App;
