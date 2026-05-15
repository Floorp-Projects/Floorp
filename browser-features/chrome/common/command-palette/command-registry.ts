// SPDX-License-Identifier: MPL-2.0

import {
  getAllGestureActions,
  getActionDescription,
  getActionDisplayName,
} from "../mouse-gesture/utils/gestures.ts";
import i18next from "i18next";
import { fuzzySearch } from "./fuzzy.ts";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import { getTabCommands, isTabCommand } from "./tab-provider.ts";
import { getConfig, shortcutToString } from "../keyboard-shortcut/config.ts";

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

  "floorp-show-pip": "media",
};

const ACTION_KEYWORDS: Record<string, string[]> = {
  "gecko-back": ["back", "previous"],
  "gecko-forward": ["forward", "next"],
  "gecko-reload": ["refresh", "reload"],
  "gecko-force-reload": ["hard refresh", "hard reload", "skip cache"],
  "gecko-close-tab": ["close", "remove tab"],
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
  "floorp-show-pip": ["pip", "picture in picture", "mini player"],
  "gecko-enter-into-customize-mode": ["customize", "toolbar"],
  "gecko-quit-from-application": ["quit", "exit"],
};

let cachedCommands: PaletteCommand[] | null = null;

function buildGestureCommands(): PaletteCommand[] {
  const gestureActions = getAllGestureActions();
  return gestureActions.map((action) => ({
    id: action.name,
    label: getActionDisplayName(action.name),
    description: getActionDescription(action.name),
    category: ACTION_CATEGORY_MAP[action.name] ?? "tools",
    keywords: ACTION_KEYWORDS[action.name] ?? [],
    fn: action.fn,
  }));
}

function buildStepCommands(): PaletteCommand[] {
  return [
    {
      id: "floorp-open-url",
      label: i18next.t("commandPalette.openUrl", { defaultValue: "Open URL" }),
      description: i18next.t("commandPalette.openUrlDescription", {
        defaultValue: "Open a URL in a new tab",
      }),
      category: "navigation",
      keywords: ["open url", "navigate", "go to", "open page", "url"],
      steps: [
        {
          id: "url",
          label: i18next.t("commandPalette.openUrlStepLabel", {
            defaultValue: "Enter URL to open",
          }),
          placeholder: i18next.t("commandPalette.openUrlStepPlaceholder", {
            defaultValue: "https://example.com",
          }),
          validate: (input: string): boolean | string => {
            const trimmed = input.trim();
            if (!trimmed)
              return i18next.t("commandPalette.openUrlValidationError", {
                defaultValue: "Please enter a valid URL",
              });
            return true;
          },
        },
      ],
      fn: (_win: Window, args?: Record<string, string>) => {
        const url = args?.url?.trim();
        if (!url) return;
        const navUrl = url.includes("://") ? url : `https://${url}`;
        try {
          globalThis.gBrowser?.loadURI?.(Services.io.newURI(navUrl));
        } catch (e) {
          console.error("[command-palette] Open URL failed", e);
        }
      },
    },
    {
      id: "floorp-search-web",
      label: i18next.t("commandPalette.searchWeb", {
        defaultValue: "Search the Web",
      }),
      description: i18next.t("commandPalette.searchWebDescription", {
        defaultValue: "Search with your default search engine",
      }),
      category: "search",
      keywords: ["search", "web search", "find", "lookup", "google"],
      steps: [
        {
          id: "query",
          label: i18next.t("commandPalette.searchWebStepLabel", {
            defaultValue: "Enter search query",
          }),
          placeholder: i18next.t("commandPalette.searchWebStepPlaceholder", {
            defaultValue: "Search terms...",
          }),
          validate: (input: string): boolean | string => {
            if (!input.trim()) return "Please enter a search query";
            return true;
          },
        },
      ],
      fn: (_win: Window, args?: Record<string, string>) => {
        const query = args?.query?.trim();
        if (!query) return;
        try {
          const engine = Services.search?.defaultEngine;
          if (engine) {
            const submission = engine.getSubmission(query);
            globalThis.gBrowser?.loadURI?.(submission.uri);
          }
        } catch (e) {
          console.error("[command-palette] Search failed", e);
        }
      },
    },
  ];
}

export function getPaletteCommands(win?: Window): PaletteCommand[] {
  if (!cachedCommands) {
    cachedCommands = buildGestureCommands();
  }
  const gestureCommands = cachedCommands;
  const tabCommands = win ? getTabCommands(win) : [];
  const stepCommands = buildStepCommands();
  return [...tabCommands, ...gestureCommands, ...stepCommands];
}

export function searchCommands(query: string, win?: Window): PaletteCommand[] {
  const commands = getPaletteCommands(win);
  if (!query.trim()) return commands;
  return fuzzySearch(query, commands);
}

export function getCommand(
  id: string,
  win?: Window,
): PaletteCommand | undefined {
  return getPaletteCommands(win).find((cmd) => cmd.id === id);
}

export function invalidateCache() {
  cachedCommands = null;
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
