import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { useTranslation } from "react-i18next";
import { CloudOff, Trash2 } from "lucide-react";
import { ClipItem } from "@/components/clips/ClipItem.tsx";
import { ClipComposer } from "@/components/clips/ClipComposer.tsx";
import { ConfirmModal } from "@/components/common/ConfirmModal.tsx";
import {
  deleteClips,
  getAllClips,
  putClip,
  replaceAll,
} from "@/lib/db.ts";
import {
  clipFromText,
  clipsFromFiles,
  filePathsFromDataTransfer,
} from "@/lib/intake.ts";
import {
  addPrefObserver,
  addPrefObservers,
  clips as clipsRpc,
  rpc,
} from "@/lib/rpc/rpc.ts";
import {
  CLEAR_ON_EXIT_PREF,
  type ClipsSettings,
  DATA_PREF,
  DEFAULT_MAX_ITEMS,
  FILE_ACTION_PREF,
  getPageState,
  getSettings,
  MAX_ITEMS_PREF,
  MODE_PREF,
  PENDING_PREF,
  savePageState,
} from "@/lib/settings.ts";
import { isWritingSync, pullAndMerge, push } from "@/lib/syncStore.ts";
import { watchClipboard } from "@/lib/clipboardWatch.ts";
import type { Clip } from "@/types/clip.ts";

/** What the confirmation dialog is currently asking about. */
type Pending =
  | { kind: "delete"; id: string }
  | { kind: "cleanup" };

const INITIAL_SETTINGS: ClipsSettings = {
  mode: "local",
  maxItems: DEFAULT_MAX_ITEMS,
  clearOnExit: false,
  fileAction: "reveal",
};

