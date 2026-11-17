// SPDX-License-Identifier: MPL-2.0

/** A top-level entry of a window's tab strip. */
export type MergeItemKind = "tab" | "group" | "splitView";

/**
 * One tab-strip entry that can be moved into another window.
 *
 * Groups and split views are moved as a whole so their tabs keep their
 * relationship; a plain tab carries one tab.
 */
export interface MergeableItem {
  readonly kind: MergeItemKind;
  /** Pinned items land directly after the target window's pinned tabs. */
  readonly pinned: boolean;
  /** Tabs carried by this item: 1 for a tab, n for a group or split view. */
  readonly tabCount: number;
}

/** A window whose tabs can be merged into the target window. */
export interface MergeSource {
  /** True when every tab in this window belongs to private browsing. */
  readonly isPrivate: boolean;
}

/** Why a source window was left untouched. */
export type MergeSkipReason = "private-mismatch" | "empty";

/** Firefox-side operations the merge core drives. */
export interface MergeDeps<
  TItem extends MergeableItem,
  TSource extends MergeSource,
> {
  /** Whether the target window is a private browsing window. */
  readonly targetIsPrivate: boolean;
  /** Top-level tab-strip entries of `source`, in tab-strip order. */
  itemsOf(source: TSource): readonly TItem[];
  /** Insertion index for the next item with the given pin state. */
  nextIndex(pinned: boolean): number;
  /** Moves one item into the target window; false when Firefox refused. */
  adopt(item: TItem, index: number): boolean;
  /** Closes a source window that no longer owns tabs; true when it is gone. */
  closeSource(source: TSource): boolean;
  onSkip?(source: TSource, reason: MergeSkipReason): void;
}

/** Bookkeeping for one merge run. */
export interface MergeResult {
  /** Items moved into the target window. */
  movedItems: number;
  /** Tabs moved into the target window (groups/split views carry several). */
  movedTabs: number;
  /** Items Firefox refused to move; their window is left open. */
  failedItems: number;
  /** Source windows closed because all their tabs moved. */
  closedWindows: number;
  /** Source windows left untouched, e.g. private ones next to a normal target. */
  skippedWindows: number;
}

// ---------------------------------------------------------------------------
// Firefox-facing types (see controller.ts for the implementation)
// ---------------------------------------------------------------------------

/** Options the tabbrowser adoption methods accept. */
export interface AdoptOptions {
  elementIndex?: number;
  tabIndex?: number;
  selectTab?: boolean;
}

/** The part of `gBrowser` this feature uses. */
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

/** A tab: it may carry `tabs` when Firefox reports it as a container. */
export interface StripTab extends XULElement {
  closing?: boolean;
  pinned?: boolean;
  tabs?: ArrayLike<XULElement>;
}

/** A tab, tab group or split view: anything that can carry member tabs. */
export type StripContainer = StripTab;

/** One entry of a source window's tab strip, ready to be adopted. */
export interface StripItem {
  readonly kind: MergeItemKind;
  readonly pinned: boolean;
  readonly tabCount: number;
  readonly element: XULElement;
}

/** A source window bound to its tabbrowser. */
export interface WindowSource extends MergeSource {
  readonly window: MergeWindow;
  readonly browser: MergeTabBrowser;
}

/** Shape of the lazily imported PrivateBrowsingUtils module. */
export interface PrivateBrowsingUtilsModule {
  PrivateBrowsingUtils: { isWindowPrivate(window: Window): boolean };
}
