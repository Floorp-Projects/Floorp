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
