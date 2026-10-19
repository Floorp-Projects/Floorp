// SPDX-License-Identifier: MPL-2.0

import {
  type Accessor,
  createEffect,
  createSignal,
  onCleanup,
  type Setter,
} from "solid-js";
import { createRootHMR } from "@nora/solid-xul";
import {
  DEFAULT_CATEGORY_PRIORITY,
  parseCategoryPriority,
} from "./category-priority.ts";
import type {
  CommandPaletteShortcut,
  SelectableCommand,
} from "./types.ts";

export const COMMAND_PALETTE_ENABLED_PREF = "floorp.commandPalette.enabled";
export const COMMAND_PALETTE_RECENT_PREF = "floorp.commandPalette.recentCommands";
export const COMMAND_PALETTE_FREQUENCY_PREF = "floorp.commandPalette.commandFrequency";
export const COMMAND_PALETTE_WIDTH_PREF = "floorp.commandPalette.width";
export const COMMAND_PALETTE_MAX_HEIGHT_PREF = "floorp.commandPalette.maxHeight";
export const COMMAND_PALETTE_OFFSET_TOP_PREF = "floorp.commandPalette.offsetTop";
export const COMMAND_PALETTE_HORIZONTAL_ALIGN_PREF = "floorp.commandPalette.horizontalAlign";
export const COMMAND_PALETTE_FONT_SIZE_PREF = "floorp.commandPalette.fontSize";
export const COMMAND_PALETTE_SHOW_TABS_PREF = "floorp.commandPalette.showTabs";
export const COMMAND_PALETTE_SHOW_HISTORY_PREF = "floorp.commandPalette.showHistory";
export const COMMAND_PALETTE_SHOW_BOOKMARKS_PREF = "floorp.commandPalette.showBookmarks";
export const COMMAND_PALETTE_CATEGORY_PRIORITY_PREF = "floorp.commandPalette.categoryPriority";
export const COMMAND_PALETTE_MAX_RESULTS_PER_CATEGORY_PREF =
  "floorp.commandPalette.maxResultsPerCategory";
export const COMMAND_PALETTE_MAX_BOOKMARK_SUGGESTIONS_PREF = "floorp.commandPalette.maxBookmarkSuggestions";
export const COMMAND_PALETTE_MAX_HISTORY_SUGGESTIONS_PREF = "floorp.commandPalette.maxHistorySuggestions";
export const COMMAND_PALETTE_MAX_TABS_RESULTS_PREF = "floorp.commandPalette.maxTabsResults";
export const COMMAND_PALETTE_SHORTCUTS_PREF = "floorp.commandPalette.shortcuts";
export const COMMAND_PALETTE_SELECTABLE_COMMANDS_PREF =
  "floorp.commandPalette.selectableCommands";

export type CommandPaletteHorizontalAlign = "center" | "left" | "right";

export interface CommandPaletteConfig {
  enabled: boolean;
  recentCommands: string[];
  maxRecentCommands: number;
  width: number;
  maxHeight: number;
  offsetTop: number;
  horizontalAlign: CommandPaletteHorizontalAlign;
  fontSize: number;
  showTabs: boolean;
  showHistory: boolean;
  showBookmarks: boolean;
  categoryPriority: string[];
  maxResultsPerCategory: number;
  maxBookmarkSuggestions: number;
  maxHistorySuggestions: number;
  maxTabsResults: number;
  shortcuts: CommandPaletteShortcut[];
}

// KEEP IN SYNC with:
// - browser-features/pages-settings/src/app/command-palette/dataManager.ts
// - the Seekbar min/max props in browser-features/pages-settings/src/app/command-palette/components/ResultLimitSettings.tsx
export const DEFAULT_MAX_RESULTS_PER_CATEGORY = 5;
const DEFAULT_MAX_BOOKMARK_SUGGESTIONS = 5;
const DEFAULT_MAX_HISTORY_SUGGESTIONS = 5;
const DEFAULT_MAX_TABS_RESULTS = 5;

