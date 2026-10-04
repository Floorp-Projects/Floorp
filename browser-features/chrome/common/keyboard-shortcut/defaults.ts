/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { KeyboardShortcutConfig, ShortcutConfig } from "./type.ts";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs",
);

export const KEYBOARD_SHORTCUT_SCHEMA_VERSION = 2 as const;
export const COMMAND_PALETTE_ACTION = "floorp-toggle-command-palette";
export const ZEN_MODE_ACTION = "floorp-toggle-zen-mode";

export function createZenModeShortcut(platform: string): ShortcutConfig {
  const isMac = platform === "macosx";

  return {
    key: "KeyZ",
    modifiers: {
      alt: true,
      ctrl: !isMac,
      meta: isMac,
      shift: false,
    },
    action: ZEN_MODE_ACTION,
  };
}

export function createDefaultShortcuts(
  platform: string = AppConstants.platform,
): Record<string, ShortcutConfig> {
  return {
    [COMMAND_PALETTE_ACTION]: {
      key: "F2",
      modifiers: { alt: false, ctrl: false, meta: false, shift: false },
      action: COMMAND_PALETTE_ACTION,
    },
    [ZEN_MODE_ACTION]: createZenModeShortcut(platform),
  };
}

export function createDefaultConfig(
  platform: string = AppConstants.platform,
): KeyboardShortcutConfig {
  return {
    schemaVersion: KEYBOARD_SHORTCUT_SCHEMA_VERSION,
    enabled: true,
    shortcuts: createDefaultShortcuts(platform),
  };
}
