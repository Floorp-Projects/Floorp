/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Pure conversion utilities between Floorp shortcut combos and the Firefox
 * XUL / CustomKeys formats.
 *
 * Floorp format: `{ modifiers: { alt, ctrl, meta, shift }, key: string }`
 * where `key` is an `event.code` such as "KeyA" or "F2" (see the
 * keyboard-shortcut feature's `Modifiers` type).
 *
 * XUL / CustomKeys format (customKeys.json and `<key>` elements):
 * `{ modifiers: "accel,shift", key?: "T", keycode?: "VK_F5" }` where
 * `modifiers` is a comma separated, alphabetically sorted token list and
 * `key` / `keycode` are mutually exclusive.
 *
 * This module is intentionally free of Firefox APIs so it can run both in
 * the browser and on the host (Deno). Whether the current platform is macOS
 * is always passed explicitly by the caller (`isMac`).
 */

/** Floorp shortcut modifier state. */
export interface Modifiers {
  alt: boolean;
  ctrl: boolean;
  meta: boolean;
  shift: boolean;
}

/** Firefox XUL / CustomKeys `<key>` representation. */
export interface XulCombo {
  modifiers: string;
  key?: string;
  keycode?: string;
}

/**
 * Body of an XUL key binding: either a character `key` or a `VK_*`
 * `keycode`, never both (matching the CustomKeys changeKey contract).
 */
export type XulKeyDefinition = { key: string } | { keycode: string };

interface XulKeyLookup {
  key?: string;
  keycode?: string;
}

/** Fixed canonical ordering for Floorp modifier names. */
const MODIFIER_ORDER = ["alt", "ctrl", "meta", "shift"] as const;

/** Fixed label ordering for formatted display text. */
const DISPLAY_MODIFIER_ORDER = ["ctrl", "alt", "shift", "meta"] as const;

type ModifierName = (typeof MODIFIER_ORDER)[number];

function createCodeToXulKeyTable(): Record<string, XulKeyDefinition> {
  const table: Record<string, XulKeyDefinition> = {
    Backspace: { keycode: "VK_BACK_SPACE" },
    Enter: { keycode: "VK_RETURN" },
    Escape: { keycode: "VK_ESCAPE" },
    Tab: { keycode: "VK_TAB" },
    Space: { key: " " },
    Delete: { keycode: "VK_DELETE" },
    Insert: { keycode: "VK_INSERT" },
    Home: { keycode: "VK_HOME" },
    End: { keycode: "VK_END" },
    PageUp: { keycode: "VK_PAGE_UP" },
    PageDown: { keycode: "VK_PAGE_DOWN" },
    ArrowUp: { keycode: "VK_UP" },
    ArrowDown: { keycode: "VK_DOWN" },
    ArrowLeft: { keycode: "VK_LEFT" },
    ArrowRight: { keycode: "VK_RIGHT" },
    Minus: { key: "-" },
    Equal: { key: "=" },
    BracketLeft: { key: "[" },
    BracketRight: { key: "]" },
    Semicolon: { key: ";" },
    Quote: { key: "'" },
    Backquote: { key: "`" },
    Comma: { key: "," },
    Period: { key: "." },
    Slash: { key: "/" },
    Backslash: { key: "\\" },
    NumpadAdd: { keycode: "VK_ADD" },
    NumpadSubtract: { keycode: "VK_SUBTRACT" },
    NumpadMultiply: { keycode: "VK_MULTIPLY" },
    NumpadDivide: { keycode: "VK_DIVIDE" },
    NumpadDecimal: { keycode: "VK_DECIMAL" },
  };

  for (let letter = 0; letter < 26; letter++) {
    const character = String.fromCharCode(65 + letter);
    table[`Key${character}`] = { key: character };
  }
  for (let digit = 0; digit <= 9; digit++) {
    table[`Digit${digit}`] = { key: `${digit}` };
  }
  for (let index = 1; index <= 24; index++) {
    table[`F${index}`] = { keycode: `VK_F${index}` };
  }
  for (let digit = 0; digit <= 9; digit++) {
    table[`Numpad${digit}`] = { keycode: `VK_NUMPAD${digit}` };
  }

  return table;
}

const CODE_TO_XUL_KEY: Readonly<Record<string, XulKeyDefinition>> = Object
  .freeze(createCodeToXulKeyTable());

function createXulKeyToCodeLookups(): {
  byKey: Map<string, string>;
  byKeycode: Map<string, string>;
} {
  const byKey = new Map<string, string>();
  const byKeycode = new Map<string, string>();
  for (const [code, definition] of Object.entries(CODE_TO_XUL_KEY)) {
    if ("key" in definition) {
      byKey.set(definition.key, code);
    } else {
      byKeycode.set(definition.keycode, code);
    }
  }
  return { byKey, byKeycode };
}

const XUL_KEY_TO_CODE_LOOKUPS = createXulKeyToCodeLookups();

const MAC_MODIFIER_LABELS: Record<ModifierName, string> = {
  alt: "Opt",
  ctrl: "Ctrl",
  meta: "Cmd",
  shift: "Shift",
};

const NON_MAC_MODIFIER_LABELS: Record<ModifierName, string> = {
  alt: "Alt",
  ctrl: "Ctrl",
  meta: "Meta",
  shift: "Shift",
};

/** Human readable labels for the common `VK_*` keycodes. */
const XUL_KEYCODE_LABELS: Readonly<Record<string, string>> = Object.freeze({
  VK_UP: "Up",
  VK_DOWN: "Down",
  VK_LEFT: "Left",
  VK_RIGHT: "Right",
  VK_BACK_SPACE: "Backspace",
  VK_RETURN: "Enter",
  VK_ESCAPE: "Escape",
  VK_TAB: "Tab",
  VK_DELETE: "Delete",
  VK_INSERT: "Insert",
  VK_HOME: "Home",
  VK_END: "End",
  VK_PAGE_UP: "PageUp",
  VK_PAGE_DOWN: "PageDown",
});

/**
 * Convert Floorp modifiers into the XUL `modifiers` attribute string
 * (CustomKeysParent spec: `ctrl` becomes "control" on macOS and "accel"
 * elsewhere; `meta` becomes "accel" on macOS and "meta" elsewhere).
 * Tokens are sorted alphabetically and joined with commas; an empty
 * modifier set yields "".
 */
export function modifiersToXulString(
  modifiers: Modifiers,
  isMac: boolean,
): string {
  const tokens: string[] = [];
  if (modifiers.alt) {
    tokens.push("alt");
  }
  if (modifiers.ctrl) {
    tokens.push(isMac ? "control" : "accel");
  }
  if (modifiers.meta) {
    tokens.push(isMac ? "accel" : "meta");
  }
  if (modifiers.shift) {
    tokens.push("shift");
  }
  tokens.sort();
  return tokens.join(",");
}

/**
 * Parse a XUL `modifiers` string ("accel,shift") back into Floorp
 * modifiers. Tokens are split on commas, trimmed and lowercased. "os" and
 * unrecognized tokens are ignored; "accel" maps to meta on macOS and ctrl
 * elsewhere, while "control" and "meta" keep their literal meaning on every
 * platform.
 */
export function xulStringToModifiers(
  value: string,
  isMac: boolean,
): Modifiers {
  const modifiers: Modifiers = {
    alt: false,
    ctrl: false,
    meta: false,
    shift: false,
  };
  for (const rawToken of value.split(",")) {
    const token = rawToken.trim().toLowerCase();
    if (token === "accel") {
      if (isMac) {
        modifiers.meta = true;
      } else {
        modifiers.ctrl = true;
      }
    } else if (token === "control") {
      modifiers.ctrl = true;
    } else if (token === "meta") {
      modifiers.meta = true;
    } else if (token === "alt") {
      modifiers.alt = true;
    } else if (token === "shift") {
      modifiers.shift = true;
    }
  }
  return modifiers;
}

/**
 * Convert an `event.code` ("KeyA", "F5", "ArrowUp", ...) into the XUL key
 * body (`{ key: "A" }` or `{ keycode: "VK_F5" }`). Returns null for codes
 * that have no XUL representation.
 */
export function codeToXulKey(code: string): XulKeyDefinition | null {
  if (!Object.hasOwn(CODE_TO_XUL_KEY, code)) {
    return null;
  }
  return CODE_TO_XUL_KEY[code];
}

/**
 * Convert an XUL key body back into an `event.code`. When both `key` and
 * `keycode` are present the `key` wins (mirroring Gecko's precedence).
 * Empty strings are treated as absent. Returns null when nothing maps.
 */
export function xulKeyToCode(xul: XulKeyLookup): string | null {
  const key = xul.key;
  if (typeof key === "string" && key.length > 0) {
    return XUL_KEY_TO_CODE_LOOKUPS.byKey.get(key) ?? null;
  }
  const keycode = xul.keycode;
  if (typeof keycode === "string" && keycode.length > 0) {
    return XUL_KEY_TO_CODE_LOOKUPS.byKeycode.get(keycode) ?? null;
  }
  return null;
}

/**
 * Canonical conflict-comparison key for a Floorp combo, e.g.
 * "alt+ctrl+KeyZ". Modifier names appear in the fixed order
 * alt, ctrl, meta, shift (only the enabled ones) followed by the
 * `event.code`-space key, joined with "+".
 */
export function comboToCanonicalCode(
  modifiers: Modifiers,
  code: string,
): string {
  const parts: string[] = [];
  for (const name of MODIFIER_ORDER) {
    if (modifiers[name]) {
      parts.push(name);
    }
  }
  parts.push(code);
  return parts.join("+");
}

/**
 * Canonical conflict-comparison key for a XUL combo. The modifiers are
 * parsed platform-aware and the key body is normalized into the
 * `event.code` space first; returns null when the key body cannot be
 * converted.
 */
export function xulComboToCanonicalCode(
  xul: XulCombo,
  isMac: boolean,
): string | null {
  const code = xulKeyToCode(xul);
  if (code === null) {
    return null;
  }
  return comboToCanonicalCode(xulStringToModifiers(xul.modifiers, isMac), code);
}

function formatXulKeycodeText(keycode: string): string {
  if (Object.hasOwn(XUL_KEYCODE_LABELS, keycode)) {
    return XUL_KEYCODE_LABELS[keycode];
  }
  return keycode.startsWith("VK_") ? keycode.slice(3) : keycode;
}

function formatXulKeyText(xul: XulKeyLookup): string {
  const key = xul.key;
  if (typeof key === "string" && key.length > 0) {
    return key.toUpperCase();
  }
  const keycode = xul.keycode;
  if (typeof keycode === "string" && keycode.length > 0) {
    return formatXulKeycodeText(keycode);
  }
  return "";
}

/**
 * Human readable text for a XUL combo, e.g. "Ctrl+Shift+T". On macOS the
 * labels are Ctrl/Opt/Shift/Cmd (accel shown as Cmd); elsewhere
 * Ctrl/Alt/Shift/Meta. Modifier labels always appear in the order
 * Ctrl, Alt, Shift, Meta. The key body prefers `key` (uppercased) and
 * falls back to a prettified keycode ("VK_F5" -> "F5",
 * "VK_BACK_SPACE" -> "Backspace", unknown codes keep their "VK_"-stripped
 * name). Returns "" when neither modifiers nor a key are present.
 */
export function formatXulComboText(xul: XulCombo, isMac: boolean): string {
  const modifiers = xulStringToModifiers(xul.modifiers, isMac);
  const labels = isMac ? MAC_MODIFIER_LABELS : NON_MAC_MODIFIER_LABELS;
  const parts: string[] = [];
  for (const name of DISPLAY_MODIFIER_ORDER) {
    if (modifiers[name]) {
      parts.push(labels[name]);
    }
  }
  const keyText = formatXulKeyText(xul);
  if (keyText.length > 0) {
    parts.push(keyText);
  }
  return parts.join("+");
}