export const defaultConfig: CommandPaletteConfig = {
  enabled: true,
  recentCommands: [],
  maxRecentCommands: 10,
  width: 560,
  maxHeight: 400,
  offsetTop: 20,
  horizontalAlign: "center",
  fontSize: 14,
  showTabs: true,
  showHistory: true,
  showBookmarks: true,
  categoryPriority: [...DEFAULT_CATEGORY_PRIORITY],
  maxResultsPerCategory: DEFAULT_MAX_RESULTS_PER_CATEGORY,
  maxBookmarkSuggestions: DEFAULT_MAX_BOOKMARK_SUGGESTIONS,
  maxHistorySuggestions: DEFAULT_MAX_HISTORY_SUGGESTIONS,
  maxTabsResults: DEFAULT_MAX_TABS_RESULTS,
  // @s, @t, @b and @h are built-in reserved prefixes handled directly by
  // the controller — no pref entry needed. The shortcuts pref is for
  // user-defined @prefix aliases only (default: empty).
  shortcuts: [],
};

/**
 * Prefixes reserved for built-in command palette behavior. These cannot be
 * used for user-defined @prefix shortcuts:
 * - "s" — @s is the built-in web search shortcut (floorp-search-web)
 * - "t" — @t is the built-in open-tabs search mode
 * - "b" — @b is the built-in bookmark search mode
 * - "h" — @h is the built-in history search mode
 *
 * KEEP IN SYNC with:
 * - browser-features/pages-settings/src/app/command-palette/dataManager.ts
 */
export const RESERVED_SHORTCUT_PREFIXES: readonly string[] = [
  "s",
  "t",
  "b",
  "h",
];

// Bounds for the customizable size/position prefs.
export const WIDTH_BOUNDS = { min: 400, max: 1000 } as const;
export const MAX_HEIGHT_BOUNDS = { min: 300, max: 800 } as const;
export const OFFSET_TOP_BOUNDS = { min: 0, max: 60 } as const;
export const FONT_SIZE_BOUNDS = { min: 11, max: 22 } as const;
// KEEP IN SYNC with:
// - browser-features/pages-settings/src/app/command-palette/dataManager.ts
// - the Seekbar min/max props in browser-features/pages-settings/src/app/command-palette/components/ResultLimitSettings.tsx
export const MAX_RESULTS_PER_CATEGORY_BOUNDS = { min: 1, max: 20 } as const;
// KEEP IN SYNC with:
// - browser-features/pages-settings/src/app/command-palette/dataManager.ts
// - the Seekbar min/max props in browser-features/pages-settings/src/app/command-palette/components/ResultLimitSettings.tsx
export const MAX_BOOKMARK_SUGGESTIONS_BOUNDS = { min: 1, max: 20 } as const;
// KEEP IN SYNC with:
// - browser-features/pages-settings/src/app/command-palette/dataManager.ts
// - the Seekbar min/max props in browser-features/pages-settings/src/app/command-palette/components/ResultLimitSettings.tsx
export const MAX_HISTORY_SUGGESTIONS_BOUNDS = { min: 1, max: 20 } as const;
// KEEP IN SYNC with:
// - browser-features/pages-settings/src/app/command-palette/dataManager.ts
// - the Seekbar min/max props in browser-features/pages-settings/src/app/command-palette/components/ResultLimitSettings.tsx
export const MAX_TABS_RESULTS_BOUNDS = { min: 1, max: 20 } as const;
export const HORIZONTAL_ALIGN_VALUES: readonly CommandPaletteHorizontalAlign[] = [
  "center",
  "left",
  "right",
] as const;

export function clampInt(
  value: number,
  min: number,
  max: number,
  fallback: number,
): number {
  if (!Number.isFinite(value)) return fallback;
  const i = Math.round(value);
  return Math.min(max, Math.max(min, i));
}

export function normalizeHorizontalAlign(
  value: string,
): CommandPaletteHorizontalAlign {
  return HORIZONTAL_ALIGN_VALUES.includes(value as CommandPaletteHorizontalAlign)
    ? (value as CommandPaletteHorizontalAlign)
    : defaultConfig.horizontalAlign;
}

const parseRecentCommands = (jsonStr: string): string[] => {
  try {
    const parsed = JSON.parse(jsonStr);
    if (
      Array.isArray(parsed) &&
      parsed.every((el): el is string => typeof el === "string")
    ) {
      return parsed;
    }
  } catch {
    // ignore
  }
  return [];
};

