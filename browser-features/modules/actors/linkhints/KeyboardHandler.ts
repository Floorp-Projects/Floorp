/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

export interface KeyboardHandlerCallbacks {
  /** Called when a character is typed (hint character) */
  onCharacter: (char: string) => void;
  /** Called when backspace is pressed */
  onBackspace: () => void;
  /** Called when Escape is pressed */
  onEscape: () => void;
  /** Called when Enter is pressed (for activating active hint in filter mode) */
  onEnter?: () => void;
}

export class KeyboardHandler {
  private callbacks: KeyboardHandlerCallbacks;
  private boundHandler: ((e: KeyboardEvent) => void) | null = null;
  private active = false;

  constructor(callbacks: KeyboardHandlerCallbacks) {
    this.callbacks = callbacks;
  }

  /**
   * Start capturing keyboard events on the given window.
   */
  start(win: Window): void {
    if (this.active) return;
    this.active = true;
    this.boundHandler = (e: KeyboardEvent) => this.handleKeyEvent(e);
    win.addEventListener("keydown", this.boundHandler, true);
  }

  /**
   * Stop capturing keyboard events.
   */
  stop(win: Window): void {
    if (!this.active) return;
    if (this.boundHandler) {
      win.removeEventListener("keydown", this.boundHandler, true);
      this.boundHandler = null;
    }
    this.active = false;
  }

  private handleKeyEvent(e: KeyboardEvent): void {
    // Don't capture if user is in an input field
    const target = e.target;
    if (target instanceof HTMLElement) {
      const tagName = target.tagName.toLowerCase();
      if (tagName === "input" || tagName === "textarea" || tagName === "select") {
        return;
      }
      if (target.isContentEditable) return;
    }

    if (e.key === "Escape") {
      e.preventDefault();
      e.stopPropagation();
      this.callbacks.onEscape();
      return;
    }

    if (e.key === "Backspace") {
      e.preventDefault();
      e.stopPropagation();
      this.callbacks.onBackspace();
      return;
    }

    if (e.key === "Enter") {
      e.preventDefault();
      e.stopPropagation();
      this.callbacks.onEnter?.();
      return;
    }

    // Only handle single printable characters
    if (e.key.length === 1 && !e.ctrlKey && !e.altKey && !e.metaKey) {
      e.preventDefault();
      e.stopPropagation();
      this.callbacks.onCharacter(e.key);
    }
  }
}
