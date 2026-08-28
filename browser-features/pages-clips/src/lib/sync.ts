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
   * The ids of the clips that stayed home because there was no room.
   *
   * Without it, "this clip is not in the payload" would be indistinguishable
   * from "someone deleted it", and a device holding the rest would keep losing
   * them. Naming them says exactly which absences mean nothing; every other
   * absence is a deletion, and can be believed.
   *
   * This used to be one timestamp, a water line under which the payload said
   * nothing. But pinned clips travel first, so what stays home is not simply
   * the older end — the line sat below clips that had stayed, and called the
   * payload complete where it was not. Drawn high enough to cover them, it
   * then buried every real deletion under it for as long as one clip too big
   * to ever travel sat there. Names have neither problem, and cost a few
   * bytes each.
   */
  stayed: string[];
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
 * A clip too big for the room that is left is stepped over rather than ending
 * the walk — one large image should not keep everything behind it at home.
 */
export function selectForSync(
  clips: Clip[],
): { payload: SyncPayload; dropped: number } {
  const ordered = [...clips].sort((a, b) => {
    if (a.pinned !== b.pinned) return a.pinned ? -1 : 1;
    return b.createdAt - a.createdAt;
  });

  // A payload is the wrapper, each carried clip's own JSON, each stayed-home
  // name, and a comma between siblings — so each clip can be measured once
  // instead of writing the whole payload out again for every candidate.
  // Every clip is one or the other, so the room for all the names is put aside
  // before the walk begins and the walk just spends what is left. That puts
  // aside a little more than is needed, which is the price of one pass.
  const wrapper = byteLength(JSON.stringify({ clips: [], stayed: [] }));
  const namesRoom = clips.reduce(
    (room, c) => room + byteLength(JSON.stringify(c.id)) + 1,
    0,
  );
  const room = MAX_SYNC_BYTES - wrapper - namesRoom;

  const kept: Clip[] = [];
  const stayed: string[] = [];
  let used = 0;
  for (const clip of ordered) {
    const cost = byteLength(JSON.stringify(clip)) + (kept.length > 0 ? 1 : 0);
    if (used + cost > room) {
      stayed.push(clip.id);
      continue;
    }
    used += cost;
    kept.push(clip);
  }

  return { payload: { clips: kept, stayed }, dropped: stayed.length };
}

export function serializePayload(payload: SyncPayload): string {
  return JSON.stringify(payload);
}

/** A payload as it was read: `stayed` is null when it did not say. */
export type ReadPayload = Omit<SyncPayload, "stayed"> & {
  stayed: string[] | null;
};

export function parsePayload(raw: string | null): ReadPayload | null {
  if (!raw) return null;
  try {
    const parsed = JSON.parse(raw) as unknown;
    if (
      typeof parsed !== "object" || parsed === null || !("clips" in parsed) ||
      !Array.isArray((parsed as SyncPayload).clips)
    ) {
      return null;
    }
    const stayed = (parsed as { stayed?: unknown }).stayed;
    return {
      clips: (parsed as SyncPayload).clips,
      // A payload from before there were names does not say which absences
      // meant nothing, so it is read as saying nothing about any of them. It
      // sorts itself out the next time that device publishes.
      stayed: Array.isArray(stayed)
        ? stayed.filter((id): id is string => typeof id === "string")
        : null,
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
 * The state to record after publishing `published`, out of `held` — everything
 * we still have.
 *
 * An id we sent before and do not have any more is really gone: it is
 * remembered, stamped with now, until it is old enough to let go. An id we
 * still hold and only left out because it did not fit under the cap is not
 * gone at all — the other device still has what we last sent it, so keep
 * saying so rather than telling it to forget.
 */
export function nextSyncState(
  previous: SyncState,
  published: Clip[],
  held: Clip[],
  now: number = Date.now(),
): SyncState {
  const clips = baseFromClips(published);
  const gone: SyncBase = {};
  const stillHere = new Set(held.map((c) => c.id));

  for (const [id, at] of Object.entries(previous.gone)) {
    if (id in clips || stillHere.has(id)) continue;
    if (now - at <= GONE_KEPT_MS) gone[id] = at;
  }
  for (const [id, at] of Object.entries(previous.clips)) {
    if (id in clips) continue;
    if (stillHere.has(id)) clips[id] = at;
    else if (!(id in gone)) gone[id] = now;
  }
  return { clips, gone };
}

/**
 * Merge what is here with what came from another device.
 *
 * The base is what the two sides last agreed on, so a clip missing from one
 * side can be read properly: gone from a side that had it means deleted;
 * missing from a side that never had it means new. A clip the other device
 * named as having stayed home is never read as deleted — it simply did not
 * have room for it, and is saying so.
 *
 * `stayedThere` is null for a payload that does not say which absences meant
 * nothing. Nothing there can be read as a deletion, which is the safe way to
 * be unsure.
 *
 * When both sides changed the same clip, the later `updatedAt` wins. That only
 * ever decides a pin, so there is nothing to lose either way.
 */
export function mergeClips(
  local: Clip[],
  remote: Clip[],
  base: SyncBase,
  stayedThere: readonly string[] | null,
): Clip[] {
  const localById = new Map(local.map((c) => [c.id, c]));
  const remoteById = new Map(remote.map((c) => [c.id, c]));
  const couldNotTravel = stayedThere === null ? null : new Set(stayedThere);

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
      // Named as having stayed home, or from a payload that does not say:
      // the other device is not telling us anything about this clip.
      const unspoken = couldNotTravel === null || couldNotTravel.has(id);
      const changedSinceSync = mine.updatedAt > (base[id] ?? 0);
      if (!wasShared || unspoken || changedSinceSync) merged.push(mine);
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
