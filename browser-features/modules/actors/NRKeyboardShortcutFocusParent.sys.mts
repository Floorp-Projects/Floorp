/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  nrKeyboardShortcutFocusStore,
} from "../common/NRKeyboardShortcutFocusStore.ts";
import type { NRKeyboardShortcutFocusStore } from "../common/NRKeyboardShortcutFocusStore.ts";
import {
  NR_KEYBOARD_SHORTCUT_FOCUS_MESSAGE,
  type NRKeyboardShortcutFocusUpdate,
} from "../common/NRKeyboardShortcutFocusTypes.ts";

export { nrKeyboardShortcutFocusStore };

export function applyKeyboardShortcutFocusUpdate(
  store: NRKeyboardShortcutFocusStore,
  frame: object,
  browser: object | null,
  data: unknown,
): void {
  if (!browser) {
    store.removeFrame(frame);
    return;
  }

  const editable = (data as Partial<NRKeyboardShortcutFocusUpdate> | null)
    ?.editable;
  if (typeof editable !== "boolean") {
    store.removeFrame(frame);
    return;
  }

  store.setFrameEditable(browser, frame, editable);
}

export class NRKeyboardShortcutFocusParent extends JSWindowActorParent {
  receiveMessage(message: { name: string; data?: unknown }): void {
    if (message.name !== NR_KEYBOARD_SHORTCUT_FOCUS_MESSAGE) {
      return;
    }

    const browser = this.browsingContext?.top?.embedderElement ?? null;
    applyKeyboardShortcutFocusUpdate(
      nrKeyboardShortcutFocusStore,
      this,
      browser,
      message.data,
    );
  }

  didDestroy(): void {
    // Navigation, tab closure, and actor teardown all remove this frame token.
    nrKeyboardShortcutFocusStore.removeFrame(this);
  }
}
