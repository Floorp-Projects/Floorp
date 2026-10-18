/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Shared types for the key sync service that mirrors Firefox live `<key>`
 * elements (the `#mainKeyset` plus `ext-keyset-*` keysets) into the chrome
 * process and exposes the CustomKeys write API on top of that mirror.
 */

/**
 * One mirrored `<key>` element. Raw attributes are preserved verbatim so
 * callers can rebuild or diff the live DOM state; the derived fields
 * (`shortcutText`, `canonicalCode`) come from NRKeySyncCombo utilities.
 */
export interface FirefoxKeyEntry {
  /** Key element id, e.g. "key_newNavigator". */
  id: string;
  /** "extension" when the key lives in an `ext-keyset-id-*` keyset. */
  keysetKind: "main" | "extension";
  /** Raw XUL modifiers attribute ("" when absent). */
  modifiers: string;
  /** Raw key attribute ("" when absent). */
  key: string;
  /** Raw keycode attribute ("" when absent). */
  keycode: string;
  /** Raw command attribute ("" when absent). */
  command: string;
  /** `internal="true"` keys are not user-editable. */
  internal: boolean;
  /** `reserved="true"` keys are interpreted early in the parent process. */
  reserved: boolean;
  /** True when CustomKeys holds a stored default, i.e. the user customized it. */
  customized: boolean;
  /** Best-effort display name resolved from command / menuitem / id. */
  label: string;
  /** Human readable combo, e.g. "Ctrl+Shift+T" (formatXulComboText). */
  shortcutText: string;
  /** Conflict comparison code (null when the key body is not convertible). */
  canonicalCode: string | null;
}

/** Listener invoked with a fresh snapshot after the mirror changed. */
export type KeySyncChangeListener = (keys: FirefoxKeyEntry[]) => void;

/** Result of a write operation (changeKey / resetKey). */
export interface KeySyncChangeResult {
  ok: boolean;
  error?: string;
}

/**
 * Structural type of the Firefox CustomKeys module
 * (`moz-src:///browser/components/customkeys/CustomKeys.sys.mjs`,
 * Firefox 147+). Imports are feature-detected, so consumers must treat
 * availability as optional.
 */
export interface CustomKeysModule {
  changeKey(
    id: string,
    combo: { modifiers?: string; key?: string; keycode?: string },
  ): void;
  resetKey(id: string): void;
  clearKey(id: string): void;
  clearAll(): void;
  resetAll(): void;
  /** Returns the default key only when it has been customized; null otherwise. */
  getDefaultKey(
    keyId: string,
  ): { modifiers?: string; key?: string; keycode?: string } | null;
  initWindow(window: Window): void;
  uninitWindow(window: Window): void;
}
