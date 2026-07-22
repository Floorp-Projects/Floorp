/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { onCleanup } from "solid-js";
import {
  noraComponent,
  NoraComponentBase,
} from "#features-chrome/utils/base.ts";

const IS_MAC = /mac/i.test(navigator.platform ?? "");

/**
 * Move the window by holding a modifier chord and dragging anywhere —
 * toolbars, tab strip, or web content. Most useful in zen mode and
 * multirow tabs, where the draggable titlebar is hidden or crowded, but
 * always active.
 *
 * macOS: Alt+Cmd+drag. Windows/Linux: Ctrl+Alt+drag.
 * Plain Cmd cannot be used on its own: Cmd+click opens links in new tabs,
 * toggles tab multi-selection, and web apps (Figma, Miro, maps) use
 * Cmd+drag for canvas panning — a bare-Cmd drag start is ambiguous.
 */
@noraComponent(import.meta.hot)
export default class WindowDrag extends NoraComponentBase {
  init(): void {
    if (typeof window === "undefined" || typeof document === "undefined") {
      return;
    }

    let dragging = false;
    let lastScreenX = 0;
    let lastScreenY = 0;

    const chordHeld = (e: MouseEvent): boolean =>
      IS_MAC ? e.metaKey && e.altKey : e.ctrlKey && e.altKey;

    const onMouseDown = (e: MouseEvent) => {
      if (e.button !== 0 || !chordHeld(e)) return;
      dragging = true;
      lastScreenX = e.screenX;
      lastScreenY = e.screenY;
      e.preventDefault();
      e.stopImmediatePropagation();
    };

    const onMouseMove = (e: MouseEvent) => {
      if (!dragging) return;
      // Re-verify the chord on every event: a mouseup released outside the
      // window (or over a popup) never reaches us, and a latched drag flag
      // would otherwise keep relocating the window on every later mouse
      // move until it sits offscreen with only an edge visible.
      if (!chordHeld(e)) {
        dragging = false;
        return;
      }
      const dx = e.screenX - lastScreenX;
      const dy = e.screenY - lastScreenY;
      lastScreenX = e.screenX;
      lastScreenY = e.screenY;
      if (dx !== 0 || dy !== 0) {
        window.moveBy(dx, dy);
      }
      e.preventDefault();
      e.stopImmediatePropagation();
    };

    const endDrag = (e: MouseEvent) => {
      if (!dragging) return;
      dragging = false;
      e.preventDefault();
      e.stopImmediatePropagation();
    };

    // Releasing any part of the chord cancels the drag so the window
    // never sticks to the pointer.
    const onKeyUp = () => {
      dragging = false;
    };
    const onBlur = () => {
      dragging = false;
    };

    addEventListener("mousedown", onMouseDown, true);
    addEventListener("mousemove", onMouseMove, true);
    addEventListener("mouseup", endDrag, true);
    addEventListener("keyup", onKeyUp, true);
    addEventListener("blur", onBlur);

    onCleanup(() => {
      removeEventListener("mousedown", onMouseDown, true);
      removeEventListener("mousemove", onMouseMove, true);
      removeEventListener("mouseup", endDrag, true);
      removeEventListener("keyup", onKeyUp, true);
      removeEventListener("blur", onBlur);
    });

    this.logger.info("WindowDrag initialized (chord-drag window move)");
  }
}
