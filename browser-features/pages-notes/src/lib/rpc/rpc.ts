import type { NotesPrefFunctions, NotesStorageModule } from "./types.ts";
import type { NotesPreferenceSnapshot } from "../../../../modules/common/notes-storage-types.ts";
import { createBirpc } from "birpc";

// deno-lint-ignore no-explicit-any
declare const Services: any;
declare const ChromeUtils: {
  importESModule(uri: string): unknown;
};
declare global {
  interface Window {
    NRSettingsSend: (data: string) => void;
    NRSettingsRegisterReceiveCallback: (
      callback: (data: string) => void,
    ) => void;
  }
}

const isLocalhost5188 = import.meta.url?.includes("localhost:5188");
const notesStorage = isLocalhost5188 ? null : (ChromeUtils.importESModule(
  "resource://noraneko/modules/notes/FloorpNotesStorage.sys.mjs",
) as NotesStorageModule).FloorpNotesStorage;

// ──────────────────────────────────────────────────────────
// Pref Observer Types
// ──────────────────────────────────────────────────────────

/** Callback invoked when an observed preference changes. */
export type PrefChangeCallback = (
  prefName: string,
  snapshot: NotesPreferenceSnapshot,
) => void;

/**
 * Internal bookkeeping object returned by direct-mode observer registration.
 *
 * Observer removal is callback-based; the handle stores the underlying observer
 * instance associated with that callback so the implementation can unregister it.
 */
export interface PrefObserverHandle {
  readonly prefName: string;
  readonly callback: PrefChangeCallback;
  /** Internal: the raw observer object (nsIObserver) for removal. */
  _observer: {
    observe: (subject: unknown, topic: string, data: string) => void;
  };
  _contextObserver?: {
    observe: (subject: unknown, topic: string, data: string) => void;
  };
}

// ──────────────────────────────────────────────────────────
// Pref change observer registry (for production/direct mode)
// ──────────────────────────────────────────────────────────

const observerRegistry = new Map<PrefChangeCallback, PrefObserverHandle>();

function addDirectPrefObserver(
  prefName: string,
  callback: PrefChangeCallback,
): PrefObserverHandle {
  const observer = {
    observe(_subject: unknown, topic: string, data: string) {
      if (topic === "nsPref:changed" && data === prefName) {
        callback(prefName, notesStorage!.getSnapshot());
      }
    },
  };
  Services.prefs.addObserver(prefName, observer);
  const contextObserver = {
    observe() {
      callback(prefName, notesStorage!.getSnapshot());
    },
  };
  Services.obs.addObserver(contextObserver, "floorp-notes:context-changed");
  const handle: PrefObserverHandle = {
    prefName,
    callback,
    _observer: observer,
    _contextObserver: contextObserver,
  };
  observerRegistry.set(callback, handle);
  return handle;
}

function removeDirectPrefObserver(callback: PrefChangeCallback): void {
  const handle = observerRegistry.get(callback);
  if (handle) {
    Services.prefs.removeObserver(handle.prefName, handle._observer);
    if (handle._contextObserver) {
      Services.obs.removeObserver(
        handle._contextObserver,
        "floorp-notes:context-changed",
      );
    }
    observerRegistry.delete(callback);
  }
}

// ──────────────────────────────────────────────────────────
// RPC functions (existing)
// ──────────────────────────────────────────────────────────

const directServicesFunctions: NotesPrefFunctions = {
  getNotesSnapshot: () => Promise.resolve(notesStorage!.getSnapshot()),
  readNotesSyncState: (snapshot) => notesStorage!.readSyncState(snapshot),
  writeNotesPref: (value, context, baseValue) => {
    return Promise.resolve(notesStorage!.writeLocal(value, context, baseValue));
  },
  getBoolPref: (prefName) => {
    if (Services.prefs.getPrefType(prefName) !== Services.prefs.PREF_BOOL) {
      return Promise.resolve(null);
    }
    return Promise.resolve(Services.prefs.getBoolPref(prefName));
  },
  getIntPref: (prefName) => {
    if (Services.prefs.getPrefType(prefName) !== Services.prefs.PREF_INT) {
      return Promise.resolve(null);
    }
    return Promise.resolve(Services.prefs.getIntPref(prefName));
  },
  getStringPref: (prefName) => {
    if (prefName === "floorp.browser.note.memos") {
      return Promise.resolve(notesStorage!.getSnapshot().value);
    }
    if (Services.prefs.getPrefType(prefName) !== Services.prefs.PREF_STRING) {
      return Promise.resolve(null);
    }
    return Promise.resolve(Services.prefs.getStringPref(prefName));
  },
  setBoolPref: (prefName, value) => {
    Services.prefs.setBoolPref(prefName, value);
    return Promise.resolve();
  },
  setIntPref: (prefName, value) => {
    Services.prefs.setIntPref(prefName, value);
    return Promise.resolve();
  },
  setStringPref: (prefName, value) => {
    if (prefName === "floorp.browser.note.memos") {
      notesStorage!.writeLocal(value);
    } else {
      Services.prefs.setStringPref(prefName, value);
    }
    return Promise.resolve();
  },
};

export const rpc = isLocalhost5188
  ? createBirpc<NotesPrefFunctions, Record<string, never>>(
    {},
    {
      post: (data) => (globalThis as unknown as Window).NRSettingsSend(data),
      on: (callback) => {
        (globalThis as unknown as Window).NRSettingsRegisterReceiveCallback(
          callback,
        );
      },
      serialize: (v) => JSON.stringify(v),
      deserialize: (v) => JSON.parse(v),
    },
  )
  : directServicesFunctions;

// ──────────────────────────────────────────────────────────
// Exported pref observer API
// ──────────────────────────────────────────────────────────

// Registry for dev-mode polling handles
const devModePollers = new Map<
  PrefChangeCallback,
  ReturnType<typeof setInterval>
>();

/**
 * Register a callback that fires when the given string preference changes.
 *
 * In production (non-localhost) this uses `Services.prefs.addObserver`.
 * In development (localhost) this falls back to a polling-based approach
 * since the birpc bridge does not support real-time observers.
 *
 * **Important**: Call `removePrefObserver` on cleanup to avoid leaks.
 */
export function addPrefObserver(
  prefName: string,
  callback: PrefChangeCallback,
): PrefObserverHandle | null {
  if (isLocalhost5188) {
    // Dev mode: poll every 2 seconds as a fallback
    let lastSnapshot: NotesPreferenceSnapshot | null = null;
    let reading = false;
    // Initial read
    const initialRead = rpc
      .getNotesSnapshot()
      .then((v) => {
        lastSnapshot = v;
      })
      .catch(() => {});
    const pollId = setInterval(async () => {
      if (reading) return;
      reading = true;
      try {
        await initialRead;
        const current = await rpc.getNotesSnapshot();
        if (
          current.revision !== lastSnapshot?.revision ||
          current.epoch !== lastSnapshot?.epoch
        ) {
          lastSnapshot = current;
          callback(prefName, current);
        }
      } catch {
        // Pref may not exist yet; ignore
      } finally {
        reading = false;
      }
    }, 2000);
    devModePollers.set(callback, pollId);
    return null; // No real handle in dev mode
  }

  return addDirectPrefObserver(prefName, callback);
}

/**
 * Remove a previously registered pref observer.
 */
export function removePrefObserver(callback: PrefChangeCallback): void {
  if (isLocalhost5188) {
    const pollId = devModePollers.get(callback);
    if (pollId !== undefined) {
      clearInterval(pollId);
      devModePollers.delete(callback);
    }
    return;
  }
  removeDirectPrefObserver(callback);
}
