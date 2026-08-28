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

/**
 * How long a clip that left the payload is remembered.
 *
 * Long enough for a device that was shut away in a drawer to come back and
 * still hear about the deletion, short enough that the record does not grow
 * without end — it rides in a preference too.
 */
export const GONE_KEPT_MS = 30 * 24 * 60 * 60 * 1000;

/** What we published last time, and what stopped being published. */
export interface SyncState {
  /** `updatedAt` of each clip in the last payload we sent. */
  clips: SyncBase;
  /**
   * Ids that were in an earlier payload and are not any more, and when they
   * left.
   *
   * A deleted clip has to leave something behind. A device that has not heard
   * about the deletion yet keeps sending the clip back, and with nothing to
   * read it against it looks new — so it comes home, and the deletion never
   * sticks. These records are what it is read against.
   */
  gone: SyncBase;
}

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

function asBase(value: unknown): SyncBase {
  return typeof value === "object" && value !== null ? value as SyncBase : {};
}

/**
 * Read the stored state. Profiles written before there were `gone` records
 * hold the bare map, so read that as the published clips.
 */
export function parseSyncState(raw: string | null): SyncState {
  if (!raw) return { clips: {}, gone: {} };
  try {
    const parsed = JSON.parse(raw) as Record<string, unknown>;
    if (typeof parsed !== "object" || parsed === null) {
      return { clips: {}, gone: {} };
    }
    if (typeof parsed.clips === "object" && parsed.clips !== null) {
      return { clips: asBase(parsed.clips), gone: asBase(parsed.gone) };
    }
    return { clips: asBase(parsed), gone: {} };
  } catch {
    return { clips: {}, gone: {} };
  }
}

export function serializeSyncState(state: SyncState): string {
  return JSON.stringify(state);
}

/** The one map `mergeClips` reads against: what was published, and what left. */
export function baseOf(state: SyncState): SyncBase {
  return { ...state.gone, ...state.clips };
}

/**
 * The state to record after publishing `published`.
 *
 * An id that was in the last payload and is not in this one has left: it is
 * remembered, stamped with now, until it is old enough to let go. A clip that
 * only left because it did not fit under the cap is remembered too — the
 * remote floor is what speaks for those, and it still does.
 */
export function nextSyncState(
  previous: SyncState,
  published: Clip[],
  now: number = Date.now(),
): SyncState {
  const clips = baseFromClips(published);
  const gone: SyncBase = {};
  for (const [id, at] of Object.entries(previous.gone)) {
    if (id in clips) continue;
    if (now - at <= GONE_KEPT_MS) gone[id] = at;
  }
  for (const id of Object.keys(previous.clips)) {
    if (id in clips || id in gone) continue;
    gone[id] = now;
  }
  return { clips, gone };
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
