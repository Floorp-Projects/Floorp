/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let _isMac: boolean | undefined;

function isMac(): boolean {
  if (_isMac === undefined) {
    _isMac =
      typeof navigator !== "undefined" &&
      navigator?.platform?.toUpperCase().includes("MAC") === true;
  }
  return _isMac;
}

/**
 * Format a modifier key name for display.
 * On macOS, uses Mac-specific symbols and names.
 */
export function formatModifierLabel(
  modifier: "alt" | "ctrl" | "meta" | "shift",
): string {
  if (isMac()) {
    switch (modifier) {
      case "alt":
        return "⌥ Option";
      case "ctrl":
        return "⌃ Control";
      case "meta":
        return "⌘ Command";
      case "shift":
        return "⇧ Shift";
    }
  }
  switch (modifier) {
    case "alt":
      return "Alt";
    case "ctrl":
      return "Ctrl";
    case "meta":
      return "Meta";
    case "shift":
      return "Shift";
  }
}

/**
 * Format a modifier key as a compact symbol for preview.
 * On macOS, uses Mac-specific symbols.
 */
export function formatModifierSymbol(
  modifier: "alt" | "ctrl" | "meta" | "shift",
): string {
  if (isMac()) {
    switch (modifier) {
      case "alt":
        return "⌥";
      case "ctrl":
        return "⌃";
      case "meta":
        return "⌘";
      case "shift":
        return "⇧";
    }
  }
  switch (modifier) {
    case "alt":
      return "Alt";
    case "ctrl":
      return "Ctrl";
    case "meta":
      return "Meta";
    case "shift":
      return "Shift";
  }
}

export { isMac };
