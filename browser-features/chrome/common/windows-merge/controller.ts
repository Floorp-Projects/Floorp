// SPDX-License-Identifier: MPL-2.0

/**
 * Firefox wiring for the window merge: enumerates browser windows, moves each
 * tab-strip entry into the target window with the tabbrowser adoption APIs and
 * closes the emptied windows.
 */

import { WORKSPACE_TAB_ATTRIBUTION_ID } from "#features-chrome/common/workspaces/utils/workspaces-static-names.ts";
import { emptyMergeResult, mergeWindowsIntoOne } from "./merge.ts";
import type { MergeItemKind, MergeResult, MergeSource } from "./types.ts";

/** Action id used by the command palette, keyboard shortcuts and gestures. */
export const MERGE_WINDOWS_ACTION_ID = "floorp-merge-all-windows";

const BROWSER_WINDOW_TYPE = "navigator:browser";

const STRIP_TAG_TAB = "tab";
const STRIP_TAG_GROUP = "tab-group";
const STRIP_TAG_SPLIT_VIEW = "tab-split-view-wrapper";

interface PrivateBrowsingUtilsModule {
  PrivateBrowsingUtils: { isWindowPrivate(window: Window): boolean };
}

let privateBrowsingUtils:
  | PrivateBrowsingUtilsModule["PrivateBrowsingUtils"]
  | null = null;

/** Loaded on first use so opening a window does not pull the module in. */
function getPrivateBrowsingUtils(): PrivateBrowsingUtilsModule[
  "PrivateBrowsingUtils"
] {
  privateBrowsingUtils ??= (
    ChromeUtils.importESModule(
      "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
    ) as PrivateBrowsingUtilsModule
  ).PrivateBrowsingUtils;
  return privateBrowsingUtils;
}

interface AdoptOptions {
  elementIndex?: number;
  tabIndex?: number;
  selectTab?: boolean;
}

/** The part of gBrowser this feature uses. */
export interface MergeTabBrowser {
  readonly tabs: ArrayLike<XULElement>;
  readonly pinnedTabCount: number;
  readonly tabContainer?: {
    readonly pinnedTabsContainer?: Element | null;
    readonly arrowScrollbox?: Element | null;
  } | null;
  adoptTab(tab: XULElement, options?: AdoptOptions): XULElement | null;
  adoptTabGroup?(
    group: XULElement,
    options?: AdoptOptions,
  ): StripContainer | null;
  adoptSplitView?(
    container: XULElement,
    options?: AdoptOptions,
  ): StripContainer | null;
  hideTab?(tab: XULElement): void;
}

/** A browser window as far as the merge is concerned. */
export type MergeWindow =
  & Window
  & {
    gBrowser?: MergeTabBrowser;
    /** Present on browser windows; popups hide their toolbar. */
    toolbar?: { visible?: boolean };
  };

interface StripTab extends XULElement {
  closing?: boolean;
  pinned?: boolean;
  tabs?: ArrayLike<XULElement>;
}

/** A tab, tab group or split view: anything that can carry member tabs. */
export type StripContainer = StripTab;

export interface StripItem {
  readonly kind: MergeItemKind;
  readonly pinned: boolean;
  readonly tabCount: number;
  readonly element: XULElement;
}

interface WindowSource extends MergeSource {
  readonly window: MergeWindow;
  readonly browser: MergeTabBrowser;
}

/**
 * Classifies one child of the tab strip.
 *
 * Group and split-view containers are merged as a whole so their tabs keep
 * their relationship; anything else on the strip (periphery spacers, menus)
 * returns null and is left alone.
 */
export function classifyStripElement(element: Element): MergeItemKind | null {
  switch (element.localName.toLowerCase()) {
    case STRIP_TAG_TAB:
      return "tab";
    case STRIP_TAG_GROUP:
      return "group";
    case STRIP_TAG_SPLIT_VIEW:
      return "splitView";
    default:
      return null;
  }
}

