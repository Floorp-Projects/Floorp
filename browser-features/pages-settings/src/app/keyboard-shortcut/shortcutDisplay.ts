/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { comboToCanonicalCode } from "../../../../modules/common/NRKeySyncCombo.ts";
import type { FirefoxKeyEntry } from "../../../../modules/common/NRKeySyncTypes.ts";
import type { ShortcutConfig } from "../../types/pref.ts";

/** Categories used only by the keyboard shortcut settings page. */
export type ShortcutCategoryId =
  | "tabs"
  | "windows"
  | "page"
  | "bookmarksAndHistory"
  | "appearance"
  | "scrolling"
  | "floorp"
  | "tools"
  | "other";

export type ShortcutSource = "floorp" | "firefox";

const SHORTCUT_CATEGORY_ORDER = [
  "tabs",
  "windows",
  "page",
  "bookmarksAndHistory",
  "appearance",
  "scrolling",
  "floorp",
  "tools",
  "other",
] as const satisfies readonly ShortcutCategoryId[];

const CATEGORY_ACTION_IDS: Readonly<
  Record<Exclude<ShortcutCategoryId, "other">, ReadonlySet<string>>
> = {
  tabs: new Set([
    "gecko-close-tab",
    "gecko-close-other-tabs",
    "gecko-close-tabs-to-start",
    "gecko-close-tabs-to-end",
    "gecko-open-new-tab",
    "gecko-duplicate-tab",
    "gecko-reload-all-tabs",
    "gecko-restore-last-tab",
    "gecko-show-next-tab",
    "gecko-show-previous-tab",
    "gecko-show-previously-selected-tab",
    "gecko-show-all-tabs-panel",
    "gecko-mute-current-tab",
  ]),
  windows: new Set([
    "gecko-open-new-window",
    "gecko-open-new-private-window",
    "gecko-close-window",
    "gecko-restore-last-window",
    "gecko-quit-from-application",
  ]),
  page: new Set([
    "gecko-back",
    "gecko-forward",
    "gecko-reload",
    "gecko-force-reload",
    "gecko-stop",
    "gecko-open-home-page",
    "gecko-zoom-in",
    "gecko-zoom-out",
    "gecko-reset-zoom",
    "gecko-search-in-this-page",
    "gecko-show-next-search-result",
    "gecko-show-previous-search-result",
    "gecko-search-the-web",
    "gecko-send-with-mail",
    "gecko-save-page",
    "gecko-print-page",
    "gecko-show-source-of-page",
    "gecko-show-page-info",
    "gecko-open-screen-capture",
  ]),
  bookmarksAndHistory: new Set([
    "gecko-bookmark-this-page",
    "gecko-open-bookmark-add-tool",
    "gecko-open-bookmarks-manager",
    "gecko-toggle-bookmark-toolbar",
    "gecko-forget-history",
    "gecko-quick-forget-history",
    "gecko-restore-last-session",
    "gecko-search-history",
    "gecko-manage-history",
    "gecko-open-downloads",
    "gecko-show-bookmark-sidebar",
    "gecko-show-history-sidebar",
    "gecko-show-synced-tabs-sidebar",
  ]),
  appearance: new Set([
    "gecko-enter-into-customize-mode",
    "gecko-reverse-sidebar",
    "gecko-hide-sidebar",
    "gecko-toggle-sidebar",
  ]),
  scrolling: new Set([
    "gecko-scroll-line-up",
    "gecko-scroll-line-down",
    "gecko-scroll-up",
    "gecko-scroll-down",
    "gecko-scroll-right",
    "gecko-scroll-left",
    "gecko-scroll-to-top",
    "gecko-scroll-to-bottom",
  ]),
  floorp: new Set([
    "floorp-edit-tab-url",
    "floorp-rest-mode",
    "floorp-hide-user-interface",
    "floorp-toggle-navigation-panel",
    "floorp-show-pip",
    "gecko-workspace-next",
    "gecko-workspace-previous",
    "floorp-toggle-zen-mode",
    "floorp-toggle-command-palette",
    "floorp-open-settings",
    "floorp-open-hub",
    "floorp-toggle-share-mode",
    "floorp-copy-page-url-as-markdown",
    "floorp-split-view-open-left",
    "floorp-split-view-open-right",
    "floorp-split-view-open-top",
    "floorp-split-view-open-bottom",
    "floorp-split-view-close",
    "floorp-split-view-swap-panes",
    "floorp-split-view-cycle-layout",
    "floorp-split-view-add-pane",
    "floorp-split-view-remove-pane",
  ]),
  tools: new Set([
    "gecko-open-addons-manager",
    "gecko-open-migration-wizard",
    "gecko-enter-into-offline-mode",
    "gecko-open-general-preferences",
    "gecko-open-privacy-preferences",
    "gecko-open-workspaces-preferences",
    "gecko-open-containers-preferences",
    "gecko-open-search-preferences",
    "gecko-open-sync-preferences",
    "gecko-open-task-manager",
  ]),
};

