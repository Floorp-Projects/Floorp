// SPDX-License-Identifier: MPL-2.0

/**
 * Merge core: moves every tab of the source windows into one target window and
 * closes the emptied windows.
 *
 * Only ordering and bookkeeping live here. Adoption, insertion points and
 * window closing are injected, so the behaviour is testable without a browser
 * and the Firefox calls stay in one place (see controller.ts).
 */

import type {
  MergeableItem,
  MergeDeps,
  MergeResult,
  MergeSource,
} from "./types.ts";

export type {
  MergeableItem,
  MergeDeps,
  MergeItemKind,
  MergeResult,
  MergeSkipReason,
  MergeSource,
} from "./types.ts";

export function emptyMergeResult(): MergeResult {
  return {
    movedItems: 0,
    movedTabs: 0,
    failedItems: 0,
    closedWindows: 0,
    skippedWindows: 0,
  };
}

/**
 * Moves the tabs of `sources` into the target window, front-most source first.
 *
 * A source window is closed only when every one of its tab-strip entries moved
 * and it has no tabs left. Private and non-private windows are never mixed:
 * Firefox refuses to move those tabs, and moving them would leak private
 * browsing content into a normal window.
 */
export function mergeWindowsIntoOne<
  TItem extends MergeableItem,
  TSource extends MergeSource,
>(
  sources: readonly TSource[],
  deps: MergeDeps<TItem, TSource>,
): MergeResult {
  const result = emptyMergeResult();

  for (const source of sources) {
    if (source.isPrivate !== deps.targetIsPrivate) {
      result.skippedWindows++;
      deps.onSkip?.(source, "private-mismatch");
      continue;
    }

    const items = deps.itemsOf(source);
    if (items.length === 0) {
      result.skippedWindows++;
      deps.onSkip?.(source, "empty");
      continue;
    }

    let failedItems = 0;
    for (const item of items) {
      // The index is recomputed every time: each adoption grows the target.
      const adopted = deps.adopt(item, deps.nextIndex(item.pinned));
      if (!adopted) {
        failedItems++;
        result.failedItems++;
        continue;
      }
      result.movedItems++;
      result.movedTabs += item.tabCount;
    }

    // A window that still owns tabs (refused moves) must stay open.
    if (failedItems === 0 && deps.closeSource(source)) {
      result.closedWindows++;
    }
  }

  return result;
}