function createEnabled(): [Accessor<boolean>, Setter<boolean>] {
  const [enabled, setEnabled] = createSignal(
    Services.prefs.getBoolPref(
      COMMAND_PALETTE_ENABLED_PREF,
      defaultConfig.enabled,
    ),
  );

  createEffect(() => {
    try {
      Services.prefs.setBoolPref(COMMAND_PALETTE_ENABLED_PREF, enabled());
    } catch (e) {
      console.error("[command-palette] Failed to persist enabled pref", e);
    }
  });

  const enabledObserver = () => {
    setEnabled(
      Services.prefs.getBoolPref(
        COMMAND_PALETTE_ENABLED_PREF,
        defaultConfig.enabled,
      ),
    );
  };

  Services.prefs.addObserver(COMMAND_PALETTE_ENABLED_PREF, enabledObserver);
  onCleanup(() => {
    Services.prefs.removeObserver(
      COMMAND_PALETTE_ENABLED_PREF,
      enabledObserver,
    );
  });

  return [enabled, setEnabled];
}

function createShowTabs(): [Accessor<boolean>, Setter<boolean>] {
  const [showTabs, setShowTabs] = createSignal(
    Services.prefs.getBoolPref(
      COMMAND_PALETTE_SHOW_TABS_PREF,
      defaultConfig.showTabs,
    ),
  );

  createEffect(() => {
    try {
      Services.prefs.setBoolPref(COMMAND_PALETTE_SHOW_TABS_PREF, showTabs());
    } catch (e) {
      console.error("[command-palette] Failed to persist showTabs pref", e);
    }
  });

  const showTabsObserver = () => {
    setShowTabs(
      Services.prefs.getBoolPref(
        COMMAND_PALETTE_SHOW_TABS_PREF,
        defaultConfig.showTabs,
      ),
    );
  };

  Services.prefs.addObserver(COMMAND_PALETTE_SHOW_TABS_PREF, showTabsObserver);
  onCleanup(() => {
    Services.prefs.removeObserver(
      COMMAND_PALETTE_SHOW_TABS_PREF,
      showTabsObserver,
    );
  });

  return [showTabs, setShowTabs];
}

function createShowHistory(): [Accessor<boolean>, Setter<boolean>] {
  const [showHistory, setShowHistory] = createSignal(
    Services.prefs.getBoolPref(
      COMMAND_PALETTE_SHOW_HISTORY_PREF,
      defaultConfig.showHistory,
    ),
  );

  createEffect(() => {
    try {
      Services.prefs.setBoolPref(COMMAND_PALETTE_SHOW_HISTORY_PREF, showHistory());
    } catch (e) {
      console.error("[command-palette] Failed to persist showHistory pref", e);
    }
  });

  const showHistoryObserver = () => {
    setShowHistory(
      Services.prefs.getBoolPref(
        COMMAND_PALETTE_SHOW_HISTORY_PREF,
        defaultConfig.showHistory,
      ),
    );
  };

  Services.prefs.addObserver(
    COMMAND_PALETTE_SHOW_HISTORY_PREF,
    showHistoryObserver,
  );
  onCleanup(() => {
    Services.prefs.removeObserver(
      COMMAND_PALETTE_SHOW_HISTORY_PREF,
      showHistoryObserver,
    );
  });

  return [showHistory, setShowHistory];
}

function createShowBookmarks(): [Accessor<boolean>, Setter<boolean>] {
  const [showBookmarks, setShowBookmarks] = createSignal(
    Services.prefs.getBoolPref(
      COMMAND_PALETTE_SHOW_BOOKMARKS_PREF,
      defaultConfig.showBookmarks,
    ),
  );

  createEffect(() => {
    try {
      Services.prefs.setBoolPref(
        COMMAND_PALETTE_SHOW_BOOKMARKS_PREF,
        showBookmarks(),
      );
    } catch (e) {
      console.error(
        "[command-palette] Failed to persist showBookmarks pref",
        e,
      );
    }
  });

  const showBookmarksObserver = () => {
    setShowBookmarks(
      Services.prefs.getBoolPref(
        COMMAND_PALETTE_SHOW_BOOKMARKS_PREF,
        defaultConfig.showBookmarks,
      ),
    );
  };

  Services.prefs.addObserver(
    COMMAND_PALETTE_SHOW_BOOKMARKS_PREF,
    showBookmarksObserver,
  );
  onCleanup(() => {
    Services.prefs.removeObserver(
      COMMAND_PALETTE_SHOW_BOOKMARKS_PREF,
      showBookmarksObserver,
    );
  });

  return [showBookmarks, setShowBookmarks];
}