/**
 * Firefox key IDs whose function is identical to a Floorp action. IDs are
 * preferred over localized DOM labels because labels can vary by locale and
 * Firefox build.
 */
const FIREFOX_KEY_ACTION_BY_ID: Readonly<Record<string, string>> = Object
  .freeze({
    "key_back": "gecko-back",
    "key_forward": "gecko-forward",
    "key_reload": "gecko-reload",
    "key_reload_skip_cache": "gecko-force-reload",
    "key_stop": "gecko-stop",
    "key_newNavigatorTab": "gecko-open-new-tab",
    "key_newNavigator": "gecko-open-new-window",
    "key_newPrivateWindow": "gecko-open-new-private-window",
    "key_close": "gecko-close-tab",
    "key_closeWindow": "gecko-close-window",
    "key_undoCloseTab": "gecko-restore-last-tab",
    "key_undoCloseWindow": "gecko-restore-last-window",
    "key_nextTab": "gecko-show-next-tab",
    "key_previousTab": "gecko-show-previous-tab",
    "key_showAllTabs": "gecko-show-all-tabs-panel",
    "key_duplicateTab": "gecko-duplicate-tab",
    "key_toggleMute": "gecko-mute-current-tab",
    "goHomeKb": "gecko-open-home-page",
    "key_fullZoomEnlarge": "gecko-zoom-in",
    "key_fullZoomReduce": "gecko-zoom-out",
    "key_fullZoomReset": "gecko-reset-zoom",
    "key_find": "gecko-search-in-this-page",
    "key_findAgain": "gecko-show-next-search-result",
    "key_findPrevious": "gecko-show-previous-search-result",
    "key_search": "gecko-search-the-web",
    "key_search2": "gecko-search-the-web",
    "key_sendLink": "gecko-send-with-mail",
    "key_savePage": "gecko-save-page",
    "printKb": "gecko-print-page",
    "key_viewSource": "gecko-show-source-of-page",
    "key_viewInfo": "gecko-show-page-info",
    "addBookmarkAsKb": "gecko-bookmark-this-page",
    "bookmarkAllTabsKb": "gecko-open-bookmark-add-tool",
    "viewBookmarksSidebarKb": "gecko-show-bookmark-sidebar",
    "viewBookmarksToolbarKb": "gecko-toggle-bookmark-toolbar",
    "key_gotoHistory": "gecko-search-history",
    "showAllHistoryKb": "gecko-manage-history",
    "key_openDownloads": "gecko-open-downloads",
    "key_screenshot": "gecko-open-screen-capture",
    "key_aboutProcesses": "gecko-open-task-manager",
    "key_viewAboutProcesses": "gecko-open-task-manager",
    "key_togglePictureInPicture": "floorp-show-pip",
  });

