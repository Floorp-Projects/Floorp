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

  private executeScroll(win: Window, direction: ScrollDirection): void {
    const BEHAVIOR = "smooth"; // You can change this to "auto" if you want instant scrolling
    const LINE_SCROLL_AMOUNT = 100; // Amount to scroll for line up/down

    switch (direction) {
      case "lineUp":
        win.scrollBy({ top: -LINE_SCROLL_AMOUNT, behavior: BEHAVIOR });
        break;
      case "lineDown":
        win.scrollBy({ top: LINE_SCROLL_AMOUNT, behavior: BEHAVIOR });
        break;
      case "pageUp":
        win.scrollBy({ top: -win.innerHeight, behavior: BEHAVIOR });
        break;
      case "pageDown":
        win.scrollBy({ top: win.innerHeight, behavior: BEHAVIOR });
        break;
      case "lineRight":
        win.scrollBy({ left: LINE_SCROLL_AMOUNT, behavior: BEHAVIOR });
        break;
      case "lineLeft":
        win.scrollBy({ left: -LINE_SCROLL_AMOUNT, behavior: BEHAVIOR });
        break;
      case "toTop":
        win.scrollTo({ top: 0, behavior: BEHAVIOR });
        break;
      case "toBottom":
        win.scrollTo({
          top: win.document.documentElement?.scrollHeight ?? 0,
          behavior: BEHAVIOR,
        });
        break;
    }
  }
}
