// SPDX-License-Identifier: MPL-2.0

/**
 * Default category priority for the command palette, in display order.
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