function createRecentCommands(): [
  Accessor<string[]>,
  Setter<string[]>,
] {
  const [recent, setRecent] = createSignal(
    parseRecentCommands(
      Services.prefs.getStringPref(
        COMMAND_PALETTE_RECENT_PREF,
        "[]",
      ),
    ),
  );

  createEffect(() => {
    try {
      Services.prefs.setStringPref(
        COMMAND_PALETTE_RECENT_PREF,
        JSON.stringify(recent()),
      );
    } catch (e) {
      console.error("[command-palette] Failed to persist recent commands", e);
    }
  });

  const recentObserver = () => {
    setRecent(
      parseRecentCommands(
        Services.prefs.getStringPref(
          COMMAND_PALETTE_RECENT_PREF,
          "[]",
        ),
      ),
    );
  };

  Services.prefs.addObserver(COMMAND_PALETTE_RECENT_PREF, recentObserver);
  onCleanup(() => {
    Services.prefs.removeObserver(
      COMMAND_PALETTE_RECENT_PREF,
      recentObserver,
    );
  });

  return [recent, setRecent];
}

function createCategoryPriority(): [
  Accessor<string[]>,
  Setter<string[]>,
] {
  const defaultJson = JSON.stringify(DEFAULT_CATEGORY_PRIORITY);
  const [priority, setPriority] = createSignal(
    parseCategoryPriority(
      Services.prefs.getStringPref(
        COMMAND_PALETTE_CATEGORY_PRIORITY_PREF,
        defaultJson,
      ),
    ),
  );

  createEffect(() => {
    try {
      Services.prefs.setStringPref(
        COMMAND_PALETTE_CATEGORY_PRIORITY_PREF,
        JSON.stringify(priority()),
      );
    } catch (e) {
      console.error(
        "[command-palette] Failed to persist categoryPriority pref",
        e,
      );
    }
  });

  const observer = () => {
    setPriority(
      parseCategoryPriority(
        Services.prefs.getStringPref(
          COMMAND_PALETTE_CATEGORY_PRIORITY_PREF,
          defaultJson,
        ),
      ),
    );
  };

  Services.prefs.addObserver(
    COMMAND_PALETTE_CATEGORY_PRIORITY_PREF,
    observer,
  );
  onCleanup(() => {
    Services.prefs.removeObserver(
      COMMAND_PALETTE_CATEGORY_PRIORITY_PREF,
      observer,
    );
  });

  return [priority, setPriority];
}

export const [_enabled, _setEnabled] = createRootHMR(
  createEnabled,
  import.meta.hot,
);
export const [_recentCommands, _setRecentCommands] = createRootHMR(
  createRecentCommands,
  import.meta.hot,
);
export const [_categoryPriority, _setCategoryPriority] = createRootHMR(
  createCategoryPriority,
  import.meta.hot,
);

export const isEnabled = () => _enabled();
export const setEnabled = (value: boolean) => _setEnabled(value);
export const getRecentCommands = () => _recentCommands();
/**
 * Returns the current category-priority list.
 *
 * NOTE: Returns the signal's INTERNAL array reference (the `readonly string[]`
 * annotation is compile-time only). Callers MUST NOT mutate the returned
 * array — doing so would change internal state without triggering reactivity.
 * `setCategoryPriority` copies on input (`[...value]`); this getter does not
 * copy on output for perf (the list is read inside sort comparators). This
 * mirrors the pre-existing `getRecentCommands` pattern.
 */
export const getCategoryPriority = (): readonly string[] => _categoryPriority();
export const setCategoryPriority = (value: string[]): void => {
  _setCategoryPriority([...value]);
};

