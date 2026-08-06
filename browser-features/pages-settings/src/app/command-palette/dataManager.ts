import { rpc } from "../../lib/rpc/rpc.ts";
import type { CommandPaletteFormData } from "../../types/pref.ts";

const COMMAND_PALETTE_ENABLED_PREF = "floorp.commandPalette.enabled";
const COMMAND_PALETTE_WIDTH_PREF = "floorp.commandPalette.width";
const COMMAND_PALETTE_MAX_HEIGHT_PREF = "floorp.commandPalette.maxHeight";
const COMMAND_PALETTE_OFFSET_TOP_PREF = "floorp.commandPalette.offsetTop";
const COMMAND_PALETTE_HORIZONTAL_ALIGN_PREF = "floorp.commandPalette.horizontalAlign";
const COMMAND_PALETTE_FONT_SIZE_PREF = "floorp.commandPalette.fontSize";
const COMMAND_PALETTE_SHOW_TABS_PREF = "floorp.commandPalette.showTabs";
const COMMAND_PALETTE_SHOW_HISTORY_PREF = "floorp.commandPalette.showHistory";
const COMMAND_PALETTE_SHOW_BOOKMARKS_PREF = "floorp.commandPalette.showBookmarks";
const COMMAND_PALETTE_CATEGORY_PRIORITY_PREF = "floorp.commandPalette.categoryPriority";
const COMMAND_PALETTE_MAX_RESULTS_PER_CATEGORY_PREF = "floorp.commandPalette.maxResultsPerCategory";

const DEFAULT_WIDTH = 560;
const DEFAULT_MAX_HEIGHT = 400;
const DEFAULT_OFFSET_TOP = 20;
const DEFAULT_HORIZONTAL_ALIGN = "center";
const DEFAULT_FONT_SIZE = 14;
const DEFAULT_SHOW_TABS = true;
const DEFAULT_SHOW_HISTORY = true;
const DEFAULT_SHOW_BOOKMARKS = true;
const DEFAULT_MAX_RESULTS_PER_CATEGORY = 5;

/**
 * Default category priority for the command palette settings UI.
 *
 * NOTE: This list is DUPLICATED in
 * `browser-features/chrome/common/command-palette/category-priority.ts`
 * (the live palette feature). The two cannot share a module because the
 * settings app and chrome feature are separate packages. If you edit one,
 * edit the other. The test in this file asserts length===19 as a partial guard.
 */
export const DEFAULT_CATEGORY_PRIORITY: readonly string[] = [
  "navigation",
  "tabs",
  "zoom",
  "bookmarks",
  "page",
  "search",
  "sidebar",
  "scrolling",
  "history",
  "window",
  "tools",
  "downloads",
  "workspace",
  "floorp",
  "media",
  "open-tabs",
  "switcher",
  "history-suggestions",
  "bookmark-suggestions",
] as const;

const WIDTH_BOUNDS = { min: 400, max: 1000 } as const;
const MAX_HEIGHT_BOUNDS = { min: 300, max: 800 } as const;
const OFFSET_TOP_BOUNDS = { min: 0, max: 60 } as const;
const FONT_SIZE_BOUNDS = { min: 11, max: 22 } as const;
// KEEP IN SYNC with:
// - browser-features/chrome/common/command-palette/config.ts
// - the Seekbar min/max props in browser-features/pages-settings/src/app/command-palette/components/ResultLimitSettings.tsx
const MAX_RESULTS_PER_CATEGORY_BOUNDS = { min: 1, max: 20 } as const;
const VALID_HORIZONTAL_ALIGNS = ["center", "left", "right"] as const;

export const COMMAND_PALETTE_APPEARANCE_DEFAULTS = {
  width: DEFAULT_WIDTH,
  maxHeight: DEFAULT_MAX_HEIGHT,
  offsetTop: DEFAULT_OFFSET_TOP,
  horizontalAlign: DEFAULT_HORIZONTAL_ALIGN,
  fontSize: DEFAULT_FONT_SIZE,
} as const;

function clampInt(
  value: number,
  bounds: { min: number; max: number },
  fallback: number,
): number {
  if (!Number.isFinite(value)) return fallback;
  const i = Math.round(value);
  return Math.min(bounds.max, Math.max(bounds.min, i));
}

function parseCategoryPriority(raw: string | null): string[] {
  if (typeof raw !== "string" || raw.length === 0) {
    return [...DEFAULT_CATEGORY_PRIORITY];
  }
  let parsed: unknown;
  try {
    parsed = JSON.parse(raw);
  } catch {
    return [...DEFAULT_CATEGORY_PRIORITY];
  }
  if (
    !Array.isArray(parsed) ||
    !parsed.every((el): el is string => typeof el === "string")
  ) {
    return [...DEFAULT_CATEGORY_PRIORITY];
  }
  return parsed;
}