function elementChildren(container: Element | null | undefined): Element[] {
  return container ? Array.from(container.children) : [];
}

function tabCountOf(item: StripTab, kind: MergeItemKind): number {
  if (kind === "tab") return 1;
  return Math.max(1, item.tabs?.length ?? 0);
}

function pushStripItem(
  items: StripItem[],
  element: Element,
  pinned: boolean,
): void {
  const kind = classifyStripElement(element);
  if (!kind) return;

  const tab = element as StripTab;
  // A closing tab is on its way out of the source window already.
  if (kind === "tab" && tab.closing) return;

  items.push({ kind, pinned, tabCount: tabCountOf(tab, kind), element: tab });
}

/**
 * Collects the mergeable entries of one window's tab strip in strip order.
 *
 * Hidden tabs (Floorp workspaces, collapsed groups) are part of the strip and
 * must not be dropped, so the DOM children are used instead of `gBrowser.tabs`
 * helpers that filter on visibility.
 */
export function stripItems(browser: MergeTabBrowser): StripItem[] {
  const items: StripItem[] = [];
  const pinnedTabsContainer = browser.tabContainer?.pinnedTabsContainer;
  const arrowScrollbox = browser.tabContainer?.arrowScrollbox;

  if (!pinnedTabsContainer && !arrowScrollbox) {
    // Older/renamed tab-container internals: fall back to the flat tab list so
    // the merge still moves tabs instead of silently doing nothing.
    console.warn(
      "[windows-merge] tab container internals unavailable, using the flat tab list",
    );
    const flatTabCount = browser.tabs.length;
    for (let index = 0; index < flatTabCount; index++) {
      const tab = browser.tabs[index] as StripTab;
      if (tab.closing) continue;
      items.push({
        kind: "tab",
        pinned: tab.pinned === true,
        tabCount: 1,
        element: tab,
      });
    }
    return items;
  }

  for (const element of elementChildren(pinnedTabsContainer)) {
    pushStripItem(items, element, true);
  }
  for (const element of elementChildren(arrowScrollbox)) {
    pushStripItem(items, element, false);
  }
  return items;
}

/** Carries the per-tab state that adoption does not copy by itself. */
function preserveTabState(
  sourceTab: XULElement,
  adoptedTab: XULElement,
  browser: MergeTabBrowser,
): void {
  const workspaceId = sourceTab.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID);
  if (workspaceId) {
    adoptedTab.setAttribute(WORKSPACE_TAB_ATTRIBUTION_ID, workspaceId);
  }
  // Workspace tabs and collapsed-group members are hidden; keep them hidden so
  // the merged window does not suddenly show every workspace at once.
  if (sourceTab.hasAttribute("hidden")) {
    browser.hideTab?.(adoptedTab);
  }
}

/**
 * Copies the per-tab state of a group's or split view's members onto the tabs
 * Firefox created for them.
 *
 * The adoption APIs return the new container, whose members are in the same
 * order as the source members, so the two lists are zipped by index.
 */
function preserveMemberState(
  sourceContainer: StripContainer,
  adoptedContainer: StripContainer,
  browser: MergeTabBrowser,
): void {
  const sourceMembers = Array.from(sourceContainer.tabs ?? []);
  const adoptedMembers = Array.from(adoptedContainer.tabs ?? []);
  const count = Math.min(sourceMembers.length, adoptedMembers.length);

  for (let index = 0; index < count; index++) {
    preserveTabState(sourceMembers[index], adoptedMembers[index], browser);
  }
}

