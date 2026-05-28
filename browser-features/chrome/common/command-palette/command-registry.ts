// SPDX-License-Identifier: MPL-2.0

import {
  getAllGestureActions,
  getActionDescription,
  getActionDisplayName,
} from "../mouse-gesture/utils/gestures.ts";
import { fuzzySearch } from "./fuzzy.ts";
import i18next from "i18next";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import { getTabCommands, isTabCommand } from "./tab-provider.ts";
import { getConfig, shortcutToString } from "../keyboard-shortcut/config.ts";
import {
  bookmarkSwitcherCommand,
  closedTabSwitcherCommand,
  closedWindowSwitcherCommand,
  historySwitcherCommand,
  openUrlCommand,
  searchWebCommand,
  reopenInContainerCommand,
  tabSwitcherCommand,
} from "./multiInputCommand/index.ts";
import { searchHistory, isHistoryCommand } from "./history-provider.ts";
import { searchBookmarks, isBookmarkCommand } from "./bookmark-provider.ts";

export interface CommandStepChoice {
  /** Display label shown to the user */
  label: string;
  /** Actual value passed to the command function */
  value: string;
  /** Optional description shown below the label */
  description?: string;
}

export interface StepChoicesResult {
  choices: CommandStepChoice[];
  /** Index of the choice to select by default when the step loads. */
  defaultIndex?: number;
  /** Whether more choices can be loaded via loadMore. */
  hasMore?: boolean;
  /** Load more choices and return the newly loaded choices with updated hasMore flag. */
  loadMore?: () => Promise<{ choices: CommandStepChoice[]; hasMore: boolean }>;
}

export interface CommandStep {
  id: string;
  label: string;
  placeholder: string;
  /**
   * Validate user input for this step.
   * Return `true` to pass, or a string error message to display.
   * Omit to skip validation.
   */
  validate?: (input: string) => boolean | string;
  /**
   * Static predefined choices for this step.
   */
  choices?: CommandStepChoice[];
  /**
   * Dynamic choices loader. Called when entering this step.
   * If both `choices` and `choicesLoader` are defined, `choices` takes priority.
   * Use this for choices that need to be fetched asynchronously (e.g., search engines).
   */
  choicesLoader?: () => Promise<CommandStepChoice[] | StepChoicesResult>;
}

export interface PaletteCommand {
  id: string;
  label: string;
  description: string;
  category: string;
  keywords: string[];
  fn: (win: Window, args?: Record<string, string>) => void;
  /** If defined, the palette will prompt the user for each step before executing fn. */
  steps?: CommandStep[];
}

const ACTION_CATEGORY_MAP: Record<string, string> = {
  "gecko-back": "navigation",
  "gecko-forward": "navigation",
  "gecko-reload": "navigation",
  "gecko-force-reload": "navigation",
  "gecko-stop": "navigation",
  "gecko-open-home-page": "navigation",

  "gecko-close-tab": "tabs",
  "gecko-close-other-tabs": "tabs",
  "gecko-close-tabs-to-start": "tabs",
  "gecko-close-tabs-to-end": "tabs",
  "gecko-open-new-tab": "tabs",
  "gecko-duplicate-tab": "tabs",
  "gecko-reload-all-tabs": "tabs",
  "gecko-restore-last-tab": "tabs",
  "gecko-show-next-tab": "tabs",
  "gecko-show-previous-tab": "tabs",
  "gecko-show-previously-selected-tab": "tabs",
  "gecko-show-all-tabs-panel": "tabs",
  "gecko-mute-current-tab": "tabs",

  "gecko-zoom-in": "zoom",
  "gecko-zoom-out": "zoom",
  "gecko-reset-zoom": "zoom",

  "gecko-bookmark-this-page": "bookmarks",

  "gecko-save-page": "page",
  "gecko-print-page": "page",
  "gecko-show-source-of-page": "page",
  "gecko-show-page-info": "page",
  "gecko-open-screen-capture": "page",

  "gecko-search-in-this-page": "search",
  "gecko-show-next-search-result": "search",
  "gecko-show-previous-search-result": "search",
  "gecko-search-the-web": "search",

  "gecko-show-bookmark-sidebar": "sidebar",
  "gecko-show-history-sidebar": "sidebar",
  "gecko-show-synced-tabs-sidebar": "sidebar",
  "gecko-reverse-sidebar": "sidebar",
  "gecko-hide-sidebar": "sidebar",
  "gecko-toggle-sidebar": "sidebar",

  "gecko-scroll-up": "scrolling",
  "gecko-scroll-down": "scrolling",
  "gecko-scroll-right": "scrolling",
  "gecko-scroll-left": "scrolling",
  "gecko-scroll-to-top": "scrolling",
  "gecko-scroll-to-bottom": "scrolling",

  "gecko-restore-last-session": "history",
  "gecko-search-history": "history",
  "gecko-manage-history": "history",
  "gecko-forget-history": "history",
  "gecko-quick-forget-history": "history",

  "gecko-open-new-window": "window",
  "gecko-open-new-private-window": "window",
  "gecko-close-window": "window",
  "gecko-restore-last-window": "window",
  "gecko-quit-from-application": "window",

  "gecko-open-addons-manager": "tools",
  "gecko-open-migration-wizard": "tools",
  "gecko-enter-into-customize-mode": "tools",
  "gecko-open-task-manager": "tools",
  "gecko-send-with-mail": "tools",
  "gecko-enter-into-offline-mode": "tools",
  "gecko-open-sync-preferences": "tools",

  "gecko-open-downloads": "downloads",

  "gecko-workspace-next": "workspace",
  "gecko-workspace-previous": "workspace",

  "floorp-rest-mode": "floorp",
  "floorp-hide-user-interface": "floorp",
  "floorp-toggle-navigation-panel": "floorp",
  "floorp-toggle-zen-mode": "floorp",
  "floorp-toggle-command-palette": "floorp",
  "floorp-open-settings": "floorp",
  "floorp-open-hub": "floorp",

  "floorp-show-pip": "media",
  "floorp-toggle-share-mode": "floorp",
  "floorp-copy-page-url-as-markdown": "page",
};

