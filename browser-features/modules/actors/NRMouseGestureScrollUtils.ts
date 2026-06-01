/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export const VALID_SCROLL_DIRECTIONS = [
  "lineUp",
  "lineDown",
  "pageUp",
  "pageDown",
  "lineRight",
  "lineLeft",
  "toTop",
  "toBottom",
] as const;

export type ScrollDirection = (typeof VALID_SCROLL_DIRECTIONS)[number];

export function findScrollableElement(win: Window, horizontal: boolean): Element | null {
  const doc = win.document;
  let el = doc.activeElement || doc.body;
  if (!el) return doc.scrollingElement || doc.documentElement;

  while (el && el !== doc.documentElement) {
    const style = win.getComputedStyle(el);
    if (horizontal) {
      const overflowX = style.overflowX;
      if (
        (overflowX === "auto" || overflowX === "scroll") &&
        el.scrollWidth > el.clientWidth
      ) {
        return el;
      }
    } else {
      const overflowY = style.overflowY;
      if (
        (overflowY === "auto" || overflowY === "scroll") &&
        el.scrollHeight > el.clientHeight
      ) {
        return el;
      }
    }
    el = el.parentElement;
  }

  return doc.scrollingElement || doc.documentElement;
}
