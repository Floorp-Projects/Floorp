import { rpc } from "@/lib/rpc/rpc.ts";
import { DATA_PREF, SYNC_STATE_PREF } from "@/lib/settings.ts";
import {
  baseFromClips,
  mergeClips,
  parseBase,
  parsePayload,
  selectForSync,
  serializePayload,
} from "@/lib/sync.ts";
import type { Clip } from "@/types/clip.ts";

/**
 * True while this page is the one writing the synced pref.
 *
 * The pref observer cannot tell our own write apart from one Sync brought in,
 * and reacting to our own would send us round in a circle.
 */
let writing = false;

export function isWritingSync(): boolean {
  return writing;
}

/** Merge whatever is in the synced pref into the clips we hold. */
export async function pullAndMerge(local: Clip[]): Promise<Clip[] | null> {
  try {
    const payload = parsePayload(await rpc.getStringPref(DATA_PREF));
    if (!payload) return null;
    const base = parseBase(await rpc.getStringPref(SYNC_STATE_PREF));
    return mergeClips(local, payload.clips, base);
  } catch (e) {
    console.error("[Floorp Clips] Failed to read the synced clips:", e);
    return null;
  }
}

/**
 * Publish the clips, and record what we published as the base for next time.
 *
 * Returns how many clips did not fit under the size cap — the caller says so
 * in the UI rather than letting them quietly not travel.
 */
export async function push(clips: Clip[]): Promise<number> {
  const { payload, dropped } = selectForSync(clips);
  writing = true;
  try {
    await rpc.setStringPref(DATA_PREF, serializePayload(payload));
    await rpc.setStringPref(
      SYNC_STATE_PREF,
      JSON.stringify(baseFromClips(payload.clips)),
    );
  } catch (e) {
    console.error("[Floorp Clips] Failed to publish the clips:", e);
  } finally {
    // Let the observer that our own write triggers go by first.
    setTimeout(() => (writing = false), 0);
  }
  return dropped;
}