const ACTION_KEYWORDS: Record<string, string[]> = {
  "gecko-back": ["back", "previous"],
  "gecko-forward": ["forward", "next"],
  "gecko-reload": ["refresh", "reload"],
  "gecko-force-reload": ["hard refresh", "hard reload", "skip cache"],
  "gecko-close-tab": ["close", "remove tab"],
  "gecko-close-other-tabs": [
    "close other",
    "close all other tabs",
    "close others",
  ],
  "gecko-close-tabs-to-start": [
    "close",
    "up",
    "close left",
    "close above",
    "close tabs to left",
    "close previous tabs",
  ],
  "gecko-close-tabs-to-end": [
    "close",
    "down",
    "close right",
    "close below",
    "close tabs to right",
    "close following tabs",
  ],
  "gecko-open-new-tab": ["new", "open tab", "create tab"],
  "gecko-duplicate-tab": ["clone", "copy tab"],
  "gecko-show-next-tab": ["next tab", "switch right"],
  "gecko-show-previous-tab": ["previous tab", "switch left"],
  "gecko-show-all-tabs-panel": ["list tabs", "tab list"],
  "gecko-mute-current-tab": ["mute", "silence", "audio"],
  "gecko-bookmark-this-page": ["bookmark", "favorite", "star"],
  "gecko-save-page": ["save", "download page"],
  "gecko-print-page": ["print"],
  "gecko-show-source-of-page": ["source", "view source", "html"],
  "gecko-show-page-info": ["page info", "info"],
  "gecko-open-screen-capture": ["screenshot", "capture", "screen"],
  "gecko-search-in-this-page": ["find", "search page"],
  "gecko-search-the-web": ["web search", "search"],
  "gecko-toggle-sidebar": ["sidebar", "panel"],
  "gecko-scroll-to-top": ["top", "beginning"],
  "gecko-scroll-to-bottom": ["bottom", "end"],
  "gecko-open-addons-manager": ["addons", "extensions", "plugins"],
  "gecko-open-task-manager": ["task manager", "processes"],
  "gecko-forget-history": ["clear history", "delete history"],
  "gecko-restore-last-session": ["restore session", "recover"],
  "gecko-open-downloads": ["downloads"],
  "gecko-workspace-next": ["next workspace"],
  "gecko-workspace-previous": ["previous workspace"],
  "floorp-rest-mode": ["rest", "break"],
  "floorp-hide-user-interface": ["hide ui", "hide interface"],
  "floorp-toggle-zen-mode": ["zen", "focus", "distraction free"],
  "floorp-toggle-command-palette": ["command palette", "palette", "command"],
  "floorp-open-settings": ["settings", "preferences", "options"],
  "floorp-open-hub": ["hub", "floorp hub", "settings", "preferences"],
  "floorp-show-pip": ["pip", "picture in picture", "mini player"],
  "floorp-toggle-share-mode": [
    "share",
    "share mode",
    "presentation",
    "screen share",
  ],
  "floorp-copy-page-url-as-markdown": [
    "copy",
    "markdown",
    "url",
    "title",
    "link",
    "obsidian",
  ],
  "gecko-enter-into-customize-mode": ["customize", "toolbar"],
  "gecko-quit-from-application": ["quit", "exit"],
};

/**
 * Get hiragana reading keywords for a given action/command ID from i18n.
 * Returns an empty array for non-Japanese locales or if no readings are defined.
 */
function getJapaneseReadings(id: string): string[] {
  try {
    const readings: unknown = i18next.t(`commandPaletteReadings.${id}`, {
      defaultValue: [] as string[],
      returnObjects: true,
    });
    if (Array.isArray(readings)) {
      return readings.filter((r): r is string => typeof r === "string");
    }
    return [];
  } catch (err) {
    console.error("[CommandPalette] getJapaneseReadings error for id=", id, err);
    return [];
  }
}

