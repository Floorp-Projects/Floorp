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
  saveSyncState,
  syncStateFromNotes,
} from "./lib/dataManager.ts";
import { addPrefObserver, removePrefObserver, rpc } from "./lib/rpc/rpc.ts";
import { useTranslation } from "react-i18next";
import { ConfirmModal } from "./components/common/ConfirmModal.tsx";
import { SaveStatus } from "./components/common/SaveStatus.tsx";
import { NoteSearch } from "./components/notes/NoteSearch.tsx";
import type { Note } from "./types/note.ts";
import { extractPlainText } from "./lib/extractText.ts";

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
  const saveTimeoutRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const titleSyncTimeoutRef = useRef<ReturnType<typeof setTimeout> | null>(
    null,
  );
  const notesRef = useRef<Note[]>([]);
  const isSavingRef = useRef(false);
  const pendingSaveRef = useRef<Note[] | null>(null);
  const titleInputRef = useRef<HTMLInputElement>(null);
  const createButtonRef = useRef<HTMLButtonElement>(null);
  /** Flag: when true, the current pref write originates from our sync merge — skip observer reaction. */
  const isWritingFromSyncRef = useRef(false);
  /** Flag: when true, the current pref write is a user-initiated save — observer should skip. */
  const isWritingFromLocalRef = useRef(false);

  // Keep notesRef in sync
  useEffect(() => {
    notesRef.current = notes;
  }, [notes]);

  // Auto-reset "saved" status after 2 seconds
  useEffect(() => {
    if (saveStatus === "saved") {
      const timer = setTimeout(() => setSaveStatus("idle"), 2000);
      return () => clearTimeout(timer);
    }
  }, [saveStatus]);

  useEffect(() => {
    const loadNotes = async () => {
      try {
        setIsLoading(true);
        const notesData = await getNotes(t);

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

        setNotes(convertedNotes);

        if (convertedNotes.length > 0) {
          setSelectedNote(convertedNotes[0]);
          setTitle(convertedNotes[0].title);
        }
      } catch (error) {
        console.error("Failed to load note data:", error);
      } finally {
        setIsLoading(false);
      }
    };

    loadNotes();
  }, []);

  // ──────────────────────────────────────────────────────────
  // Sync observer: detect external pref changes (Firefox Sync)
  // and apply three-way merge to resolve conflicts.
  // ──────────────────────────────────────────────────────────
  useEffect(() => {
    if (isLoading) return;

    const onPrefChanged = async (prefName: string) => {
      // Skip if this change was triggered by our own save or merge
      if (isWritingFromLocalRef.current || isWritingFromSyncRef.current) {
        return;
      }

      console.info(
        `[Floorp Notes] Detected external change to pref "${prefName}", running three-way merge…`,
      );

      try {
        // 1. Read remote notes from the pref (what sync just wrote)
        const remoteStr = await rpc.getStringPref(NOTES_PREF_NAME);
        if (!remoteStr) return; // Empty or null — nothing to merge
        const remoteNotes = notesDataToNotes(
          JSON.parse(remoteStr) as NotesData,
        );

        // 2. Local notes = current React state
        const localNotes = notesRef.current;

        // 3. Load base (last-synced snapshots)
        const syncState = await loadSyncState();

        // 4. Apply three-way merge
        const result = mergeNotesThreeWay(
          localNotes,
          remoteNotes,
          syncState.lastSyncedSnapshots,
        );

        // 5. Save merged result back to pref
        isWritingFromSyncRef.current = true;
        try {
          await saveNotes(notesToNotesData(result.merged));
        } finally {
          isWritingFromSyncRef.current = false;
        }

        // 6. Save updated sync state
        await saveSyncState(syncStateFromNotes(result.merged));

        // 7. Update React state with merged notes
        setNotes(result.merged);

        // 8. Preserve selected note if it still exists, otherwise select first
        const currentSelectedId = selectedNoteRef.current?.id;
        const newSelected = currentSelectedId
          ? result.merged.find((n) => n.id === currentSelectedId)
          : result.merged[0];
        setSelectedNote(newSelected ?? result.merged[0] ?? null);
        setTitle(newSelected?.title ?? result.merged[0]?.title ?? "");

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

    console.debug("[Floorp Notes] Registering sync pref observer…");
    addPrefObserver(NOTES_PREF_NAME, onPrefChanged);

    return () => {
      console.debug("[Floorp Notes] Removing sync pref observer.");
      removePrefObserver(onPrefChanged);
    };
  }, [isLoading]);

  const buildNotesData = useCallback((notesToSave: Note[]): NotesData => ({
    ids: notesToSave.map((note) => note.id),
    titles: notesToSave.map((note) => note.title),
    contents: notesToSave.map((note) => note.content),
    createdAts: notesToSave.map((note) => note.createdAt),
    updatedAts: notesToSave.map((note) => note.updatedAt),
  }), []);

  const saveNotesToStorage = useCallback(async (notesToSave: Note[]) => {
    if (isSavingRef.current) {
      // Queue the latest save request; only the most recent matters
      pendingSaveRef.current = notesToSave;
      return;
    }
    isSavingRef.current = true;
    isWritingFromLocalRef.current = true;
    try {
      setSaveStatus("saving");
      await saveNotes(buildNotesData(notesToSave));

      // Update sync base after local save.
      // NOTE: We advance lastSyncedSnapshots on local save so that the next
      // incoming sync can diff against what we just wrote. This means a true
      // conflict (local edit + remote edit to the same note between syncs)
      // will be detected and a conflict copy created, which is the desired
      // behaviour. The trade-off is that if the remote side never receives
      // our change (e.g. sync is disabled), a subsequent remote change to the
      // same note would appear as a unilateral remote edit rather than a
      // conflict. This is acceptable because Firefox Sync is expected to
      // deliver the local change before the remote one arrives.
      try {
        const syncState = await loadSyncState();
        const updatedState = syncStateFromNotes(notesToSave);
        Object.assign(
          syncState.lastSyncedSnapshots,
          updatedState.lastSyncedSnapshots,
        );
        syncState.lastSyncTime = updatedState.lastSyncTime;
        await saveSyncState(syncState);
      } catch (syncErr) {
        // Non-critical: sync state update failure shouldn't block the UI
        console.warn(
          "[Floorp Notes] Failed to update sync state after save:",
          syncErr,
        );
      }

      setSaveStatus("saved");
    } catch (error) {
      console.error("Failed to save note data:", error);
      setSaveStatus("error");
    } finally {
      isSavingRef.current = false;
      isWritingFromLocalRef.current = false;
      // Flush any queued save
      const pending = pendingSaveRef.current;
      if (pending) {
        pendingSaveRef.current = null;
        saveNotesToStorage(pending);
      }
    }
  }, [buildNotesData]);

  const debouncedSave = useCallback((notesToSave: Note[]) => {
    if (saveTimeoutRef.current) {
      clearTimeout(saveTimeoutRef.current);
    }
    saveTimeoutRef.current = setTimeout(() => {
      saveNotesToStorage(notesToSave);
    }, 500);
  }, [saveNotesToStorage]);

  useEffect(() => {
    if (!isLoading) {
      debouncedSave(notes);
    }
  }, [notes, isLoading, debouncedSave]);

  // Flush pending save on unmount and beforeunload
  useEffect(() => {
    const flushSave = () => {
      if (saveTimeoutRef.current) {
        clearTimeout(saveTimeoutRef.current);
      }
      if (titleSyncTimeoutRef.current) {
        clearTimeout(titleSyncTimeoutRef.current);
      }
      if (notesRef.current.length > 0) {
        saveNotes(buildNotesData(notesRef.current)).catch(() => {});
      }
    };

    globalThis.addEventListener("beforeunload", flushSave);
    return () => {
      globalThis.removeEventListener("beforeunload", flushSave);
      flushSave();
    };
  }, [buildNotesData]);

  // Debounced title sync to notes state
  useEffect(() => {
    if (!selectedNote || title === selectedNote.title) return;

    if (titleSyncTimeoutRef.current) {
      clearTimeout(titleSyncTimeoutRef.current);
    }
    titleSyncTimeoutRef.current = setTimeout(() => {
      setNotes((prevNotes) =>
        prevNotes.map((note) =>
          note.id === selectedNote.id
            ? { ...note, title, updatedAt: Date.now() }
            : note
        )
      );
    }, 300);

    return () => {
      if (titleSyncTimeoutRef.current) {
        clearTimeout(titleSyncTimeoutRef.current);
      }
    };
  }, [title, selectedNote]);

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
    setNotes((prev) => [newNote, ...prev]);
    setSelectedNote(newNote);
    setTitle(newNote.title);
    requestAnimationFrame(() => {
      titleInputRef.current?.focus();
      titleInputRef.current?.select();
    });
  }, [t]);

  const selectedNoteRef = useRef<Note | null>(null);
  useEffect(() => {
    selectedNoteRef.current = selectedNote;
  }, [selectedNote]);

  const updateCurrentNote = useCallback((content: string) => {
    const current = selectedNoteRef.current;
    if (!current) return;
    const updatedNote = { ...current, content, updatedAt: Date.now() };
    setSelectedNote(updatedNote);
    setNotes((prevNotes) =>
      prevNotes.map((note) => note.id === current.id ? updatedNote : note)
    );
  }, []);

  const requestDeleteNote = useCallback((id: string) => {
    setDeleteTarget(id);
  }, []);

  const confirmDeleteNote = useCallback(() => {
    if (!deleteTarget) return;

    setNotes((prevNotes) => {
      const currentIndex = prevNotes.findIndex((n) => n.id === deleteTarget);
      const remaining = prevNotes.filter((note) => note.id !== deleteTarget);

      setSelectedNote((prevSelected) => {
        if (prevSelected?.id !== deleteTarget) return prevSelected;

        const nextIndex = Math.min(currentIndex, remaining.length - 1);
        const nextNote = remaining[nextIndex] ?? null;
        setTitle(nextNote?.title ?? "");

        if (!nextNote) {
          requestAnimationFrame(() => createButtonRef.current?.focus());
        }

        return nextNote;
      });

      return remaining;
    });

    setDeleteTarget(null);
  }, [deleteTarget]);

  const selectNote = useCallback((note: Note) => {
    // Flush pending title change before switching
    if (titleSyncTimeoutRef.current) {
      clearTimeout(titleSyncTimeoutRef.current);
      titleSyncTimeoutRef.current = null;
    }
    setSelectedNote((prev) => {
      if (prev && title !== prev.title) {
        setNotes((prevNotes) =>
          prevNotes.map((n) =>
            n.id === prev.id ? { ...n, title, updatedAt: Date.now() } : n
          )
        );
      }
      return note;
    });
    setTitle(note.title);
  }, [title]);

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
        </div>
        <div className="flex gap-1">
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
                  setNotes((prevNotes) => {
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
                          placeholder={t("notes.titlePlaceholder")}
                          aria-label={t("notes.titlePlaceholder")}
                          value={title}
                          onChange={(e) => setTitle(e.target.value)}
                          onBlur={() => {
                            if (selectedNote && title !== selectedNote.title) {
                              if (titleSyncTimeoutRef.current) {
                                clearTimeout(titleSyncTimeoutRef.current);
                                titleSyncTimeoutRef.current = null;
                              }
                              const updatedNote = {
                                ...selectedNote,
                                title,
                                updatedAt: Date.now(),
                              };
                              setNotes((prevNotes) =>
                                prevNotes.map((note) =>
                                  note.id === selectedNote.id
                                    ? updatedNote
                                    : note
                                )
                              );
                              setSelectedNote(updatedNote);
                            }
                          }}
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
