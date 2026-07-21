/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { getConfig, isEnabled, isSafeErrorHandling } from "./config.ts";
import { gestureActions } from "../mouse-gesture/utils/gestures.ts";
import type { ShortcutConfig } from "./type.ts";

export class KeyboardShortcutController {
  private eventListenersAttached = false;
  private pressedKeys = new Set<string>();
  private pressedModifiers = {
    alt: false,
    ctrl: false,
    meta: false,
    shift: false,
  };

  private targetWindow: Window;

  constructor(win: Window = globalThis as unknown as Window) {
    this.targetWindow = win;
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
      this.targetWindow.removeEventListener("keydown", this.handleKeyDown, true);
      this.targetWindow.removeEventListener("keyup", this.handleKeyUp, true);
      this.eventListenersAttached = false;
    }
    this.resetState();
  }

  private resetState(): void {
    this.pressedKeys.clear();
    this.pressedModifiers = {
      alt: false,
      ctrl: false,
      meta: false,
      shift: false,
    };
  }

  private handleKeyDown = (event: KeyboardEvent): void => {
    if (!isEnabled()) return;

    this.pressedModifiers = {
      alt: event.altKey,
      ctrl: event.ctrlKey,
      meta: event.metaKey,
      shift: event.shiftKey,
    };

    const code = event.code;
    this.pressedKeys.add(code);

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

    if (this.checkAndExecuteShortcut()) {
      event.preventDefault();
      event.stopPropagation();
    }
  };

  private handleKeyUp = (event: KeyboardEvent): void => {
    if (!isEnabled()) return;

    const code = event.code;
    this.pressedKeys.delete(code);

    this.pressedModifiers = {
      alt: event.altKey,
      ctrl: event.ctrlKey,
      meta: event.metaKey,
      shift: event.shiftKey,
    };
  };

  private checkAndExecuteShortcut(): boolean {
    const config = getConfig();
    const shortcuts = config.shortcuts;
    const typing = this.isTypingContext();

    for (const [_id, shortcut] of Object.entries(shortcuts)) {
      if (this.isShortcutMatch(shortcut)) {
        // Bare printable keys (like plain "Z") must not fire while the user
        // is typing into an editable field.
        if (typing && this.isBarePrintableShortcut(shortcut)) {
          continue;
        }
        this.executeShortcut(shortcut);
        this.resetState();
        return true;
      }
    }

    return false;
  }

  private isBarePrintableShortcut(shortcut: ShortcutConfig): boolean {
    const m = shortcut.modifiers;
    return (
      !m.alt && !m.ctrl && !m.meta && !m.shift &&
      /^[A-Za-z0-9]$/.test(shortcut.key)
    );
  }

  private isTypingContext(): boolean {
    const doc = this.targetWindow.document;
    const el = doc?.activeElement as
      | (Element & { isInputFocused?: boolean })
      | null;
    if (!el) return false;

    const name = el.localName ?? "";
    if (name === "input" || name === "textarea") return true;
    if ((el as unknown as HTMLElement).isContentEditable) return true;
    // urlbar/searchbar/findbar host their <input> in shadow DOM, so
    // activeElement often reports the host element.
    if (el.closest?.("#urlbar, #searchbar, findbar")) return true;
    // Remote web content: the <browser> element holds focus. Trust its
    // input-focus hint when the platform provides one; otherwise assume
    // not typing (same behavior as before this guard).
    if (name === "browser") {
      return el.isInputFocused === true;
    }
    return false;
  }

  private isShortcutMatch(shortcut: ShortcutConfig): boolean {
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

    return this.pressedKeys.has(key);
  }

  private executeShortcut(shortcut: ShortcutConfig): void {
    if (isSafeErrorHandling()) {
      // Experiment: ks_safe_error_handling (treatment)
      // Expanded try-catch covers both getAction() resolution and fn()
      // invocation so callers can always run cleanup.
      try {
        const fn = gestureActions.getAction(shortcut.action);
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
      const fn = gestureActions.getAction(shortcut.action);
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
