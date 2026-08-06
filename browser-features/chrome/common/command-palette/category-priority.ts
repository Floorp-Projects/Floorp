// SPDX-License-Identifier: MPL-2.0

/**
 * Default category priority for the command palette, in display order.
 *
 * NOTE: This list is DUPLICATED in
 * `browser-features/pages-settings/src/app/command-palette/dataManager.ts`
 * because the settings app and chrome feature live in separate packages with
 * different build/i18n systems. If you edit one, edit the other. Both test
 * files assert length===19 as a partial guard.
 *
 * `recent` is intentionally absent: it is a runtime pseudo-category that is
 * always pinned to the very top of the list and never participates in priority
 * sorting. Hidden pseudo-categories (`navigation-suggestion`,
 * `search-suggestion`) are also absent; their positions are controller-driven
 * (top and bottom of the flat command list respectively).
 */
export const DEFAULT_CATEGORY_PRIORITY: readonly string[] = [
  "navigation",
  "tabs",
  "zoom",
  "bookmarks",
  "page",
  "search",
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

/**
 * Returns the priority index of `category` within `priorityList`.
 *
 * Categories that are not listed sink to the bottom of the sort by returning
 * `Number.MAX_SAFE_INTEGER`.
 */
export function getCategoryPriorityIndex(
  category: string,
  priorityList: readonly string[],
): number {
  const idx = priorityList.indexOf(category);
  return idx === -1 ? Number.MAX_SAFE_INTEGER : idx;
}

/**
 * Stable comparator: lower priority index first. Returns 0 when both items
 * share the same priority index, leaving their existing relative order intact
 * (assuming the caller uses `Array.prototype.sort`, which is stable in ES2019+).
 */
export function compareByPriority(
  a: { category: string },
  b: { category: string },
  priorityList: readonly string[],
): number {
  return (
    getCategoryPriorityIndex(a.category, priorityList) -
    getCategoryPriorityIndex(b.category, priorityList)
  );
}

/**
 * Parses the raw pref value (a JSON string array) into a `string[]`.
 *
 * Falls back to a copy of {@link DEFAULT_CATEGORY_PRIORITY} when the input is
 * missing, empty, not a JSON array, or contains non-string elements.
 */
export function parseCategoryPriority(
  raw: string | null | undefined,
): string[] {
  if (typeof raw !== "string" || raw.length === 0) {
    return [...DEFAULT_CATEGORY_PRIORITY];
  }
  try {
    const parsed: unknown = JSON.parse(raw);
    if (
      Array.isArray(parsed) &&
      parsed.every((el): el is string => typeof el === "string")
    ) {
      return parsed;
    }
  } catch {
    // ignore — fall through to default
  }
  return [...DEFAULT_CATEGORY_PRIORITY];
}

/**
 * Returns a NEW array sorted by category priority. Does NOT mutate the input.
 *
 * Uses a stable comparator so items sharing a priority index keep their
 * incoming relative order (e.g. fuzzy-score order from the caller).
 */
export function sortCategoriesByPriority<T extends { category: string }>(
  items: T[],
  priorityList: readonly string[],
): T[] {
  return [...items].sort((a, b) => compareByPriority(a, b, priorityList));
}

/**
 * Returns a NEW array containing at most `limit` items per category, preserving
 * the incoming order within each category. Items beyond the per-category cap are
 * dropped. Stable and non-mutating.
 *
 * A `limit` of 0 or negative is treated as unlimited (defensive — returns a
 * shallow copy of the input). This shouldn't happen in practice because the
 * settings UI clamps to `min: 1`, but the helper is public so it must not
 * return an empty list by accident.
 *
 * Pseudo-categories are NOT special-cased here. Callers must exempt them
 * explicitly if needed:
 * - `recent`: multi-item (recently-used commands). The controller exempts it
 *   in `doUpdateSearch` and `appendSuggestionResults` so the recents list is
 *   never truncated.
 * - `navigation-suggestion`, `search-suggestion`: each has only 1 item, so the
 *   limit has no practical effect.
 */
export function truncateByCategory<T extends { category: string }>(
  items: T[],
  limit: number,
): T[] {
  if (!Number.isFinite(limit) || limit <= 0) {
    return [...items];
  }
  const counts = new Map<string, number>();
  const result: T[] = [];
  const max = Math.floor(limit);
  for (const item of items) {
    const count = counts.get(item.category) ?? 0;
    if (count < max) {
      result.push(item);
      counts.set(item.category, count + 1);
    }
  }
  return result;
}