/** Stable Gecko command IDs used as a fallback for renamed Firefox keys. */
const FIREFOX_KEY_ACTION_BY_COMMAND: Readonly<Record<string, string>> = Object
  .freeze({
    "Browser:Back": "gecko-back",
    "Browser:Forward": "gecko-forward",
    "Browser:Reload": "gecko-reload",
    "Browser:ReloadSkipCache": "gecko-force-reload",
    "Browser:Stop": "gecko-stop",
    "cmd_newNavigatorTab": "gecko-open-new-tab",
    "cmd_newNavigator": "gecko-open-new-window",
    "Tools:PrivateBrowsing": "gecko-open-new-private-window",
    "cmd_close": "gecko-close-tab",
    "cmd_closeWindow": "gecko-close-window",
    "cmd_undoCloseTab": "gecko-restore-last-tab",
    "cmd_undoCloseWindow": "gecko-restore-last-window",
    "Browser:NextTab": "gecko-show-next-tab",
    "Browser:PrevTab": "gecko-show-previous-tab",
    "Browser:ShowAllTabs": "gecko-show-all-tabs-panel",
    "cmd_duplicateTab": "gecko-duplicate-tab",
    "cmd_toggleMute": "gecko-mute-current-tab",
    "Browser:GoHome": "gecko-open-home-page",
    "cmd_fullZoomEnlarge": "gecko-zoom-in",
    "cmd_fullZoomReduce": "gecko-zoom-out",
    "cmd_fullZoomReset": "gecko-reset-zoom",
    "cmd_find": "gecko-search-in-this-page",
    "cmd_findAgain": "gecko-show-next-search-result",
    "cmd_findPrevious": "gecko-show-previous-search-result",
    "Tools:Search": "gecko-search-the-web",
    "Browser:SendLink": "gecko-send-with-mail",
    "Browser:AddBookmarkAs": "gecko-bookmark-this-page",
    "Browser:ShowAllBookmarks": "gecko-open-bookmarks-manager",
    "Browser:BookmarkAllTabs": "gecko-open-bookmark-add-tool",
    "viewBookmarksSidebar": "gecko-show-bookmark-sidebar",
    "Browser:BookmarksToolbar": "gecko-toggle-bookmark-toolbar",
    "History:SearchHistory": "gecko-search-history",
    "Browser:ShowAllHistory": "gecko-manage-history",
    "Browser:Downloads": "gecko-open-downloads",
    "Browser:SavePage": "gecko-save-page",
    "cmd_print": "gecko-print-page",
    "View:PageSource": "gecko-show-source-of-page",
    "View:PageInfo": "gecko-show-page-info",
    "Tools:Sanitize": "gecko-forget-history",
    "Browser:Screenshot": "gecko-open-screen-capture",
    "View:AboutProcesses": "gecko-open-task-manager",
    "View:PictureInPicture": "floorp-show-pip",
    "Tools:AddonMgr": "gecko-open-addons-manager",
  });

/** Normalize a Floorp shortcut key into the canonical combo space. */
export function getFloorpCanonicalCode(shortcut: ShortcutConfig): string {
  let key = shortcut.key;
  if (/^[A-Z]$/.test(key)) {
    key = `Key${key}`;
  } else if (/^[0-9]$/.test(key)) {
    key = `Digit${key}`;
  }

  return comboToCanonicalCode(shortcut.modifiers, key);
}

/** Resolve a Firefox live key to its Floorp canonical action, if known. */
export function getFirefoxKeyActionId(
  entry: FirefoxKeyEntry,
): string | null {
  return FIREFOX_KEY_ACTION_BY_ID[entry.id] ??
    FIREFOX_KEY_ACTION_BY_COMMAND[entry.command] ?? null;
}

function getCategoryForAction(actionId: string): ShortcutCategoryId {
  return (Object.entries(CATEGORY_ACTION_IDS) as Array<
    [Exclude<ShortcutCategoryId, "other">, ReadonlySet<string>]
  >).find(([, ids]) => ids.has(actionId))?.[0] ?? "other";
}

/**
 * Split actions into visible rows and rows replaced by Firefox. A mapped
 * Firefox action replaces only an unconfigured Floorp add row; configured
 * Floorp bindings remain editable and are flagged as conflicts for display.
 */
export function filterActionsForFirefoxPriority<
  TAction extends { id: string },
>(
  actions: readonly TAction[],
  shortcuts: Record<string, ShortcutConfig[]>,
  firefoxKeys: readonly FirefoxKeyEntry[],
): {
  visibleActions: TAction[];
  hiddenActionCount: number;
  visibleBindingsByAction: Record<string, ShortcutConfig[]>;
} {
  const firefoxActionIds = new Set(
    firefoxKeys.flatMap((entry) => {
      const actionId = getFirefoxKeyActionId(entry);
      return actionId === null ? [] : [actionId];
    }),
  );
  const visibleBindingsByAction: Record<string, ShortcutConfig[]> = {};
  let hiddenActionCount = 0;
  const visibleActions = actions.filter((action) => {
    const bindings = shortcuts[action.id] ?? [];
    hiddenActionCount += bindings.length === 0 &&
        firefoxActionIds.has(action.id)
      ? 1
      : 0;
    visibleBindingsByAction[action.id] = bindings;
    return bindings.length > 0 ? true : !firefoxActionIds.has(
      action.id,
    );
  });

  return {
    visibleActions,
    hiddenActionCount,
    visibleBindingsByAction,
  };
}

