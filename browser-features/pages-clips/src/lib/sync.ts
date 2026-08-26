import type { Clip } from "@/types/clip.ts";

/**
 * Sync mode rides on a preference that Firefox Sync carries, the way Notes
 * does. A preference record is not a place for many megabytes, and a clip can
 * hold a compressed image, so only as much as fits under this cap travels —
 * newest first. The rest stays on the machine that made it.
 */
export const MAX_SYNC_BYTES = 512 * 1024;

export interface SyncPayload {
  clips: Clip[];
  /**
   * The `createdAt` of the oldest clip that fitted.
   *
   * Without it, "this clip is not in the payload" would be indistinguishable
   * from "someone deleted it", and a device holding older clips would keep
   * losing them. Deletions are only believed for clips at or after the floor.
   */
  floor: number;
}

/** The last-synced state: what each id looked like when we last agreed. */
export type SyncBase = Record<string, number>;

function byteLength(value: string): number {
  return new TextEncoder().encode(value).length;
}

/**
 * As many clips as fit under the cap, newest first.
 *
 * Pinned clips go in before the rest: they are the ones the user said to keep.
 */
export function selectForSync(
  clips: Clip[],
): { payload: SyncPayload; dropped: number } {
  const ordered = [...clips].sort((a, b) => {
    if (a.pinned !== b.pinned) return a.pinned ? -1 : 1;
    return b.createdAt - a.createdAt;
  });

  const kept: Clip[] = [];
  for (const clip of ordered) {
    const candidate = [...kept, clip];
    if (byteLength(JSON.stringify({ clips: candidate, floor: 0 })) > MAX_SYNC_BYTES) {
      break;
    }
    kept.push(clip);
  }

  const floor = kept.length === clips.length
    ? 0
    : kept.reduce((min, c) => Math.min(min, c.createdAt), Infinity);

  return {
    payload: { clips: kept, floor: Number.isFinite(floor) ? floor : 0 },
    dropped: clips.length - kept.length,
  };
}

export function serializePayload(payload: SyncPayload): string {
  return JSON.stringify(payload);
}

export function parsePayload(raw: string | null): SyncPayload | null {
  if (!raw) return null;
  try {
    const parsed = JSON.parse(raw) as unknown;
    if (
      typeof parsed !== "object" || parsed === null || !("clips" in parsed) ||
      !Array.isArray((parsed as SyncPayload).clips)
    ) {
      return null;
    }
    const payload = parsed as SyncPayload;
    return {
      clips: payload.clips,
      floor: typeof payload.floor === "number" ? payload.floor : 0,
    };
  } catch (e) {
    console.warn("[Floorp Clips] Could not read the synced payload:", e);
    return null;
  }
}

export function baseFromClips(clips: Clip[]): SyncBase {
  const base: SyncBase = {};
  for (const clip of clips) base[clip.id] = clip.updatedAt;
  return base;
}

export function parseBase(raw: string | null): SyncBase {
  if (!raw) return {};
  try {
    const parsed = JSON.parse(raw) as unknown;
    return typeof parsed === "object" && parsed !== null
      ? parsed as SyncBase
      : {};
  } catch {
    return {};
  }
}

/**
 * Merge what is here with what came from another device.
 *
 * The base is what the two sides last agreed on, so a clip missing from one
 * side can be read properly: gone from a side that had it means deleted;
 * missing from a side that never had it means new. A clip older than the
 * remote floor is never read as deleted — the other device may simply not
 * have had room for it.
 *
 * When both sides changed the same clip, the later `updatedAt` wins. That only
 * ever decides a pin, so there is nothing to lose either way.
 */
export function mergeClips(
  local: Clip[],
  remote: Clip[],
  base: SyncBase,
): Clip[] {
  const localById = new Map(local.map((c) => [c.id, c]));
  const remoteById = new Map(remote.map((c) => [c.id, c]));
  const remoteFloor = remote.length === 0 ? 0 : Math.min(
    ...remote.map((c) => c.createdAt),
  );

  const merged: Clip[] = [];
  for (const id of new Set([...localById.keys(), ...remoteById.keys()])) {
    const mine = localById.get(id);
    const theirs = remoteById.get(id);

    if (mine && theirs) {
      merged.push(theirs.updatedAt > mine.updatedAt ? theirs : mine);
      continue;
    }

    if (mine) {
      const wasShared = id in base;
      // Below the remote floor the other device is not saying anything about
      // this clip, so keep it.
      const belowFloor = remoteFloor > 0 && mine.createdAt < remoteFloor;
      const changedSinceSync = mine.updatedAt > (base[id] ?? 0);
      if (!wasShared || belowFloor || changedSinceSync) merged.push(mine);
      continue;
    }

    if (theirs) {
      const wasShared = id in base;
      const changedSinceSync = theirs.updatedAt > (base[id] ?? 0);
      if (!wasShared || changedSinceSync) merged.push(theirs);
    }
  }

  return merged.sort((a, b) => a.createdAt - b.createdAt);
}