export function addRecentCommand(id: string) {
  const current = _recentCommands().filter((c) => c !== id);
  current.unshift(id);
  _setRecentCommands(current.slice(0, defaultConfig.maxRecentCommands));
}

const parseFrequency = (jsonStr: string): Record<string, number> => {
  try {
    const parsed = JSON.parse(jsonStr);
    if (typeof parsed === "object" && parsed !== null) {
      return parsed as Record<string, number>;
    }
  } catch {
    // ignore
  }
  return {};
};

function createFrequency(): [
  Accessor<Record<string, number>>,
  Setter<Record<string, number>>,
] {
  const [freq, setFreq] = createSignal(
    parseFrequency(
      Services.prefs.getStringPref(COMMAND_PALETTE_FREQUENCY_PREF, "{}"),
    ),
  );

  createEffect(() => {
    try {
      Services.prefs.setStringPref(
        COMMAND_PALETTE_FREQUENCY_PREF,
        JSON.stringify(freq()),
      );
    } catch (e) {
      console.error("[command-palette] Failed to persist frequency", e);
    }
  });

  const freqObserver = () => {
    setFreq(
      parseFrequency(
        Services.prefs.getStringPref(COMMAND_PALETTE_FREQUENCY_PREF, "{}"),
      ),
    );
  };

  Services.prefs.addObserver(COMMAND_PALETTE_FREQUENCY_PREF, freqObserver);
  onCleanup(() => {
    Services.prefs.removeObserver(COMMAND_PALETTE_FREQUENCY_PREF, freqObserver);
  });

  return [freq, setFreq];
}

export const [_frequency, _setFrequency] = createRootHMR(
  createFrequency,
  import.meta.hot,
);

export const getFrequencies = () => _frequency();

export function incrementFrequency(id: string) {
  const current = { ..._frequency() };
  current[id] = (current[id] ?? 0) + 1;
  _setFrequency(current);
}

// --- @prefix shortcuts & selectable command cache ---
//
// `shortcuts` is user-editable (written by the settings page): an array of
// `{ prefix, commandId }` aliases. `selectableCommands` is a chrome-side cache
// of the command catalogue (id/label/category) that the settings page reads to
// populate its command picker. Both are JSON strings persisted in prefs.

export function parseShortcuts(
  jsonStr: string,
  defaultVal: CommandPaletteShortcut[],
): CommandPaletteShortcut[] {
  try {
    const parsed: unknown = JSON.parse(jsonStr);
    if (
      Array.isArray(parsed) &&
      parsed.every(
        (el): el is CommandPaletteShortcut =>
          typeof el === "object" &&
          el !== null &&
          typeof (el as { prefix?: unknown }).prefix === "string" &&
          typeof (el as { commandId?: unknown }).commandId === "string",
      )
    ) {
      // Dedupe by prefix, retaining the first occurrence of each prefix so
      // persisted data can never carry duplicate prefixes.
      const seen = new Set<string>();
      return parsed.filter((el) => {
        if (seen.has(el.prefix)) return false;
        seen.add(el.prefix);
        return true;
      });
    }
  } catch {
    // ignore — fall through to default
  }
  // Copy so callers cannot mutate (and corrupt) the shared default.
  return [...defaultVal];
}

export function parseSelectableCommands(
  jsonStr: string,
): SelectableCommand[] {
  try {
    const parsed: unknown = JSON.parse(jsonStr);
    if (
      Array.isArray(parsed) &&
      parsed.every(
        (el): el is SelectableCommand =>
          typeof el === "object" &&
          el !== null &&
          typeof (el as { id?: unknown }).id === "string" &&
          typeof (el as { label?: unknown }).label === "string" &&
          typeof (el as { category?: unknown }).category === "string",
      )
    ) {
      return parsed;
    }
  } catch {
    // ignore — fall through to default
  }
  return [];
}

