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

  constructor() {
    this.init();
  }

  private init(): void {
    if (this.eventListenersAttached) return;

    if (typeof globalThis !== "undefined") {
      globalThis.addEventListener("keydown", this.handleKeyDown);
      globalThis.addEventListener("keyup", this.handleKeyUp);
      this.eventListenersAttached = true;
    }
  }

  public destroy(): void {
    if (typeof globalThis !== "undefined" && this.eventListenersAttached) {
      globalThis.removeEventListener("keydown", this.handleKeyDown);
      globalThis.removeEventListener("keyup", this.handleKeyUp);
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

    const key = event.key.toUpperCase();
    this.pressedKeys.add(key);

    if (
      key === "ALT" || key === "CONTROL" || key === "META" || key === "SHIFT"
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

    const key = event.key.toUpperCase();
    this.pressedKeys.delete(key);

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

    return this.pressedKeys.has(shortcut.key.toUpperCase());
  }

  private executeShortcut(shortcut: ShortcutConfig): void {
    const action = actions.find((a) => a.name === shortcut.action);
    if (action) {
      action.fn();
    }
  }
}