export interface ShortcutCategory<TAction> {
  id: ShortcutCategoryId;
  actions: TAction[];
}

export interface ShortcutActionOption {
  id: string;
  name: string;
}

/** A row in the unified Floorp/Firefox shortcut table. */
export interface UnifiedShortcutRow {
  /** Stable React key containing the source namespace. */
  key: string;
  source: ShortcutSource;
  /** Canonical action ID when this row can be compared across sources. */
  actionId: string | null;
  name: string;
  category: ShortcutCategoryId;
  /** Floorp persisted shortcut; Firefox shortcuts read their live entry. */
  shortcut?: ShortcutConfig;
  /** Index within the action's Floorp binding array. */
  shortcutIndex?: number;
  /** Synthetic row used to append another binding to the same action. */
  isAddRow?: boolean;
  /** Whether the Floorp binding shares a canonical code with Firefox. */
  hasFirefoxConflict?: boolean;
  /** Firefox live entry used for editing, reset and conflict display. */
  firefoxKey?: FirefoxKeyEntry;
}

/** Normalize shortcut search text, including modifier/key separators. */
export function normalizeShortcutSearchText(value: string): string {
  return value
    .normalize("NFKD")
    .replace(/[\u0300-\u036f]/g, "")
    .toLowerCase()
    .replace(/[+\-_]/g, " ")
    .replace(/\s+/g, " ")
    .trim();
}

/** Build searchable shortcut text that accepts both "T" and "KeyT". */
function shortcutToSearchText(shortcut: ShortcutConfig): string {
  const modifiers = [
    shortcut.modifiers.alt && "Alt",
    shortcut.modifiers.ctrl && "Ctrl",
    shortcut.modifiers.meta && "Meta",
    shortcut.modifiers.shift && "Shift",
  ].filter((value): value is string => typeof value === "string");
  const key = shortcut.key.replace(/^(Key|Digit)/, "") || shortcut.key;
  return [...modifiers, key.toUpperCase()].join("+");
}

/**
 * Keep only rows matching the query in action names, source names, category
 * names, labels or shortcut text. Punctuation such as "+" is ignored so both
 * "ctrl t" and "ctrl+t" find Ctrl+T.
 */
export function filterUnifiedShortcutRows(
  categories: ReadonlyArray<{
    id: ShortcutCategoryId;
    actions: readonly UnifiedShortcutRow[];
  }>,
  query: string,
  translate: (key: string) => string,
): Array<{
  id: ShortcutCategoryId;
  actions: UnifiedShortcutRow[];
}> {
  const normalizedQuery = normalizeShortcutSearchText(query);
  if (normalizedQuery.length === 0) {
    return categories.map(({ id, actions }) => ({
      id,
      actions: [...actions],
    }));
  }

  return categories.flatMap(({ id, actions }) => {
    const categoryName = translate(`keyboardShortcut.categories.${id}`);
    const matchingActions = actions.filter((row) =>
      [
        row.name,
        row.actionId ?? "",
        row.source,
        categoryName,
        row.firefoxKey?.label ?? "",
        row.firefoxKey?.shortcutText ?? "",
        row.firefoxKey?.keycode ?? "",
        row.shortcut?.key ?? "",
        row.shortcut ? shortcutToSearchText(row.shortcut) : "",
      ].map(normalizeShortcutSearchText).some((text) =>
        text.includes(normalizedQuery)
      )
    );

    return matchingActions.length > 0 ? [{ id, actions: matchingActions }] : [];
  });
}

/**
 * Build one table containing both settings systems. Firefox keys mapped to a
 * canonical action use that action's display name, while unmapped Firefox
 * keys keep Firefox's best-effort label. Existing Firefox priority filtering
 * is applied to Floorp rows before they are added.
 */
