/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { getConfig, isEnabled } from "./config.ts";
import { actions } from "../mouse-gesture/utils/actions.ts";
import { type ShortcutConfig } from "./type.ts";

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

    this.targetWindow.addEventListener("keydown", this.handleKeyDown);
    this.targetWindow.addEventListener("keyup", this.handleKeyUp);
    this.eventListenersAttached = true;
  }

  public destroy(): void {
    if (this.eventListenersAttached) {
      this.targetWindow.removeEventListener("keydown", this.handleKeyDown);
      this.targetWindow.removeEventListener("keyup", this.handleKeyUp);
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

    for (const [_id, shortcut] of Object.entries(shortcuts)) {
      if (this.isShortcutMatch(shortcut)) {
        this.executeShortcut(shortcut);
        this.resetState();
        return true;
      }
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
    const action = actions.find((a) => a.name === shortcut.action);
    if (action) {
      action.fn();
    }
  }
}
