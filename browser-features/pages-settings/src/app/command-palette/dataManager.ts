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
const COMMAND_PALETTE_MAX_BOOKMARK_SUGGESTIONS_PREF = "floorp.commandPalette.maxBookmarkSuggestions";
const COMMAND_PALETTE_MAX_HISTORY_SUGGESTIONS_PREF = "floorp.commandPalette.maxHistorySuggestions";
const COMMAND_PALETTE_MAX_TABS_RESULTS_PREF = "floorp.commandPalette.maxTabsResults";

// KEEP IN SYNC with the chrome-side command-palette config:
// - browser-features/chrome/common/command-palette/config.ts
// The pref names, JSON shapes, and default values below are DUPLICATED on the
// chrome side. The settings app and chrome feature are separate packages and
// cannot share a module, so if you edit one side, edit the other.
//   shortcuts          -> user-editable {prefix, commandId} pairs (read/write)
//   selectableCommands -> chrome-cached command catalog (read-only here)
// The shortcuts pref defaults to "[]" (empty). @s, @t, @b and @h are
// built-in reserved prefixes handled directly by the controller — no pref
// entry is needed. `loadShortcuts()` migrates away any stale "s" or "t"
// entries that linger from before the prefixes were reserved.
const COMMAND_PALETTE_SHORTCUTS_PREF = "floorp.commandPalette.shortcuts";
const COMMAND_PALETTE_SELECTABLE_COMMANDS_PREF =
  "floorp.commandPalette.selectableCommands";

/**
 * Prefixes reserved for built-in command palette behavior. The settings UI
 * must not allow users to create or remove shortcuts with these prefixes
 * (@s = web search, @t = open-tabs search, @b = bookmark search, @h =
 * history search).
 *
 * KEEP IN SYNC with:
 * - browser-features/chrome/common/command-palette/config.ts
 */
export const RESERVED_SHORTCUT_PREFIXES: readonly string[] = [
  "s",
  "t",
  "b",
  "h",
];

export function isReservedShortcutPrefix(prefix: string): boolean {
  return RESERVED_SHORTCUT_PREFIXES.includes(prefix);
}

const DEFAULT_WIDTH = 560;
const DEFAULT_MAX_HEIGHT = 400;
const DEFAULT_OFFSET_TOP = 20;
const DEFAULT_HORIZONTAL_ALIGN = "center";
const DEFAULT_FONT_SIZE = 14;
const DEFAULT_SHOW_TABS = true;
const DEFAULT_SHOW_HISTORY = true;
const DEFAULT_SHOW_BOOKMARKS = true;
const DEFAULT_MAX_RESULTS_PER_CATEGORY = 5;
const DEFAULT_MAX_BOOKMARK_SUGGESTIONS = 5;
const DEFAULT_MAX_HISTORY_SUGGESTIONS = 5;
const DEFAULT_MAX_TABS_RESULTS = 5;

/**
 * Default category priority for the command palette settings UI.
 *
 * NOTE: This list is DUPLICATED in
 * `browser-features/chrome/common/command-palette/category-priority.ts`
 * (the live palette feature). The two cannot share a module because the
 * settings app and chrome feature are separate packages. If you edit one,
 * edit the other. The test in this file asserts length===18 as a partial guard.
 */