function createShortcuts(): [
  Accessor<CommandPaletteShortcut[]>,
  Setter<CommandPaletteShortcut[]>,
] {
  const [shortcuts, setShortcuts] = createSignal(
    parseShortcuts(
      Services.prefs.getStringPref(
        COMMAND_PALETTE_SHORTCUTS_PREF,
        JSON.stringify(defaultConfig.shortcuts),
      ),
      defaultConfig.shortcuts,
    ),
  );

  createEffect(() => {
    try {
      Services.prefs.setStringPref(
        COMMAND_PALETTE_SHORTCUTS_PREF,
        JSON.stringify(shortcuts()),
      );
    } catch (e) {
      console.error("[command-palette] Failed to persist shortcuts pref", e);
    }
  });

  const shortcutsObserver = () => {
    setShortcuts(
      parseShortcuts(
        Services.prefs.getStringPref(
          COMMAND_PALETTE_SHORTCUTS_PREF,
          JSON.stringify(defaultConfig.shortcuts),
        ),
        defaultConfig.shortcuts,
      ),
    );
  };

  Services.prefs.addObserver(COMMAND_PALETTE_SHORTCUTS_PREF, shortcutsObserver);
  onCleanup(() => {
    Services.prefs.removeObserver(
      COMMAND_PALETTE_SHORTCUTS_PREF,
      shortcutsObserver,
    );
  });

  return [shortcuts, setShortcuts];
}

function createSelectableCommands(): [
  Accessor<SelectableCommand[]>,
  Setter<SelectableCommand[]>,
] {
  const [selectable, setSelectable] = createSignal(
    parseSelectableCommands(
      Services.prefs.getStringPref(
        COMMAND_PALETTE_SELECTABLE_COMMANDS_PREF,
        "[]",
      ),
    ),
  );

  createEffect(() => {
    try {
      Services.prefs.setStringPref(
        COMMAND_PALETTE_SELECTABLE_COMMANDS_PREF,
        JSON.stringify(selectable()),
      );
    } catch (e) {
      console.error(
        "[command-palette] Failed to persist selectableCommands pref",
        e,
      );
    }
  });

  const selectableObserver = () => {
    setSelectable(
      parseSelectableCommands(
        Services.prefs.getStringPref(
          COMMAND_PALETTE_SELECTABLE_COMMANDS_PREF,
          "[]",
        ),
      ),
    );
  };

  Services.prefs.addObserver(
    COMMAND_PALETTE_SELECTABLE_COMMANDS_PREF,
    selectableObserver,
  );
  onCleanup(() => {
    Services.prefs.removeObserver(
      COMMAND_PALETTE_SELECTABLE_COMMANDS_PREF,
      selectableObserver,
    );
  });

  return [selectable, setSelectable];
}

export const [_shortcuts, _setShortcuts] = createRootHMR(
  createShortcuts,
  import.meta.hot,
);
export const [_selectableCommands, _setSelectableCommands] = createRootHMR(
  createSelectableCommands,
  import.meta.hot,
);

/**
 * Returns the current user-defined @prefix shortcuts.
 *
 * NOTE: Returns the signal's INTERNAL array reference (the `readonly` annotation
 * is compile-time only). Callers MUST NOT mutate the returned array — doing so
 * would change internal state without triggering reactivity. Mirrors the
 * `getRecentCommands` / `getCategoryPriority` pattern.
 */
export const getShortcuts = (): readonly CommandPaletteShortcut[] =>
  _shortcuts();
export const setShortcuts = (value: CommandPaletteShortcut[]): void => {
  _setShortcuts([...value]);
};
export const getSelectableCommands = (): readonly SelectableCommand[] =>
  _selectableCommands();
export const setSelectableCommands = (value: SelectableCommand[]): void => {
  _setSelectableCommands([...value]);
};

/**
 * Shared implementation for the integer pref factories below. Creates a
 * signal seeded from the pref, persists the clamped signal value via an
 * effect, and mirrors external pref changes back into the signal via an
 * observer. The persistence error label is derived from the last segment
 * of the pref name, matching the messages of the factories it replaces.
 */
