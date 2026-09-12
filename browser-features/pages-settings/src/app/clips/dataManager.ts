import { rpc } from "@/lib/rpc/rpc.ts";

export type ClipsMode = "local" | "sync" | "clipboard";
export type FileAction = "reveal" | "launch";

export interface ClipsSettings {
  mode: ClipsMode;
  maxItems: number;
  clearOnExit: boolean;
  fileAction: FileAction;
}

/** Kept in step with browser-features/pages-clips/src/lib/settings.ts. */
const MODE_PREF = "floorp.browser.clips.mode";
const MAX_ITEMS_PREF = "floorp.browser.clips.maxItems";
const CLEAR_ON_EXIT_PREF = "floorp.browser.clips.clearOnExit";
const FILE_ACTION_PREF = "floorp.browser.clips.fileAction";

export const DEFAULT_SETTINGS: ClipsSettings = {
  mode: "local",
  maxItems: 64,
  clearOnExit: false,
  fileAction: "reveal",
};

function isMode(value: string | null): value is ClipsMode {
  return value === "local" || value === "sync" || value === "clipboard";
}

export async function getClipsSettings(): Promise<ClipsSettings> {
  const [mode, maxItems, clearOnExit, fileAction] = await Promise.all([
    rpc.getStringPref(MODE_PREF).catch(() => null),
    rpc.getIntPref(MAX_ITEMS_PREF).catch(() => null),
    rpc.getBoolPref(CLEAR_ON_EXIT_PREF).catch(() => null),
    rpc.getStringPref(FILE_ACTION_PREF).catch(() => null),
  ]);

  return {
    mode: isMode(mode) ? mode : DEFAULT_SETTINGS.mode,
    maxItems: maxItems !== null && maxItems > 0
      ? maxItems
      : DEFAULT_SETTINGS.maxItems,
    clearOnExit: clearOnExit ?? DEFAULT_SETTINGS.clearOnExit,
    fileAction: fileAction === "launch" ? "launch" : "reveal",
  };
}

export async function saveClipsSettings(
  settings: ClipsSettings,
): Promise<void> {
  await Promise.all([
    rpc.setStringPref(MODE_PREF, settings.mode),
    rpc.setIntPref(MAX_ITEMS_PREF, settings.maxItems),
    rpc.setBoolPref(CLEAR_ON_EXIT_PREF, settings.clearOnExit),
    rpc.setStringPref(FILE_ACTION_PREF, settings.fileAction),
  ]);
}