/**
 * Mouse gesture actions listed here will be excluded from the command palette.
 * Useful for: dangerous operations, duplicates, or commands that are not appropriate
 * for the palette UI. This does NOT affect mouse gesture assignment — excluded
 * actions remain available in the gesture settings.
 */
const EXCLUDED_PALETTE_ACTIONS: ReadonlySet<string> = new Set([
  // Add action IDs here to exclude them from the command palette, e.g.:
  "floorp-toggle-command-palette", // This would be redundant to include in the palette
  "gecko-search-the-web", // This is already available as a step command, and may not work well as a gesture
]);

const cachedCommands: Record<string, PaletteCommand[]> = {};

type ChromeWindow = Window & { gBrowser?: typeof globalThis.gBrowser };

function isVerticalMode(win?: Window): boolean {
  const tabContainer = (win as ChromeWindow | undefined)?.gBrowser
    ?.tabContainer as (EventTarget & { verticalMode?: boolean }) | undefined;
  return !!tabContainer?.verticalMode;
}

function getActionDisplayNameByOrientation(
  actionId: string,
  win?: Window,
): string {
  const isVertical = isVerticalMode(win);
  if (!isVertical) {
    return getActionDisplayName(actionId);
  }

  const verticalLabel = getActionDisplayName(`${actionId}-vertical`);
  return verticalLabel !== `${actionId}-vertical`
    ? verticalLabel
    : getActionDisplayName(actionId);
}

function getActionDescriptionByOrientation(
  actionId: string,
  win?: Window,
): string {
  const isVertical = isVerticalMode(win);
  if (!isVertical) {
    return getActionDescription(actionId);
  }

  const verticalDescription = getActionDescription(`${actionId}-vertical`);
  return verticalDescription !== `${actionId}-vertical`
    ? verticalDescription
    : getActionDescription(actionId);
}

function buildGestureCommands(win?: Window): PaletteCommand[] {
  const gestureActions = getAllGestureActions();
  return gestureActions
    .filter((action) => !EXCLUDED_PALETTE_ACTIONS.has(action.name))
    .map((action) => ({
      id: action.name,
      label: getActionDisplayNameByOrientation(action.name, win),
      description: getActionDescriptionByOrientation(action.name, win),
      category: ACTION_CATEGORY_MAP[action.name] ?? "tools",
      keywords: [
        ...(ACTION_KEYWORDS[action.name] ?? []),
        ...getJapaneseReadings(action.name),
      ],
      fn: action.fn,
    }));
}

function buildStepCommands(): PaletteCommand[] {
  return [
    bookmarkSwitcherCommand,
    closedTabSwitcherCommand,
    closedWindowSwitcherCommand,
    historySwitcherCommand,
    openUrlCommand,
    searchWebCommand,
    reopenInContainerCommand,
    tabSwitcherCommand,
  ];
}

export function getPaletteCommands(win?: Window): PaletteCommand[] {
  const orientationKey = isVerticalMode(win) ? "vertical" : "horizontal";
  if (!cachedCommands[orientationKey]) {
    cachedCommands[orientationKey] = buildGestureCommands(win);
  }
  const gestureCommands = cachedCommands[orientationKey];
  const tabCommands = win ? getTabCommands(win) : [];
  const stepCommands = buildStepCommands();
  return [...tabCommands, ...gestureCommands, ...stepCommands];
}

export function searchCommands(query: string, win?: Window): PaletteCommand[] {
  const commands = getPaletteCommands(win);
  if (!query.trim()) return commands;
  return fuzzySearch(query, commands);
}

export { isHistoryCommand, isBookmarkCommand };

/**
 * Search browsing history for the given query.
 */
export function searchHistoryCommands(
  query: string,
  limit: number = 10,
): Promise<PaletteCommand[]> {
  if (!query.trim()) return Promise.resolve([]);

  return searchHistory(query, limit);
}

/**
 * Search bookmarks for the given query.
 */
export function searchBookmarkCommands(
  query: string,
  limit: number = 10,
): Promise<PaletteCommand[]> {
  if (!query.trim()) return Promise.resolve([]);

  return searchBookmarks(query, limit);
}

export function getCommand(
  id: string,
  win?: Window,
): PaletteCommand | undefined {
  return getPaletteCommands(win).find((cmd) => cmd.id === id);
}

export function invalidateCache() {
  for (const key of Object.keys(cachedCommands)) {
    delete cachedCommands[key];
  }
}

export function getShortcutForAction(actionId: string): string | null {
  if (isTabCommand(actionId)) return null;
  try {
    const config = getConfig();
    for (const shortcut of Object.values(config.shortcuts)) {
      if (shortcut.action === actionId) {
        return shortcutToString(shortcut);
      }
    }
  } catch {
    // keyboard-shortcut module may not be available
  }
  return null;
}

addI18nObserver(() => {
  invalidateCache();
});