function createClampedIntPref(
  prefName: string,
  bounds: { readonly min: number; readonly max: number },
  fallback: number,
): [Accessor<number>, Setter<number>] {
  const [value, setValue] = createSignal(
    clampInt(
      Services.prefs.getIntPref(prefName, fallback),
      bounds.min,
      bounds.max,
      fallback,
    ),
  );

  createEffect(() => {
    try {
      Services.prefs.setIntPref(
        prefName,
        clampInt(value(), bounds.min, bounds.max, fallback),
      );
    } catch (e) {
      const label = prefName.split(".").pop() ?? prefName;
      console.error(`[command-palette] Failed to persist ${label} pref`, e);
    }
  });

  const prefObserver = () => {
    setValue(
      clampInt(
        Services.prefs.getIntPref(prefName, fallback),
        bounds.min,
        bounds.max,
        fallback,
      ),
    );
  };

  Services.prefs.addObserver(prefName, prefObserver);
  onCleanup(() => {
    Services.prefs.removeObserver(prefName, prefObserver);
  });

  return [value, setValue];
}

function createWidth(): [Accessor<number>, Setter<number>] {
  return createClampedIntPref(
    COMMAND_PALETTE_WIDTH_PREF,
    WIDTH_BOUNDS,
    defaultConfig.width,
  );
}

function createMaxHeight(): [Accessor<number>, Setter<number>] {
  return createClampedIntPref(
    COMMAND_PALETTE_MAX_HEIGHT_PREF,
    MAX_HEIGHT_BOUNDS,
    defaultConfig.maxHeight,
  );
}

function createOffsetTop(): [Accessor<number>, Setter<number>] {
  return createClampedIntPref(
    COMMAND_PALETTE_OFFSET_TOP_PREF,
    OFFSET_TOP_BOUNDS,
    defaultConfig.offsetTop,
  );
}

function createHorizontalAlign(): [
  Accessor<CommandPaletteHorizontalAlign>,
  Setter<CommandPaletteHorizontalAlign>,
] {
  const [align, setAlign] = createSignal(
    normalizeHorizontalAlign(
      Services.prefs.getStringPref(
        COMMAND_PALETTE_HORIZONTAL_ALIGN_PREF,
        defaultConfig.horizontalAlign,
      ),
    ),
  );

  createEffect(() => {
    try {
      Services.prefs.setStringPref(
        COMMAND_PALETTE_HORIZONTAL_ALIGN_PREF,
        normalizeHorizontalAlign(align()),
      );
    } catch (e) {
      console.error(
        "[command-palette] Failed to persist horizontalAlign pref",
        e,
      );
    }
  });

  const alignObserver = () => {
    setAlign(
      normalizeHorizontalAlign(
        Services.prefs.getStringPref(
          COMMAND_PALETTE_HORIZONTAL_ALIGN_PREF,
          defaultConfig.horizontalAlign,
        ),
      ),
    );
  };

  Services.prefs.addObserver(
    COMMAND_PALETTE_HORIZONTAL_ALIGN_PREF,
    alignObserver,
  );
  onCleanup(() => {
    Services.prefs.removeObserver(
      COMMAND_PALETTE_HORIZONTAL_ALIGN_PREF,
      alignObserver,
    );
  });

  return [align, setAlign];
}

function createFontSize(): [Accessor<number>, Setter<number>] {
  return createClampedIntPref(
    COMMAND_PALETTE_FONT_SIZE_PREF,
    FONT_SIZE_BOUNDS,
    defaultConfig.fontSize,
  );
}

function createMaxResultsPerCategory(): [Accessor<number>, Setter<number>] {
  return createClampedIntPref(
    COMMAND_PALETTE_MAX_RESULTS_PER_CATEGORY_PREF,
    MAX_RESULTS_PER_CATEGORY_BOUNDS,
    defaultConfig.maxResultsPerCategory,
  );
}

function createMaxBookmarkSuggestions(): [Accessor<number>, Setter<number>] {
  return createClampedIntPref(
    COMMAND_PALETTE_MAX_BOOKMARK_SUGGESTIONS_PREF,
    MAX_BOOKMARK_SUGGESTIONS_BOUNDS,
    defaultConfig.maxBookmarkSuggestions,
  );
}

function createMaxHistorySuggestions(): [Accessor<number>, Setter<number>] {
  return createClampedIntPref(
    COMMAND_PALETTE_MAX_HISTORY_SUGGESTIONS_PREF,
    MAX_HISTORY_SUGGESTIONS_BOUNDS,
    defaultConfig.maxHistorySuggestions,
  );
}