export const DEFAULT_CATEGORY_PRIORITY: readonly string[] = [
  "navigation",
  "tabs",
  "zoom",
  "bookmarks",
  "page",
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
// KEEP IN SYNC with:
// - browser-features/chrome/common/command-palette/config.ts
// - the Seekbar min/max props in DynamicSearchSettings.tsx
const MAX_BOOKMARK_SUGGESTIONS_BOUNDS = { min: 1, max: 20 } as const;
const MAX_HISTORY_SUGGESTIONS_BOUNDS = { min: 1, max: 20 } as const;
const MAX_TABS_RESULTS_BOUNDS = { min: 1, max: 20 } as const;
const VALID_HORIZONTAL_ALIGNS = ["center", "left", "right"] as const;

export const COMMAND_PALETTE_APPEARANCE_DEFAULTS = {
  width: DEFAULT_WIDTH,
  maxHeight: DEFAULT_MAX_HEIGHT,
  offsetTop: DEFAULT_OFFSET_TOP,
  horizontalAlign: DEFAULT_HORIZONTAL_ALIGN,
  fontSize: DEFAULT_FONT_SIZE,
} as const;

/**
 * Default values for the whole command-palette settings form. Used as the
 * react-hook-form `defaultValues` so reads before the initial async pref
 * load still resolve defined values. `categoryPriority` is copied so callers
 * can never mutate `DEFAULT_CATEGORY_PRIORITY` through this object.
 */
export const COMMAND_PALETTE_DEFAULT_VALUES: CommandPaletteFormData = {
  enabled: true,
  width: DEFAULT_WIDTH,
  maxHeight: DEFAULT_MAX_HEIGHT,
  offsetTop: DEFAULT_OFFSET_TOP,
  horizontalAlign: DEFAULT_HORIZONTAL_ALIGN,
  fontSize: DEFAULT_FONT_SIZE,
  showTabs: DEFAULT_SHOW_TABS,
  showHistory: DEFAULT_SHOW_HISTORY,
  showBookmarks: DEFAULT_SHOW_BOOKMARKS,
  categoryPriority: [...DEFAULT_CATEGORY_PRIORITY],
  maxResultsPerCategory: DEFAULT_MAX_RESULTS_PER_CATEGORY,
  maxBookmarkSuggestions: DEFAULT_MAX_BOOKMARK_SUGGESTIONS,
  maxHistorySuggestions: DEFAULT_MAX_HISTORY_SUGGESTIONS,
  maxTabsResults: DEFAULT_MAX_TABS_RESULTS,
};

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
    if (settings.enabled !== undefined) {
      await rpc.setBoolPref(
        COMMAND_PALETTE_ENABLED_PREF,
        Boolean(settings.enabled),
      );
    }

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

    if (settings.maxBookmarkSuggestions !== undefined) {
      await rpc.setIntPref(
        COMMAND_PALETTE_MAX_BOOKMARK_SUGGESTIONS_PREF,
        clampInt(
          settings.maxBookmarkSuggestions,
          MAX_BOOKMARK_SUGGESTIONS_BOUNDS,
          DEFAULT_MAX_BOOKMARK_SUGGESTIONS,
        ),
      );
    }

    if (settings.maxHistorySuggestions !== undefined) {
      await rpc.setIntPref(
        COMMAND_PALETTE_MAX_HISTORY_SUGGESTIONS_PREF,
        clampInt(
          settings.maxHistorySuggestions,
          MAX_HISTORY_SUGGESTIONS_BOUNDS,
          DEFAULT_MAX_HISTORY_SUGGESTIONS,
        ),
      );
    }

    if (settings.maxTabsResults !== undefined) {
      await rpc.setIntPref(
        COMMAND_PALETTE_MAX_TABS_RESULTS_PREF,
        clampInt(
          settings.maxTabsResults,
          MAX_TABS_RESULTS_BOUNDS,
          DEFAULT_MAX_TABS_RESULTS,
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
    const maxBookmarkSuggestionsRaw = await rpc.getIntPref(
      COMMAND_PALETTE_MAX_BOOKMARK_SUGGESTIONS_PREF,
    );
    const maxHistorySuggestionsRaw = await rpc.getIntPref(
      COMMAND_PALETTE_MAX_HISTORY_SUGGESTIONS_PREF,
    );
    const maxTabsResultsRaw = await rpc.getIntPref(
      COMMAND_PALETTE_MAX_TABS_RESULTS_PREF,
    );

    return {
      enabled: enabled === null ? true : enabled,
      width: clampInt(width ?? DEFAULT_WIDTH, WIDTH_BOUNDS, DEFAULT_WIDTH),
      maxHeight: clampInt(maxHeight ?? DEFAULT_MAX_HEIGHT, MAX_HEIGHT_BOUNDS, DEFAULT_MAX_HEIGHT),
      offsetTop: clampInt(offsetTop ?? DEFAULT_OFFSET_TOP, OFFSET_TOP_BOUNDS, DEFAULT_OFFSET_TOP),
      horizontalAlign: horizontalAlign !== null &&
          (VALID_HORIZONTAL_ALIGNS as readonly string[]).includes(
            horizontalAlign,
          )
        ? (horizontalAlign as CommandPaletteFormData["horizontalAlign"])
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
      maxBookmarkSuggestions: clampInt(
        maxBookmarkSuggestionsRaw ?? DEFAULT_MAX_BOOKMARK_SUGGESTIONS,
        MAX_BOOKMARK_SUGGESTIONS_BOUNDS,
        DEFAULT_MAX_BOOKMARK_SUGGESTIONS,
      ),
      maxHistorySuggestions: clampInt(
        maxHistorySuggestionsRaw ?? DEFAULT_MAX_HISTORY_SUGGESTIONS,
        MAX_HISTORY_SUGGESTIONS_BOUNDS,
        DEFAULT_MAX_HISTORY_SUGGESTIONS,
      ),
      maxTabsResults: clampInt(
        maxTabsResultsRaw ?? DEFAULT_MAX_TABS_RESULTS,
        MAX_TABS_RESULTS_BOUNDS,
        DEFAULT_MAX_TABS_RESULTS,
      ),
    };
  } catch (error) {
    console.error("[command-palette] Failed to load settings:", error);
    return null;
  }
}

// ---------------------------------------------------------------------------
// @prefix shortcuts (floorp.commandPalette.shortcuts)
// ---------------------------------------------------------------------------
//
// A user-editable mapping from an `@prefix` (typed without the leading `@`)
// to an existing command id. The settings UI writes this pref; the live
// command palette reads it to resolve `@prefix` queries instantly.
// Pref shape: JSON string of `[{"prefix":"gh","commandId":"floorp-open-hub"}]`.
// Default: `"[]"` (empty array) when the pref is unset.

/** A single user-defined @prefix → command mapping. */
export interface CommandPaletteShortcut {
  prefix: string;
  commandId: string;
}

/**
 * Command catalog entry written by the chrome side and consumed read-only by
 * the settings UI to populate the command picker. Pref shape:
 * JSON string of `[{"id":"...","label":"...","category":"..."}]`.
 */
export interface SelectableCommand {
  id: string;
  label: string;
  category: string;
}

/** Type guard for a single shortcut object parsed from pref JSON. */
function isCommandPaletteShortcut(
  value: unknown,
): value is CommandPaletteShortcut {
  if (typeof value !== "object" || value === null) return false;
  const v = value as Record<string, unknown>;
  return typeof v.prefix === "string" && typeof v.commandId === "string";
}

/** Type guard for a single selectable-command object parsed from pref JSON. */
function isSelectableCommand(value: unknown): value is SelectableCommand {
  if (typeof value !== "object" || value === null) return false;
  const v = value as Record<string, unknown>;
  return typeof v.id === "string" && typeof v.label === "string" &&
    typeof v.category === "string";
}

/**
 * Parses a raw shortcuts pref string into a typed array.
 *
 * Falls back to an empty array on any malformed input (non-string, invalid
 * JSON, non-array, or any element failing the shape guard). A corrupted pref
 * therefore degrades gracefully — the UI simply shows no shortcuts rather
 * than throwing.
 */
export function parseShortcuts(
  raw: string | null,
): CommandPaletteShortcut[] {
  if (typeof raw !== "string" || raw.length === 0) {
    return [];
  }
  let parsed: unknown;
  try {
    parsed = JSON.parse(raw);
  } catch {
    return [];
  }
  if (!Array.isArray(parsed) || !parsed.every(isCommandPaletteShortcut)) {
    return [];
  }
  // Dedupe by prefix, retaining the first occurrence of each prefix so
  // ShortcutList keys stay unique and handleRemove cannot hit duplicates.
  const seen = new Set<string>();
  return parsed.filter((entry) => {
    if (seen.has(entry.prefix)) return false;
    seen.add(entry.prefix);
    return true;
  });
}

/**
 * Parses a raw selectableCommands pref string into a typed array.
 *
 * Falls back to an empty array on any malformed input — including the common
 * case where the chrome side has not yet populated the pref (e.g. before the
 * first browser restart). The UI must handle the empty case gracefully.
 */
export function parseSelectableCommands(
  raw: string | null,
): SelectableCommand[] {
  if (typeof raw !== "string" || raw.length === 0) {
    return [];
  }
  let parsed: unknown;
  try {
    parsed = JSON.parse(raw);
  } catch {
    return [];
  }
  if (!Array.isArray(parsed) || !parsed.every(isSelectableCommand)) {
    return [];
  }
  return parsed;
}

/**
 * Loads the user's @prefix shortcuts from the pref.
 * Always resolves (never rejects); returns an empty array on any failure.
 */
export async function loadShortcuts(): Promise<CommandPaletteShortcut[]> {
  try {
    const raw = await rpc.getStringPref(COMMAND_PALETTE_SHORTCUTS_PREF);
    const parsed = parseShortcuts(raw);
    // Migration: drop reserved prefixes (s, t, b, h) that may linger from
    // before they were reserved. They are invisible in the settings UI and
    // cannot be removed there, so cleaning them here keeps the pref consistent
    // with the reserved display. @s, @t, @b and @h are built-in and need no
    // pref entry.
    const cleaned = parsed.filter((s) => !isReservedShortcutPrefix(s.prefix));
    if (cleaned.length !== parsed.length) {
      // Best-effort; failures are logged by saveShortcuts itself.
      saveShortcuts(cleaned);
    }
    return cleaned;
  } catch (error) {
    console.error("[command-palette] Failed to load shortcuts:", error);
    return [];
  }
}

/**
 * Persists the given shortcuts array to the pref as a JSON string.
 * Logs (never throws) on failure so the UI can keep functioning.
 */
export async function saveShortcuts(
  shortcuts: CommandPaletteShortcut[],
): Promise<void> {
  try {
    await rpc.setStringPref(
      COMMAND_PALETTE_SHORTCUTS_PREF,
      JSON.stringify(shortcuts),
    );
  } catch (error) {
    console.error("[command-palette] Failed to save shortcuts:", error);
  }
}

/**
 * Loads the chrome-cached selectable command catalog from the pref (read-only).
 * Returns an empty array when the pref is unset (e.g. before first launch) or
 * malformed; the UI should prompt a restart in that case.
 */
export async function loadSelectableCommands(): Promise<SelectableCommand[]> {
  try {
    const raw = await rpc.getStringPref(
      COMMAND_PALETTE_SELECTABLE_COMMANDS_PREF,
    );
    return parseSelectableCommands(raw);
  } catch (error) {
    console.error(
      "[command-palette] Failed to load selectable commands:",
      error,
    );
    return [];
  }
}