export function adoptItem(
  item: StripItem,
  index: number,
  target: MergeTabBrowser,
): boolean {
  try {
    if (item.kind === "group") {
      if (typeof target.adoptTabGroup !== "function") return false;
      const adoptedGroup = target.adoptTabGroup(item.element, {
        tabIndex: index,
      });
      if (!adoptedGroup) return false;
      preserveMemberState(item.element, adoptedGroup, target);
      return true;
    }

    if (item.kind === "splitView") {
      if (typeof target.adoptSplitView !== "function") return false;
      const adoptedSplitView = target.adoptSplitView(item.element, {
        tabIndex: index,
      });
      if (!adoptedSplitView) return false;
      preserveMemberState(item.element, adoptedSplitView, target);
      return true;
    }

    const adoptedTab = target.adoptTab(item.element, {
      tabIndex: index,
      selectTab: false,
    });
    if (!adoptedTab) return false;

    preserveTabState(item.element, adoptedTab, target);
    return true;
  } catch (error) {
    console.error("[windows-merge] could not move a tab strip entry:", error);
    return false;
  }
}

function isPrivateWindow(window: Window): boolean {
  try {
    return getPrivateBrowsingUtils().isWindowPrivate(window);
  } catch (error) {
    // Treating an unknown window as non-private is safe: Firefox refuses the
    // cross-privacy adoption and the item then counts as failed.
    console.error("[windows-merge] could not read private state:", error);
    return false;
  }
}

/** Browser windows, front-most first. */
export function listBrowserWindows(): MergeWindow[] {
  const mediator = Services.wm as unknown as {
    getZOrderDOMWindowEnumerator?(
      windowType: string,
      frontToBack: boolean,
    ): Iterable<unknown>;
    getEnumerator(windowType: string): Iterable<unknown>;
  };

  const enumerator = typeof mediator.getZOrderDOMWindowEnumerator === "function"
    ? mediator.getZOrderDOMWindowEnumerator(BROWSER_WINDOW_TYPE, true)
    : mediator.getEnumerator(BROWSER_WINDOW_TYPE);

  const windows: MergeWindow[] = [];
  for (const candidate of enumerator) {
    windows.push(candidate as MergeWindow);
  }
  return windows;
}

function closeEmptiedWindow(window: MergeWindow): boolean {
  if (window.closed) return true;

  const remaining = window.gBrowser?.tabs.length ?? 0;
  if (remaining > 0) return false;

  try {
    window.close();
  } catch (error) {
    console.error("[windows-merge] could not close an emptied window:", error);
    return false;
  }
  return true;
}

/**
 * Whether this window belongs in a merge.
 *
 * Popups (`window.open` without the toolbar, e.g. a login or OAuth dialog) also
 * report `navigator:browser`; merging one would drag its tab into the target
 * window and then close the dialog, so they are left alone.
 */
export function isMergeableWindow(
  window: MergeWindow,
  target: Window,
): boolean {
  if (window === target || window.closed) return false;
  if (!window.gBrowser) return false;
  return window.toolbar?.visible !== false;
}

function collectSources(target: Window): WindowSource[] {
  const sources: WindowSource[] = [];
  for (const window of listBrowserWindows()) {
    if (!isMergeableWindow(window, target)) continue;

    const browser = window.gBrowser;
    if (!browser) continue;

    sources.push({
      window,
      browser,
      isPrivate: isPrivateWindow(window),
    });
  }
  return sources;
}

/**
 * Merges every other browser window into `window`, then closes the windows
 * whose tabs all moved.
 *
 * Private windows are skipped when the target is not private (and vice versa).
 */
export function mergeAllWindows(window: Window): MergeResult {
  const target = (window as MergeWindow)?.gBrowser;
  if (!target) {
    console.error("[windows-merge] the target window has no gBrowser");
    return emptyMergeResult();
  }

  const result = mergeWindowsIntoOne(collectSources(window), {
    targetIsPrivate: isPrivateWindow(window),
    itemsOf: (source) => stripItems(source.browser),
    nextIndex: (pinned) => pinned ? target.pinnedTabCount : target.tabs.length,
    adopt: (item, index) => adoptItem(item, index, target),
    closeSource: (source) => closeEmptiedWindow(source.window),
    onSkip: (source, reason) =>
      console.debug(
        "[windows-merge] skipped a window:",
        reason,
        isPrivateWindow(source.window) ? "(private)" : "(normal)",
      ),
  });

  console.info("[windows-merge] finished:", result);
  return result;
}