function App() {
  const { t } = useTranslation();
  const [clips, setClips] = useState<Clip[]>([]);
  const [settings, setSettings] = useState<ClipsSettings>(INITIAL_SETTINGS);
  const [isLoading, setIsLoading] = useState(true);
  const [pinnedOnly, setPinnedOnly] = useState(false);
  const [pending, setPending] = useState<Pending | null>(null);
  const [zoomed, setZoomed] = useState<Clip | null>(null);
  const [isDragOver, setIsDragOver] = useState(false);
  const [error, setError] = useState<string | null>(null);
  /** How many clips did not fit in the synced payload. */
  const [notSynced, setNotSynced] = useState(0);
  const bottomRef = useRef<HTMLDivElement>(null);
  const zoomRef = useRef<HTMLDialogElement>(null);
  /** Nested dragenter/dragleave pairs — count them instead of guessing. */
  const dragDepthRef = useRef(0);
  /** The current clips, for callbacks that must not close over stale state. */
  const clipsRef = useRef<Clip[]>([]);
  const settingsRef = useRef<ClipsSettings>(INITIAL_SETTINGS);

  useEffect(() => {
    clipsRef.current = clips;
  }, [clips]);
  useEffect(() => {
    settingsRef.current = settings;
  }, [settings]);

  /** Store the clips and, in sync mode, publish them. */
  const commit = useCallback(async (next: Clip[]) => {
    setClips(next);
    clipsRef.current = next;
    if (settingsRef.current.mode === "sync") {
      setNotSynced(await push(next));
    }
  }, []);

  // ──────────────────────────────────────────────────────────
  // First load: settings, clips, and the things that may have happened
  // while the page was closed (a restart, or a mode switch).
  // ──────────────────────────────────────────────────────────
  useEffect(() => {
    (async () => {
      try {
        const [loaded, stored, pageState, sessionStart] = await Promise.all([
          getSettings(),
          getAllClips(),
          getPageState(),
          clipsRpc.getSessionStartTime().catch(() => 0),
        ]);
        setSettings(loaded);
        settingsRef.current = loaded;

        const restarted = loaded.clearOnExit && pageState.sessionStart !== 0 &&
          pageState.sessionStart !== sessionStart;
        const modeChanged = pageState.mode !== null &&
          pageState.mode !== loaded.mode;

        let current = stored;
        if (restarted || modeChanged) {
          const doomed = current.filter((c) => !c.pinned).map((c) => c.id);
          await deleteClips(doomed);
          current = current.filter((c) => c.pinned);
        }

        if (loaded.mode === "sync") {
          const merged = await pullAndMerge(current);
          if (merged) {
            await replaceAll(merged);
            current = merged;
          }
        }

        setClips(current);
        clipsRef.current = current;
        await savePageState({ sessionStart, mode: loaded.mode });
        if (loaded.mode === "sync") setNotSynced(await push(current));
      } catch (e) {
        console.error("[Floorp Clips] Failed to load clips:", e);
      } finally {
        setIsLoading(false);
      }
    })();
  }, []);

  const visible = useMemo(
    () => (pinnedOnly ? clips.filter((c) => c.pinned) : clips),
    [clips, pinnedOnly],
  );

  useEffect(() => {
    bottomRef.current?.scrollIntoView({ block: "end" });
  }, [visible.length]);

  useEffect(() => {
    const dialog = zoomRef.current;
    if (!dialog) return;
    if (zoomed && !dialog.open) dialog.showModal();
    if (!zoomed && dialog.open) dialog.close();
  }, [zoomed]);

  /**
   * Store new clips, then drop the oldest unpinned ones over the limit.
   * Pinned clips are outside the count, and are never dropped this way.
   */
  const addClips = useCallback(async (incoming: Clip[]) => {
    if (incoming.length === 0) return;
    try {
      for (const clip of incoming) await putClip(clip);

      let next = [...clipsRef.current, ...incoming];
      const unpinned = next.filter((c) => !c.pinned);
      const excess = unpinned.length - settingsRef.current.maxItems;
      if (excess > 0) {
        const doomed = new Set(unpinned.slice(0, excess).map((c) => c.id));
        await deleteClips([...doomed]);
        next = next.filter((c) => !doomed.has(c.id));
      }
      await commit(next);
    } catch (e) {
      console.error("[Floorp Clips] Failed to add clips:", e);
    }
  }, [commit]);

  const addText = useCallback((text: string) => {
    void addClips([clipFromText(text)]);
  }, [addClips]);

  const addFiles = useCallback(
    async (files: File[], paths: (string | undefined)[]) => {
      const { clips: made, failed } = await clipsFromFiles(files, paths);
      setError(failed > 0 ? t("clips.imageFailed") : null);
      await addClips(made);
    },
    [addClips, t],
  );

  const togglePin = useCallback(async (clip: Clip) => {
    const updated = { ...clip, pinned: !clip.pinned, updatedAt: Date.now() };
    try {
      await putClip(updated);
      await commit(
        clipsRef.current.map((c) => (c.id === clip.id ? updated : c)),
      );
    } catch (e) {
      console.error("[Floorp Clips] Failed to pin/unpin:", e);
    }
  }, [commit]);

  const confirmPending = useCallback(async () => {
    if (!pending) return;
    const doomed = pending.kind === "delete"
      ? [pending.id]
      : clipsRef.current.filter((c) => !c.pinned).map((c) => c.id);
    setPending(null);
    try {
      await deleteClips(doomed);
      const gone = new Set(doomed);
      await commit(clipsRef.current.filter((c) => !gone.has(c.id)));
    } catch (e) {
      console.error("[Floorp Clips] Failed to delete clips:", e);
    }
  }, [pending, commit]);

  // ──────────────────────────────────────────────────────────
  // The mode changed in the settings page while we were open. Switching
  // modes forgets the unpinned clips — the settings page warns first.
  // ──────────────────────────────────────────────────────────
  useEffect(() => {
    if (isLoading) return;
    const onChanged = async () => {
      const loaded = await getSettings();
      const previous = settingsRef.current.mode;
      setSettings(loaded);
      settingsRef.current = loaded;
      // Only a mode switch forgets clips; the other settings just take effect.
      if (previous === loaded.mode) return;

      const doomed = clipsRef.current.filter((c) => !c.pinned).map((c) => c.id);
      await deleteClips(doomed);
      const kept = clipsRef.current.filter((c) => c.pinned);
      setClips(kept);
      clipsRef.current = kept;
      const sessionStart = await clipsRpc.getSessionStartTime().catch(() => 0);
      await savePageState({ sessionStart, mode: loaded.mode });
      if (loaded.mode === "sync") setNotSynced(await push(kept));
    };
    return addPrefObservers(
      [MODE_PREF, MAX_ITEMS_PREF, CLEAR_ON_EXIT_PREF, FILE_ACTION_PREF],
      () => void onChanged(),
    );
  }, [isLoading]);

  // ── Sync mode: merge what other devices sent ──────────────
  useEffect(() => {
    if (isLoading || settings.mode !== "sync") return;
    const onChanged = async () => {
      if (isWritingSync()) return;
      const merged = await pullAndMerge(clipsRef.current);
      if (!merged) return;
      await replaceAll(merged);
      setClips(merged);
      clipsRef.current = merged;
    };
    return addPrefObserver(DATA_PREF, () => void onChanged());
  }, [isLoading, settings.mode]);

  // ── Clipboard history mode ────────────────────────────────
  useEffect(() => {
    if (isLoading || settings.mode !== "clipboard") return;
    return watchClipboard((text) => addText(text));
  }, [isLoading, settings.mode, addText]);

  // ── A clip handed over by an action while we were closed ──
  useEffect(() => {
    if (isLoading) return;
    const consume = async () => {
      const text = await rpc.getStringPref(PENDING_PREF).catch(() => null);
      if (!text) return;
      await rpc.setStringPref(PENDING_PREF, "").catch(() => {});
      addText(text);
    };
    void consume();
    return addPrefObserver(PENDING_PREF, () => void consume());
  }, [isLoading, addText]);

  return (
    <div
      className="relative flex h-screen flex-col bg-base-100 text-base-content"
      onDragEnter={(e) => {
        e.preventDefault();
        dragDepthRef.current++;
        setIsDragOver(true);
      }}
      onDragOver={(e) => e.preventDefault()}
      onDragLeave={() => {
        dragDepthRef.current = Math.max(0, dragDepthRef.current - 1);
        if (dragDepthRef.current === 0) setIsDragOver(false);
      }}
      onDrop={(e) => {
        e.preventDefault();
        dragDepthRef.current = 0;
        setIsDragOver(false);
        const files = Array.from(e.dataTransfer?.files ?? []);
        if (files.length > 0) {
          void addFiles(files, filePathsFromDataTransfer(e.dataTransfer));
          return;
        }
        const text = e.dataTransfer?.getData("text/plain") ?? "";
        if (text.trim()) addText(text.trim());
      }}
    >
      <header className="flex items-center gap-1 border-b border-base-300 px-2 py-1.5">
        <h1 className="flex-1 truncate text-sm font-semibold">
          {t("title.default")}
        </h1>
        <button
          type="button"
          data-testid="clips-pinned-filter"
          className={`btn btn-xs ${pinnedOnly ? "btn-primary" : "btn-ghost"}`}
          aria-pressed={pinnedOnly}
          onClick={() => setPinnedOnly((v) => !v)}
        >
          {pinnedOnly ? t("clips.showAll") : t("clips.pinnedOnly")}
        </button>
        <button
          type="button"
          className="btn btn-xs btn-ghost btn-square"
          aria-label={t("clips.cleanup")}
          title={t("clips.cleanup")}
          onClick={() => setPending({ kind: "cleanup" })}
        >
          <Trash2 className="h-3.5 w-3.5" />
        </button>
      </header>

      {settings.mode === "sync" && notSynced > 0 && (
        <p className="flex items-center gap-1 border-b border-base-300 px-2 py-1 text-xs text-base-content/60">
          <CloudOff className="h-3 w-3 shrink-0" />
          {t("clips.notSynced", { count: notSynced })}
        </p>
      )}

      <main className="flex-1 overflow-y-auto px-2 py-2">
        {isLoading
          ? (
            <p className="p-4 text-center text-sm opacity-60">
              {t("clips.loading")}
            </p>
          )
          : visible.length === 0
          ? (
            <p className="p-4 text-center text-sm opacity-60">
              {pinnedOnly ? t("clips.noPinned") : t("clips.empty")}
            </p>
          )
          : (
            <ul className="flex flex-col gap-2">
              {visible.map((clip) => (
                <ClipItem
                  key={clip.id}
                  clip={clip}
                  fileAction={settings.fileAction}
                  onTogglePin={(c) => void togglePin(c)}
                  onDelete={(id) => setPending({ kind: "delete", id })}
                  onZoom={setZoomed}
                  onError={setError}
                />
              ))}
            </ul>
          )}
        <div ref={bottomRef} />
      </main>

      {error && (
        <p className="px-2 pb-1 text-xs text-error" role="alert">{error}</p>
      )}

      <ClipComposer
        onAddText={addText}
        onAddFiles={(files, paths) => void addFiles(files, paths)}
      />

      {isDragOver && (
        <div className="pointer-events-none absolute inset-0 flex items-center justify-center border-2 border-dashed border-primary bg-base-100/80 text-sm font-medium">
          {t("clips.dropHere")}
        </div>
      )}

      <ConfirmModal
        isOpen={pending !== null}
        onClose={() => setPending(null)}
        onConfirm={() => void confirmPending()}
        title={pending?.kind === "cleanup"
          ? t("clips.cleanupConfirmTitle")
          : t("clips.deleteConfirmTitle")}
        confirmText={t("clips.delete")}
        confirmVariant="btn-error"
      >
        {pending?.kind === "cleanup"
          ? t("clips.cleanupConfirmMessage")
          : t("clips.deleteConfirmMessage")}
      </ConfirmModal>

      <dialog ref={zoomRef} className="modal" onClose={() => setZoomed(null)}>
        <div className="modal-box max-w-full p-2">
          {zoomed?.preview && (
            <img
              src={zoomed.preview}
              alt={zoomed.fileName ?? ""}
              className="max-h-[80vh] w-full object-contain"
            />
          )}
        </div>
        <form method="dialog" className="modal-backdrop">
          <button type="submit" aria-label={t("common.close")} />
        </form>
      </dialog>
    </div>
  );
}

export default App;
