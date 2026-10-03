/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

export interface KeyboardShortcutFocusStoreReader {
  isEditableFocused(browser: object): boolean;
}

type ChromeFocusElement = Element & {
  isContentEditable?: boolean;
};

const keySegmenter = new Intl.Segmenter(undefined, { granularity: "grapheme" });

/** Text keys can contain surrogate pairs, combining marks or joined emoji. */
export function isPrintableKeyValue(key: string): boolean {
  if (!key || /\p{Cc}/u.test(key)) return false;
  const segments = keySegmenter.segment(key)[Symbol.iterator]();
  segments.next();
  // Named keys (ArrowLeft, F1, etc.) consist of multiple graphemes.
  return segments.next().done === true;
}

/**
 * Identify unmodified text input for the editable typing guard.
 * Matching still uses KeyboardEvent.code in the controller.
 */
export function isBarePrintableKeyEvent(event: KeyboardEvent): boolean {
  return isPrintableKeyValue(event.key) &&
    !event.altKey &&
    !event.ctrlKey &&
    !event.metaKey &&
    !event.shiftKey &&
    !event.isComposing &&
    !event.getModifierState?.("AltGraph");
}

/** Return whether focus is in an editable owned by the chrome document. */
export function isChromeEditableFocused(win: Window): boolean {
  const element = win.document?.activeElement as ChromeFocusElement | null;
  if (!element) {
    return false;
  }

  const localName = element.localName?.toLowerCase() ?? "";
  if (localName === "input" || localName === "textarea") {
    return true;
  }
  if (element.isContentEditable === true) {
    return true;
  }

  // These chrome controls host their editable in anonymous/shadow content,
  // so the document activeElement can be the host rather than the input.
  return element.closest("#urlbar, #searchbar, findbar") !== null;
}

/** Return the remote browser that currently owns chrome focus, if any. */
export function getFocusedRemoteBrowser(win: Window): object | null {
  const element = win.document?.activeElement;
  return element?.localName?.toLowerCase() === "browser" ? element : null;
}

/**
 * Synchronously combine independent chrome and remote editable state.
 * A missing or failed remote reader intentionally preserves prior behavior.
 */
export function isKeyboardShortcutTypingContext(
  win: Window,
  remoteFocusStore: KeyboardShortcutFocusStoreReader | null,
): boolean {
  if (isChromeEditableFocused(win)) {
    return true;
  }

  const browser = getFocusedRemoteBrowser(win);
  if (!browser || !remoteFocusStore) {
    return false;
  }

  try {
    return remoteFocusStore.isEditableFocused(browser);
  } catch (_error) {
    return false;
  }
}