export function buildUnifiedShortcutRows(
  actions: readonly ShortcutActionOption[],
  shortcuts: Record<string, ShortcutConfig[]>,
  firefoxKeys: readonly FirefoxKeyEntry[],
): {
  rows: UnifiedShortcutRow[];
  hiddenActionCount: number;
} {
  const {
    visibleActions,
    hiddenActionCount,
    visibleBindingsByAction,
  } = filterActionsForFirefoxPriority(actions, shortcuts, firefoxKeys);
  const actionNameById = new Map(
    actions.map((action) => [action.id, action.name]),
  );
  const visibleActionIds = new Set(visibleActions.map((action) => action.id));
  const rows: UnifiedShortcutRow[] = [];
  const firefoxByAction = new Map<string, FirefoxKeyEntry[]>();
  const unmappedFirefoxKeys: FirefoxKeyEntry[] = [];
  const firefoxConflictCodes = new Set(
    firefoxKeys.flatMap((entry) =>
      entry.canonicalCode === null ? [] : [entry.canonicalCode]
    ),
  );

  for (const entry of firefoxKeys) {
    const actionId = getFirefoxKeyActionId(entry);
    if (!actionId || !actionNameById.has(actionId)) {
      unmappedFirefoxKeys.push(entry);
      continue;
    }

    const entries = firefoxByAction.get(actionId) ?? [];
    entries.push(entry);
    firefoxByAction.set(actionId, entries);
  }

  for (const action of actions) {
    if (visibleActionIds.has(action.id)) {
      const bindings = visibleBindingsByAction[action.id] ?? [];
      bindings.forEach((shortcut, index) => {
        rows.push({
          key: `floorp:${action.id}:${index}`,
          source: "floorp",
          actionId: action.id,
          name: action.name,
          category: getCategoryForAction(action.id),
          shortcut,
          shortcutIndex: shortcuts[action.id]?.indexOf(shortcut) ?? index,
          hasFirefoxConflict: firefoxConflictCodes.has(
            getFloorpCanonicalCode(shortcut),
          ),
        });
      });
    }

    for (const firefoxKey of firefoxByAction.get(action.id) ?? []) {
      rows.push({
        key: `firefox:${firefoxKey.id}`,
        source: "firefox",
        actionId: action.id,
        name: action.name,
        category: getCategoryForAction(action.id),
        firefoxKey,
      });
    }

    if (visibleActionIds.has(action.id)) {
      rows.push({
        key: `floorp-add:${action.id}`,
        source: "floorp",
        actionId: action.id,
        name: action.name,
        category: getCategoryForAction(action.id),
        isAddRow: true,
      });
    }
  }

  for (const firefoxKey of unmappedFirefoxKeys) {
    rows.push({
      key: `firefox:${firefoxKey.id}`,
      source: "firefox",
      actionId: null,
      name: firefoxKey.label,
      category: "other",
      firefoxKey,
    });
  }

  return { rows, hiddenActionCount };
}

/** Group actions by stable category while preserving each source order. */
export function groupActionsByCategory<TAction extends { id: string }>(
  actions: readonly TAction[],
): Array<ShortcutCategory<TAction>> {
  const grouped = new Map<ShortcutCategoryId, TAction[]>(
    SHORTCUT_CATEGORY_ORDER.map((id) => [id, []]),
  );

  for (const action of actions) {
    grouped.get(getCategoryForAction(action.id))?.push(action);
  }

  return SHORTCUT_CATEGORY_ORDER.flatMap((id) => {
    const categoryActions = grouped.get(id) ?? [];
    return categoryActions.length === 0
      ? []
      : [{ id, actions: categoryActions }];
  });
}

/** Group unified rows using the category stored on each row. */
export function groupUnifiedShortcutRows(
  rows: readonly UnifiedShortcutRow[],
): Array<ShortcutCategory<UnifiedShortcutRow>> {
  const grouped = new Map<ShortcutCategoryId, UnifiedShortcutRow[]>(
    SHORTCUT_CATEGORY_ORDER.map((id) => [id, []]),
  );

  for (const row of rows) {
    grouped.get(row.category)?.push(row);
  }

  return SHORTCUT_CATEGORY_ORDER.flatMap((id) => {
    const categoryRows = grouped.get(id) ?? [];
    return categoryRows.length === 0 ? [] : [{ id, actions: categoryRows }];
  });
}
