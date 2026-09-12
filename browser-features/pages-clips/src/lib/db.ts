import type { Clip } from "@/types/clip.ts";

/**
 * Clips live in IndexedDB, not in a preference.
 *
 * Notes keeps everything in one string pref, which works because notes are
 * text. Clips holds compressed images too, and prefs.js is read whole at
 * startup — 64 clips worth of data URLs there would be felt every launch.
 */
const DB_NAME = "floorp-clips";
const DB_VERSION = 1;
const STORE = "clips";

let dbPromise: Promise<IDBDatabase> | null = null;

function openDb(): Promise<IDBDatabase> {
  if (dbPromise) return dbPromise;
  dbPromise = new Promise((resolve, reject) => {
    const req = indexedDB.open(DB_NAME, DB_VERSION);
    req.onupgradeneeded = () => {
      const db = req.result;
      if (!db.objectStoreNames.contains(STORE)) {
        const store = db.createObjectStore(STORE, { keyPath: "id" });
        store.createIndex("createdAt", "createdAt");
      }
    };
    req.onsuccess = () => resolve(req.result);
    req.onerror = () => reject(req.error);
  });
  return dbPromise;
}

function tx(
  db: IDBDatabase,
  mode: IDBTransactionMode,
): [IDBObjectStore, Promise<void>] {
  const t = db.transaction(STORE, mode);
  const done = new Promise<void>((resolve, reject) => {
    t.oncomplete = () => resolve();
    t.onerror = () => reject(t.error);
    t.onabort = () => reject(t.error);
  });
  return [t.objectStore(STORE), done];
}

/** All clips, oldest first — the order they are shown in. */
export async function getAllClips(): Promise<Clip[]> {
  const db = await openDb();
  const [store] = tx(db, "readonly");
  const req = store.index("createdAt").getAll();
  return await new Promise((resolve, reject) => {
    req.onsuccess = () => resolve(req.result as Clip[]);
    req.onerror = () => reject(req.error);
  });
}

export async function putClip(clip: Clip): Promise<void> {
  const db = await openDb();
  const [store, done] = tx(db, "readwrite");
  store.put(clip);
  await done;
}

export async function putClips(clips: Clip[]): Promise<void> {
  if (clips.length === 0) return;
  const db = await openDb();
  const [store, done] = tx(db, "readwrite");
  for (const clip of clips) store.put(clip);
  await done;
}

/** Make the store hold exactly these clips — used after a sync merge. */
export async function replaceAll(clips: Clip[]): Promise<void> {
  const db = await openDb();
  const [store, done] = tx(db, "readwrite");
  store.clear();
  for (const clip of clips) store.put(clip);
  await done;
}

export async function deleteClips(ids: string[]): Promise<void> {
  if (ids.length === 0) return;
  const db = await openDb();
  const [store, done] = tx(db, "readwrite");
  for (const id of ids) store.delete(id);
  await done;
}