function createMaxTabsResults(): [Accessor<number>, Setter<number>] {
  return createClampedIntPref(
    COMMAND_PALETTE_MAX_TABS_RESULTS_PREF,
    MAX_TABS_RESULTS_BOUNDS,
    defaultConfig.maxTabsResults,
  );
}

export const [_width, _setWidth] = createRootHMR(
  createWidth,
  import.meta.hot,
);
export const [_maxHeight, _setMaxHeight] = createRootHMR(
  createMaxHeight,
  import.meta.hot,
);
export const [_offsetTop, _setOffsetTop] = createRootHMR(
  createOffsetTop,
  import.meta.hot,
);
export const [_horizontalAlign, _setHorizontalAlign] = createRootHMR(
  createHorizontalAlign,
  import.meta.hot,
);
export const [_fontSize, _setFontSize] = createRootHMR(
  createFontSize,
  import.meta.hot,
);
export const [_maxResultsPerCategory, _setMaxResultsPerCategory] = createRootHMR(
  createMaxResultsPerCategory,
  import.meta.hot,
);
export const [_maxBookmarkSuggestions, _setMaxBookmarkSuggestions] = createRootHMR(
  createMaxBookmarkSuggestions,
  import.meta.hot,
);
export const [_maxHistorySuggestions, _setMaxHistorySuggestions] = createRootHMR(
  createMaxHistorySuggestions,
  import.meta.hot,
);
export const [_maxTabsResults, _setMaxTabsResults] = createRootHMR(
  createMaxTabsResults,
  import.meta.hot,
);
export const [_showTabs, _setShowTabs] = createRootHMR(
  createShowTabs,
  import.meta.hot,
);
export const [_showHistory, _setShowHistory] = createRootHMR(
  createShowHistory,
  import.meta.hot,
);
export const [_showBookmarks, _setShowBookmarks] = createRootHMR(
  createShowBookmarks,
  import.meta.hot,
);

export const getWidth = () => _width();
export const getMaxHeight = () => _maxHeight();
export const getOffsetTop = () => _offsetTop();
export const getHorizontalAlign = () => _horizontalAlign();
export const getFontSize = () => _fontSize();
export const getShowTabs = () => _showTabs();
export const getShowHistory = () => _showHistory();
export const getShowBookmarks = () => _showBookmarks();
export const getMaxResultsPerCategory = (): number => _maxResultsPerCategory();
export const setMaxResultsPerCategory = (value: number): void => {
  _setMaxResultsPerCategory(
    clampInt(
      value,
      MAX_RESULTS_PER_CATEGORY_BOUNDS.min,
      MAX_RESULTS_PER_CATEGORY_BOUNDS.max,
      defaultConfig.maxResultsPerCategory,
    ),
  );
};
export const getMaxBookmarkSuggestions = (): number => _maxBookmarkSuggestions();
export const setMaxBookmarkSuggestions = (value: number): void => {
  _setMaxBookmarkSuggestions(
    clampInt(
      value,
      MAX_BOOKMARK_SUGGESTIONS_BOUNDS.min,
      MAX_BOOKMARK_SUGGESTIONS_BOUNDS.max,
      defaultConfig.maxBookmarkSuggestions,
    ),
  );
};
export const getMaxHistorySuggestions = (): number => _maxHistorySuggestions();
export const setMaxHistorySuggestions = (value: number): void => {
  _setMaxHistorySuggestions(
    clampInt(
      value,
      MAX_HISTORY_SUGGESTIONS_BOUNDS.min,
      MAX_HISTORY_SUGGESTIONS_BOUNDS.max,
      defaultConfig.maxHistorySuggestions,
    ),
  );
};
export const getMaxTabsResults = (): number => _maxTabsResults();
export const setMaxTabsResults = (value: number): void => {
  _setMaxTabsResults(
    clampInt(
      value,
      MAX_TABS_RESULTS_BOUNDS.min,
      MAX_TABS_RESULTS_BOUNDS.max,
      defaultConfig.maxTabsResults,
    ),
  );
};
