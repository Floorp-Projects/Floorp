/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { getConfig, isEnabled, isSafeErrorHandling } from "./config.ts";
import { getKeyboardShortcutAction } from "./actions.ts";
import type { ShortcutConfig } from "./type.ts";
import {
  isBarePrintableKeyEvent,
  isKeyboardShortcutTypingContext,
  type KeyboardShortcutFocusStoreReader,
} from "./editable-focus.ts";

export class KeyboardShortcutController {
  private eventListenersAttached = false;
  private pressedModifiers = {
    alt: false,
    ctrl: false,
    meta: false,
    shift: false,
  };

  private targetWindow: Window;
  private remoteFocusStore: KeyboardShortcutFocusStoreReader | null;

  constructor(
    win: Window = globalThis as unknown as Window,
    remoteFocusStore: KeyboardShortcutFocusStoreReader | null = null,
  ) {
    this.targetWindow = win;
    this.remoteFocusStore = remoteFocusStore;
    this.init();
  }

  private init(): void {
    if (this.eventListenersAttached) return;

    this.targetWindow.addEventListener("keydown", this.handleKeyDown, true);
    this.targetWindow.addEventListener("keyup", this.handleKeyUp, true);
    this.eventListenersAttached = true;
  }

  public destroy(): void {
    if (this.eventListenersAttached) {
      this.targetWindow.removeEventListener(
        "keydown",
        this.handleKeyDown,
        true,
      );
      this.targetWindow.removeEventListener("keyup", this.handleKeyUp, true);
      this.eventListenersAttached = false;
    }
    this.resetState();
  }

  private resetState(): void {
    this.pressedModifiers = {
      alt: false,
      ctrl: false,
      meta: false,
      shift: false,
    };
  }

  private handleKeyDown = (event: KeyboardEvent): void => {
    if (!isEnabled()) return;
    if (event.repeat || event.getModifierState?.("AltGraph")) return;

    this.pressedModifiers = {
      alt: event.altKey,
      ctrl: event.ctrlKey,
      meta: event.metaKey,
      shift: event.shiftKey,
    };

    const code = event.code;

    // Synthetic events may lack a code; real key events always carry one.
    if (!code) return;

    if (
      isBarePrintableKeyEvent(event) &&
      isKeyboardShortcutTypingContext(
        this.targetWindow,
        this.remoteFocusStore,
      )
    ) {
      return;
    }

    // Ignore pure modifier key presses. Using startsWith keeps this concise
    // and handles location-specific variants like "AltLeft" / "AltRight".
    if (
      code.startsWith("Alt") ||
      code.startsWith("Control") ||
      code.startsWith("Meta") ||
      code.startsWith("Shift")
    ) {
      return;
    }

    if (this.checkAndExecuteShortcut(code)) {
      event.preventDefault();
      event.stopPropagation();
    }
  };

  private handleKeyUp = (event: KeyboardEvent): void => {
    if (!isEnabled()) return;

    this.pressedModifiers = {
      alt: event.altKey,
      ctrl: event.ctrlKey,
      meta: event.metaKey,
      shift: event.shiftKey,
    };
  };

  private checkAndExecuteShortcut(code: string): boolean {
    const config = getConfig();
    const shortcuts = config.shortcuts;

    for (const [_id, shortcut] of Object.entries(shortcuts)) {
      if (this.isShortcutMatch(shortcut, code)) {
        this.executeShortcut(shortcut);
        this.resetState();
        return true;
      }
    }

    return false;
  }

  private isShortcutMatch(shortcut: ShortcutConfig, currentCode: string): boolean {
    if (
      shortcut.modifiers.alt !== this.pressedModifiers.alt ||
      shortcut.modifiers.ctrl !== this.pressedModifiers.ctrl ||
      shortcut.modifiers.meta !== this.pressedModifiers.meta ||
      shortcut.modifiers.shift !== this.pressedModifiers.shift
    ) {
      return false;
    }

    let key = shortcut.key;
    if (/^[A-Z]$/.test(key)) {
      key = `Key${key}`;
    } else if (/^[0-9]$/.test(key)) {
      key = `Digit${key}`;
    }

    if (!key) return false;

    // Matching currentCode (the event's own code from this keydown) is
    // sufficient — no stale state is involved.
    return currentCode === key;
  }

  private executeShortcut(shortcut: ShortcutConfig): void {
    if (isSafeErrorHandling()) {
      // Experiment: ks_safe_error_handling (treatment)
      // Expanded try-catch covers both getAction() resolution and fn()
      // invocation so callers can always run cleanup.
      try {
        const fn = getKeyboardShortcutAction(shortcut.action);
        if (fn) {
          fn(this.targetWindow);
        }
      } catch (e) {
        console.error(
          `[keyboard-shortcut] Action "${shortcut.action}" failed:`,
          e,
        );
      }
    } else {
      // Control: original behaviour (try-catch only around fn call)
      const fn = getKeyboardShortcutAction(shortcut.action);
      if (fn) {
        try {
          fn(this.targetWindow);
        } catch (e) {
          console.error(
            `[keyboard-shortcut] Action "${shortcut.action}" failed:`,
            e,
          );
        }
      }
    }
  }
}
