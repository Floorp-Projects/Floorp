/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type {
  KeyboardShortcutActionDefinition,
  KeyboardShortcutActionOption,
  KeyboardShortcutConfig,
} from "../../types/pref.ts";
import { createDefaultKeyboardShortcutConfig as createDefaultKeyboardShortcutConfigWithPlatform } from "../../types/pref.ts";

export const INLINE_TAB_URL_ACTION_ID = "floorp-edit-tab-url" as const;

export const KEYBOARD_ONLY_ACTION_DEFINITIONS:
  readonly KeyboardShortcutActionDefinition[] = Object.freeze([
    Object.freeze({
      id: INLINE_TAB_URL_ACTION_ID,
      translationKey:
        `keyboardShortcut.actionLabels.${INLINE_TAB_URL_ACTION_ID}`,
    }),
  ]);

export function getKeyboardShortcutActionOptions(
  translate: (key: string, fallback: string) => string,
  existingOptions: readonly KeyboardShortcutActionOption[] = [],
): KeyboardShortcutActionOption[] {
  const existingIds = new Set(existingOptions.map((option) => option.id));
  const keyboardOnlyOptions = KEYBOARD_ONLY_ACTION_DEFINITIONS.map((
    action,
  ) => ({
    id: action.id,
    name: translate(action.translationKey, action.id),
  })).filter((action) => !existingIds.has(action.id));
  return [...existingOptions, ...keyboardOnlyOptions];
}

export function createDefaultKeyboardShortcutConfig(
  enabled: boolean,
): KeyboardShortcutConfig {
  // Delegate to the single source of truth in pref.ts, which requires a
  // platform string. Detect platform the same way the settings hook does.
  const platform = typeof navigator !== "undefined" &&
      navigator.platform.toUpperCase().includes("MAC")
    ? "macosx"
    : "other";

  return createDefaultKeyboardShortcutConfigWithPlatform(enabled, platform);
}
