/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const VALID_SCROLL_DIRECTIONS = [
  "lineUp",
  "lineDown",
  "pageUp",
  "pageDown",
  "lineRight",
  "lineLeft",
  "toTop",
  "toBottom",
] as const;

type ScrollDirection = (typeof VALID_SCROLL_DIRECTIONS)[number];

export class NRMouseGestureScrollChild extends JSWindowActorChild {
  receiveMessage(message: { name: string; data?: unknown }) {
    switch (message.name) {
      case "MouseGestureScroll:Execute": {
        const data = message.data as Record<string, unknown> | undefined;
        const direction =
          typeof data?.direction === "string" &&
          (VALID_SCROLL_DIRECTIONS as readonly string[]).includes(
            data.direction,
          )
            ? (data.direction as ScrollDirection)
            : undefined;
        if (!direction) return;

        const win = this.contentWindow;
        if (!win) return;

        this.executeScroll(win, direction);
        break;
      }
    }
  }

  private findScrollableElement(win: Window, horizontal: boolean): Element | null {
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

  private executeScroll(win: Window, direction: ScrollDirection): void {
    const BEHAVIOR = "smooth"; // You can change this to "auto" if you want instant scrolling
    const LINE_SCROLL_AMOUNT = 100; // Amount to scroll for line up/down

    switch (direction) {
      case "lineUp": {
        const el = this.findScrollableElement(win, false);
        el?.scrollBy({ top: -LINE_SCROLL_AMOUNT, behavior: BEHAVIOR });
        break;
      }
      case "lineDown": {
        const el = this.findScrollableElement(win, false);
        el?.scrollBy({ top: LINE_SCROLL_AMOUNT, behavior: BEHAVIOR });
        break;
      }
      case "pageUp": {
        const el = this.findScrollableElement(win, false);
        el?.scrollBy({ top: -(el?.clientHeight ?? win.innerHeight), behavior: BEHAVIOR });
        break;
      }
      case "pageDown": {
        const el = this.findScrollableElement(win, false);
        el?.scrollBy({ top: el?.clientHeight ?? win.innerHeight, behavior: BEHAVIOR });
        break;
      }
      case "lineRight": {
        const el = this.findScrollableElement(win, true);
        el?.scrollBy({ left: LINE_SCROLL_AMOUNT, behavior: BEHAVIOR });
        break;
      }
      case "lineLeft": {
        const el = this.findScrollableElement(win, true);
        el?.scrollBy({ left: -LINE_SCROLL_AMOUNT, behavior: BEHAVIOR });
        break;
      }
      case "toTop": {
        const el = this.findScrollableElement(win, false);
        el?.scrollTo({ top: 0, behavior: BEHAVIOR });
        break;
      }
      case "toBottom": {
        const el = this.findScrollableElement(win, false);
        el?.scrollTo({
          top: (el?.scrollHeight ?? win.document.documentElement?.scrollHeight ?? 0),
          behavior: BEHAVIOR,
        });
        break;
      }
    }
  }
}
