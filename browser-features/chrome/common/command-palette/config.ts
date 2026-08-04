// SPDX-License-Identifier: MPL-2.0

import {
  type Accessor,
  createEffect,
  createSignal,
  onCleanup,
  type Setter,
} from "solid-js";
import { createRootHMR } from "@nora/solid-xul";

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
}

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
};

// Bounds for the customizable size/position prefs.
export const WIDTH_BOUNDS = { min: 400, max: 1000 } as const;
export const MAX_HEIGHT_BOUNDS = { min: 300, max: 800 } as const;
export const OFFSET_TOP_BOUNDS = { min: 0, max: 60 } as const;
export const FONT_SIZE_BOUNDS = { min: 11, max: 22 } as const;
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

export const [_enabled, _setEnabled] = createRootHMR(
  createEnabled,
  import.meta.hot,
);
export const [_recentCommands, _setRecentCommands] = createRootHMR(
  createRecentCommands,
  import.meta.hot,
);

export const isEnabled = () => _enabled();
export const setEnabled = (value: boolean) => _setEnabled(value);
export const getRecentCommands = () => _recentCommands();

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

function createWidth(): [Accessor<number>, Setter<number>] {
  const [width, setWidth] = createSignal(
    clampInt(
      Services.prefs.getIntPref(COMMAND_PALETTE_WIDTH_PREF, defaultConfig.width),
      WIDTH_BOUNDS.min,
      WIDTH_BOUNDS.max,
      defaultConfig.width,
    ),
  );

  createEffect(() => {
    try {
      Services.prefs.setIntPref(
        COMMAND_PALETTE_WIDTH_PREF,
        clampInt(
          width(),
          WIDTH_BOUNDS.min,
          WIDTH_BOUNDS.max,
          defaultConfig.width,
        ),
      );
    } catch (e) {
      console.error("[command-palette] Failed to persist width pref", e);
    }
  });

  const widthObserver = () => {
    setWidth(
      clampInt(
        Services.prefs.getIntPref(
          COMMAND_PALETTE_WIDTH_PREF,
          defaultConfig.width,
        ),
        WIDTH_BOUNDS.min,
        WIDTH_BOUNDS.max,
        defaultConfig.width,
      ),
    );
  };

  Services.prefs.addObserver(COMMAND_PALETTE_WIDTH_PREF, widthObserver);
  onCleanup(() => {
    Services.prefs.removeObserver(COMMAND_PALETTE_WIDTH_PREF, widthObserver);
  });

  return [width, setWidth];
}

function createMaxHeight(): [Accessor<number>, Setter<number>] {
  const [maxHeight, setMaxHeight] = createSignal(
    clampInt(
      Services.prefs.getIntPref(
        COMMAND_PALETTE_MAX_HEIGHT_PREF,
        defaultConfig.maxHeight,
      ),
      MAX_HEIGHT_BOUNDS.min,
      MAX_HEIGHT_BOUNDS.max,
      defaultConfig.maxHeight,
    ),
  );

  createEffect(() => {
    try {
      Services.prefs.setIntPref(
        COMMAND_PALETTE_MAX_HEIGHT_PREF,
        clampInt(
          maxHeight(),
          MAX_HEIGHT_BOUNDS.min,
          MAX_HEIGHT_BOUNDS.max,
          defaultConfig.maxHeight,
        ),
      );
    } catch (e) {
      console.error("[command-palette] Failed to persist maxHeight pref", e);
    }
  });

  const maxHeightObserver = () => {
    setMaxHeight(
      clampInt(
        Services.prefs.getIntPref(
          COMMAND_PALETTE_MAX_HEIGHT_PREF,
          defaultConfig.maxHeight,
        ),
        MAX_HEIGHT_BOUNDS.min,
        MAX_HEIGHT_BOUNDS.max,
        defaultConfig.maxHeight,
      ),
    );
  };

  Services.prefs.addObserver(COMMAND_PALETTE_MAX_HEIGHT_PREF, maxHeightObserver);
  onCleanup(() => {
    Services.prefs.removeObserver(
      COMMAND_PALETTE_MAX_HEIGHT_PREF,
      maxHeightObserver,
    );
  });

  return [maxHeight, setMaxHeight];
}

function createOffsetTop(): [Accessor<number>, Setter<number>] {
  const [offsetTop, setOffsetTop] = createSignal(
    clampInt(
      Services.prefs.getIntPref(
        COMMAND_PALETTE_OFFSET_TOP_PREF,
        defaultConfig.offsetTop,
      ),
      OFFSET_TOP_BOUNDS.min,
      OFFSET_TOP_BOUNDS.max,
      defaultConfig.offsetTop,
    ),
  );

  createEffect(() => {
    try {
      Services.prefs.setIntPref(
        COMMAND_PALETTE_OFFSET_TOP_PREF,
        clampInt(
          offsetTop(),
          OFFSET_TOP_BOUNDS.min,
          OFFSET_TOP_BOUNDS.max,
          defaultConfig.offsetTop,
        ),
      );
    } catch (e) {
      console.error("[command-palette] Failed to persist offsetTop pref", e);
    }
  });

  const offsetTopObserver = () => {
    setOffsetTop(
      clampInt(
        Services.prefs.getIntPref(
          COMMAND_PALETTE_OFFSET_TOP_PREF,
          defaultConfig.offsetTop,
        ),
        OFFSET_TOP_BOUNDS.min,
        OFFSET_TOP_BOUNDS.max,
        defaultConfig.offsetTop,
      ),
    );
  };

  Services.prefs.addObserver(COMMAND_PALETTE_OFFSET_TOP_PREF, offsetTopObserver);
  onCleanup(() => {
    Services.prefs.removeObserver(
      COMMAND_PALETTE_OFFSET_TOP_PREF,
      offsetTopObserver,
    );
  });

  return [offsetTop, setOffsetTop];
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
  const [fontSize, setFontSize] = createSignal(
    clampInt(
      Services.prefs.getIntPref(
        COMMAND_PALETTE_FONT_SIZE_PREF,
        defaultConfig.fontSize,
      ),
      FONT_SIZE_BOUNDS.min,
      FONT_SIZE_BOUNDS.max,
      defaultConfig.fontSize,
    ),
  );

  createEffect(() => {
    try {
      Services.prefs.setIntPref(
        COMMAND_PALETTE_FONT_SIZE_PREF,
        clampInt(
          fontSize(),
          FONT_SIZE_BOUNDS.min,
          FONT_SIZE_BOUNDS.max,
          defaultConfig.fontSize,
        ),
      );
    } catch (e) {
      console.error("[command-palette] Failed to persist fontSize pref", e);
    }
  });

  const fontSizeObserver = () => {
    setFontSize(
      clampInt(
        Services.prefs.getIntPref(
          COMMAND_PALETTE_FONT_SIZE_PREF,
          defaultConfig.fontSize,
        ),
        FONT_SIZE_BOUNDS.min,
        FONT_SIZE_BOUNDS.max,
        defaultConfig.fontSize,
      ),
    );
  };

  Services.prefs.addObserver(COMMAND_PALETTE_FONT_SIZE_PREF, fontSizeObserver);
  onCleanup(() => {
    Services.prefs.removeObserver(
      COMMAND_PALETTE_FONT_SIZE_PREF,
      fontSizeObserver,
    );
  });

  return [fontSize, setFontSize];
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