export async function saveCommandPaletteSettings(
  settings: Partial<CommandPaletteFormData>,
): Promise<null | void> {
  if (Object.keys(settings).length === 0) {
    return;
  }

  try {
    await rpc.setBoolPref(
      COMMAND_PALETTE_ENABLED_PREF,
      Boolean(settings.enabled),
    );

    if (settings.width !== undefined) {
      await rpc.setIntPref(
        COMMAND_PALETTE_WIDTH_PREF,
        clampInt(Number(settings.width), WIDTH_BOUNDS, DEFAULT_WIDTH),
      );
    }

    if (settings.maxHeight !== undefined) {
      await rpc.setIntPref(
        COMMAND_PALETTE_MAX_HEIGHT_PREF,
        clampInt(Number(settings.maxHeight), MAX_HEIGHT_BOUNDS, DEFAULT_MAX_HEIGHT),
      );
    }

    if (settings.offsetTop !== undefined) {
      await rpc.setIntPref(
        COMMAND_PALETTE_OFFSET_TOP_PREF,
        clampInt(Number(settings.offsetTop), OFFSET_TOP_BOUNDS, DEFAULT_OFFSET_TOP),
      );
    }

    if (settings.horizontalAlign !== undefined) {
      const v = String(settings.horizontalAlign);
      await rpc.setStringPref(
        COMMAND_PALETTE_HORIZONTAL_ALIGN_PREF,
        (VALID_HORIZONTAL_ALIGNS as readonly string[]).includes(v)
          ? v
          : DEFAULT_HORIZONTAL_ALIGN,
      );
    }

    if (settings.fontSize !== undefined) {
      await rpc.setIntPref(
        COMMAND_PALETTE_FONT_SIZE_PREF,
        clampInt(Number(settings.fontSize), FONT_SIZE_BOUNDS, DEFAULT_FONT_SIZE),
      );
    }

    if (settings.showTabs !== undefined) {
      await rpc.setBoolPref(
        COMMAND_PALETTE_SHOW_TABS_PREF,
        Boolean(settings.showTabs),
      );
    }

    if (settings.showHistory !== undefined) {
      await rpc.setBoolPref(
        COMMAND_PALETTE_SHOW_HISTORY_PREF,
        Boolean(settings.showHistory),
      );
    }

    if (settings.showBookmarks !== undefined) {
      await rpc.setBoolPref(
        COMMAND_PALETTE_SHOW_BOOKMARKS_PREF,
        Boolean(settings.showBookmarks),
      );
    }

    if (settings.categoryPriority !== undefined) {
      await rpc.setStringPref(
        COMMAND_PALETTE_CATEGORY_PRIORITY_PREF,
        JSON.stringify(settings.categoryPriority),
      );
    }

    if (settings.maxResultsPerCategory !== undefined) {
      await rpc.setIntPref(
        COMMAND_PALETTE_MAX_RESULTS_PER_CATEGORY_PREF,
        clampInt(
          settings.maxResultsPerCategory,
          MAX_RESULTS_PER_CATEGORY_BOUNDS,
          DEFAULT_MAX_RESULTS_PER_CATEGORY,
        ),
      );
    }
  } catch (error) {
    console.error("[command-palette] Failed to save settings:", error);
  }
}

export async function getCommandPaletteSettings(): Promise<CommandPaletteFormData | null> {
  try {
    const enabled = await rpc.getBoolPref(COMMAND_PALETTE_ENABLED_PREF);
    const width = await rpc.getIntPref(COMMAND_PALETTE_WIDTH_PREF);
    const maxHeight = await rpc.getIntPref(COMMAND_PALETTE_MAX_HEIGHT_PREF);
    const offsetTop = await rpc.getIntPref(COMMAND_PALETTE_OFFSET_TOP_PREF);
    const horizontalAlign = await rpc.getStringPref(
      COMMAND_PALETTE_HORIZONTAL_ALIGN_PREF,
    );
    const fontSize = await rpc.getIntPref(COMMAND_PALETTE_FONT_SIZE_PREF);
    const showTabs = await rpc.getBoolPref(COMMAND_PALETTE_SHOW_TABS_PREF);
    const showHistory = await rpc.getBoolPref(COMMAND_PALETTE_SHOW_HISTORY_PREF);
    const showBookmarks = await rpc.getBoolPref(
      COMMAND_PALETTE_SHOW_BOOKMARKS_PREF,
    );
    const categoryPriorityRaw = await rpc.getStringPref(
      COMMAND_PALETTE_CATEGORY_PRIORITY_PREF,
    );
    const maxResultsPerCategoryRaw = await rpc.getIntPref(
      COMMAND_PALETTE_MAX_RESULTS_PER_CATEGORY_PREF,
    );

    return {
      enabled: enabled === null ? true : enabled,
      width: clampInt(width ?? DEFAULT_WIDTH, WIDTH_BOUNDS, DEFAULT_WIDTH),
      maxHeight: clampInt(maxHeight ?? DEFAULT_MAX_HEIGHT, MAX_HEIGHT_BOUNDS, DEFAULT_MAX_HEIGHT),
      offsetTop: clampInt(offsetTop ?? DEFAULT_OFFSET_TOP, OFFSET_TOP_BOUNDS, DEFAULT_OFFSET_TOP),
      horizontalAlign: (VALID_HORIZONTAL_ALIGNS as readonly string[]).includes(
        horizontalAlign ?? DEFAULT_HORIZONTAL_ALIGN,
      )
        ? (horizontalAlign as string)
        : DEFAULT_HORIZONTAL_ALIGN,
      fontSize: clampInt(fontSize ?? DEFAULT_FONT_SIZE, FONT_SIZE_BOUNDS, DEFAULT_FONT_SIZE),
      showTabs: showTabs === null ? DEFAULT_SHOW_TABS : showTabs,
      showHistory: showHistory === null ? DEFAULT_SHOW_HISTORY : showHistory,
      showBookmarks: showBookmarks === null ? DEFAULT_SHOW_BOOKMARKS : showBookmarks,
      categoryPriority: parseCategoryPriority(categoryPriorityRaw),
      maxResultsPerCategory: clampInt(
        maxResultsPerCategoryRaw ?? DEFAULT_MAX_RESULTS_PER_CATEGORY,
        MAX_RESULTS_PER_CATEGORY_BOUNDS,
        DEFAULT_MAX_RESULTS_PER_CATEGORY,
      ),
    };
  } catch (error) {
    console.error("[command-palette] Failed to load settings:", error);
    return null;
  }
}
