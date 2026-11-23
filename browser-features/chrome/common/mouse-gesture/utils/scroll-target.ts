// SPDX-License-Identifier: MPL-2.0

/** Keep scrolling in the selected tab, including its focused child frame. */
export function getScrollTargetContext<T extends { readonly top: unknown }>(
  selected: T,
  focused: T | null | undefined,
): T {
  return focused && focused.top === selected ? focused : selected;
}
