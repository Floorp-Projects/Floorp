import type { Note, NoteSnapshot } from "../../pages-notes/src/lib/merge.ts";

export function decodeNotes(
  value: string,
  allowLegacy = false,
  makeId: () => string = () => crypto.randomUUID(),
): Note[] {
  const raw: unknown = JSON.parse(value);
  if (typeof raw !== "object" || raw === null) {
    throw new TypeError("Invalid Notes record");
  }
  const data = raw as Record<string, unknown>;
  if (
    !Array.isArray(data.titles) || !Array.isArray(data.contents) ||
    data.titles.length !== data.contents.length
  ) {
    throw new TypeError("Invalid Notes arrays");
  }
  const length = data.titles.length;
  const arrays = ["ids", "createdAts", "updatedAts"] as const;
  for (const key of arrays) {
    if (allowLegacy && data[key] === undefined) continue;
    if (!Array.isArray(data[key]) || data[key].length !== length) {
      throw new TypeError(`Invalid Notes ${key}`);
    }
  }
  const seen = new Set<string>();
  return data.titles.map((title, index) => {
    const content = (data.contents as unknown[])[index];
    // Only genuinely missing legacy arrays may receive migration defaults.
    // Invalid entries in a received record must never create fresh identities.
    const id = data.ids === undefined && allowLegacy
      ? makeId()
      : (data.ids as unknown[])[index];
    const createdAt = data.createdAts === undefined && allowLegacy
      ? Date.now()
      : (data.createdAts as unknown[])[index];
    const updatedAt = data.updatedAts === undefined && allowLegacy
      ? createdAt
      : (data.updatedAts as unknown[])[index];
    if (
      typeof title !== "string" || typeof content !== "string" ||
      typeof id !== "string" || !id.trim() || seen.has(id) ||
      typeof createdAt !== "number" || !Number.isSafeInteger(createdAt) ||
      createdAt < 0 || typeof updatedAt !== "number" ||
      !Number.isSafeInteger(updatedAt) || updatedAt < 0
    ) {
      throw new TypeError("Invalid Notes item");
    }
    seen.add(id);
    return { id, title, content, createdAt, updatedAt };
  });
}

export function encodeNotes(notes: Note[]): string {
  return JSON.stringify({
    ids: notes.map((note) => note.id),
    titles: notes.map((note) => note.title),
    contents: notes.map((note) => note.content),
    createdAts: notes.map((note) => note.createdAt),
    updatedAts: notes.map((note) => note.updatedAt),
  });
}

export function notesBase(notes: Note[]): Record<string, NoteSnapshot> {
  return Object.fromEntries(
    notes.map((
      { id, title, content, updatedAt },
    ) => [id, { id, title, content, updatedAt }]),
  );
}

export function decodeNotesBase(
  value: string | null,
): Record<string, NoteSnapshot> {
  if (!value) return {};
  try {
    const raw: unknown = JSON.parse(value);
    if (
      typeof raw !== "object" || raw === null ||
      !("lastSyncedSnapshots" in raw) ||
      typeof raw.lastSyncedSnapshots !== "object" ||
      raw.lastSyncedSnapshots === null || Array.isArray(raw.lastSyncedSnapshots)
    ) return {};
    const result: Record<string, NoteSnapshot> = {};
    for (const [key, item] of Object.entries(raw.lastSyncedSnapshots)) {
      if (
        typeof item !== "object" || item === null || !("id" in item) ||
        item.id !== key || !key.trim() || !("title" in item) ||
        typeof item.title !== "string" || !("content" in item) ||
        typeof item.content !== "string" || !("updatedAt" in item) ||
        typeof item.updatedAt !== "number" ||
        !Number.isSafeInteger(item.updatedAt) || item.updatedAt < 0
      ) return {};
      result[key] = {
        id: key,
        title: item.title,
        content: item.content,
        updatedAt: item.updatedAt,
      };
    }
    return result;
  } catch {
    return {};
  }
}
