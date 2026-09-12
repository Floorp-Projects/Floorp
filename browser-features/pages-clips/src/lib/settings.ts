import { rpc } from "@/lib/rpc/rpc.ts";

/**
 * How Clips behaves.
 *
 * - `local`: everything stays on this machine. The default.
 * - `sync`: the clips ride along on Firefox Sync, like Notes.
 * - `clipboard`: while the page is open, whatever you copy becomes a clip.
 *   Also local-only.
 */
export type ClipsMode = "local" | "sync" | "clipboard";

/** What happens when a file clip is opened. */
export type FileAction = "reveal" | "launch";

export const MODE_PREF = "floorp.browser.clips.mode";
export const MAX_ITEMS_PREF = "floorp.browser.clips.maxItems";
export const CLEAR_ON_EXIT_PREF = "floorp.browser.clips.clearOnExit";
export const FILE_ACTION_PREF = "floorp.browser.clips.fileAction";
/** The synced payload. Listed in override.ini as a pref Sync carries. */
export const DATA_PREF = "floorp.browser.clips.data";
/** The last synced state, used as the base of the three-way merge. */
export const SYNC_STATE_PREF = "floorp.browser.clips.syncState";
/**
 * What the page saw the last time it ran: which browser session, and which
 * mode. Both are needed to notice something that happened while the page was
 * closed — a restart, or a mode switch made from the settings page.
 */
export const PAGE_STATE_PREF = "floorp.browser.clips.pageState";
/** A clip handed over by an action while the page was not open. */
export const PENDING_PREF = "floorp.browser.clips.pending";

export const DEFAULT_MAX_ITEMS = 64;

export interface ClipsSettings {
  mode: ClipsMode;
  maxItems: number;
  clearOnExit: boolean;
  fileAction: FileAction;
}

function isMode(value: string | null): value is ClipsMode {
  return value === "local" || value === "sync" || value === "clipboard";
}

export async function getSettings(): Promise<ClipsSettings> {
  const [mode, maxItems, clearOnExit, fileAction] = await Promise.all([
    rpc.getStringPref(MODE_PREF).catch(() => null),
    rpc.getIntPref(MAX_ITEMS_PREF).catch(() => null),
    rpc.getBoolPref(CLEAR_ON_EXIT_PREF).catch(() => null),
    rpc.getStringPref(FILE_ACTION_PREF).catch(() => null),
  ]);

  return {
    mode: isMode(mode) ? mode : "local",
    maxItems: maxItems !== null && Number.isFinite(maxItems) && maxItems > 0
      ? maxItems
      : DEFAULT_MAX_ITEMS,
    clearOnExit: clearOnExit ?? false,
    fileAction: fileAction === "launch" ? "launch" : "reveal",
  };
}

export interface PageState {
  sessionStart: number;
  mode: ClipsMode | null;
}

export async function getPageState(): Promise<PageState> {
  try {
    const raw = await rpc.getStringPref(PAGE_STATE_PREF);
    if (!raw) return { sessionStart: 0, mode: null };
    const parsed = JSON.parse(raw) as Partial<PageState>;
    return {
      sessionStart: typeof parsed.sessionStart === "number"
        ? parsed.sessionStart
        : 0,
      mode: isMode(parsed.mode ?? null) ? parsed.mode as ClipsMode : null,
    };
  } catch {
    return { sessionStart: 0, mode: null };
  }
}

export async function savePageState(state: PageState): Promise<void> {
  try {
    await rpc.setStringPref(PAGE_STATE_PREF, JSON.stringify(state));
  } catch (e) {
    console.error("[Floorp Clips] Failed to remember the page state:", e);
  }
}
