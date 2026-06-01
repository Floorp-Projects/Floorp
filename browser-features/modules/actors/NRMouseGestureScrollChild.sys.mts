/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { findScrollableElement, VALID_SCROLL_DIRECTIONS, type ScrollDirection } from "./NRMouseGestureScrollUtils.ts";

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

  private executeScroll(win: Window, direction: ScrollDirection): void {
    const BEHAVIOR = "smooth"; // You can change this to "auto" if you want instant scrolling
    const LINE_SCROLL_AMOUNT = 100; // Amount to scroll for line up/down

    switch (direction) {
      case "lineUp": {
        const el = findScrollableElement(win, false);
        el?.scrollBy({ top: -LINE_SCROLL_AMOUNT, behavior: BEHAVIOR });
        break;
      }
      case "lineDown": {
        const el = findScrollableElement(win, false);
        el?.scrollBy({ top: LINE_SCROLL_AMOUNT, behavior: BEHAVIOR });
        break;
      }
      case "pageUp": {
        const el = findScrollableElement(win, false);
        el?.scrollBy({ top: -(el?.clientHeight ?? win.innerHeight), behavior: BEHAVIOR });
        break;
      }
      case "pageDown": {
        const el = findScrollableElement(win, false);
        el?.scrollBy({ top: el?.clientHeight ?? win.innerHeight, behavior: BEHAVIOR });
        break;
      }
      case "lineRight": {
        const el = findScrollableElement(win, true);
        el?.scrollBy({ left: LINE_SCROLL_AMOUNT, behavior: BEHAVIOR });
        break;
      }
      case "lineLeft": {
        const el = findScrollableElement(win, true);
        el?.scrollBy({ left: -LINE_SCROLL_AMOUNT, behavior: BEHAVIOR });
        break;
      }
      case "toTop": {
        const el = findScrollableElement(win, false);
        el?.scrollTo({ top: 0, behavior: BEHAVIOR });
        break;
      }
      case "toBottom": {
        const el = findScrollableElement(win, false);
        el?.scrollTo({
          top: (el?.scrollHeight ?? win.document.documentElement?.scrollHeight ?? 0),
          behavior: BEHAVIOR,
        });
        break;
      }
    }
  }
}
