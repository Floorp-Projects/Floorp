/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { signal } from "@preact/signals";
import type { Signal } from "@preact/signals";
import { effect } from "@preact/signals";
import { addDisposer, createRootHMR } from "#features-chrome/utils/base";
import {
  type KeyboardShortcutConfig,
  type ShortcutConfig,
  zKeyboardShortcutConfig,
} from "./type.ts";
import { clearLegacyConfig, migrateLegacyConfig } from "./migration.ts";
import { isRight } from "fp-ts/Either";

export const KEYBOARD_SHORTCUT_ENABLED_PREF = "floorp.keyboardshortcut.enabled";
export const KEYBOARD_SHORTCUT_CONFIG_PREF = "floorp.keyboardshortcut.config";

const createDefaultShortcuts = (): Record<string, ShortcutConfig> => {
  return {};
};

export const defaultConfig: KeyboardShortcutConfig = {
  enabled: true,
  shortcuts: createDefaultShortcuts(),
};

export const strDefaultConfig = JSON.stringify(defaultConfig);

const normalizeConfig = (
  config: Record<string, unknown>,
): KeyboardShortcutConfig => {
  return {
    ...defaultConfig,
    ...config,
    shortcuts:
      (config?.shortcuts as Record<string, ShortcutConfig> | undefined) ??
      defaultConfig.shortcuts,
  } as KeyboardShortcutConfig;
};

const parseConfig = (jsonStr: string): KeyboardShortcutConfig => {
  try {
    const parsed = JSON.parse(jsonStr);
    const result = zKeyboardShortcutConfig.decode(parsed);
    if (isRight(result)) {
      return normalizeConfig(result.right);
    }
    console.warn(
      "Keyboard shortcut configuration validation failed, attempting to recover partial data",
    );
    return normalizeConfig(parsed);
  } catch (e) {
    console.error(
      "Failed to parse keyboard shortcut configuration JSON, using default",
      e,
    );
    return defaultConfig;
  }
};

function createEnabled(): Signal<boolean> {
  const enabled = signal(
    Services.prefs.getBoolPref(
      KEYBOARD_SHORTCUT_ENABLED_PREF,
      defaultConfig.enabled,
    ),
  );

  addDisposer(effect(() => {
    Services.prefs.setBoolPref(KEYBOARD_SHORTCUT_ENABLED_PREF, enabled.value);
  }));

  const enabledObserver = () => {
    enabled.value = Services.prefs.getBoolPref(
      KEYBOARD_SHORTCUT_ENABLED_PREF,
      defaultConfig.enabled,
    );
  };

  Services.prefs.addObserver(KEYBOARD_SHORTCUT_ENABLED_PREF, enabledObserver);
  addDisposer(() => {
    Services.prefs.removeObserver(
      KEYBOARD_SHORTCUT_ENABLED_PREF,
      enabledObserver,
    );
  });

  return enabled;
}

function createConfig(): Signal<KeyboardShortcutConfig> {
  const legacyConfig = migrateLegacyConfig();
  if (legacyConfig) {
    clearLegacyConfig();
    Services.prefs.setStringPref(
      KEYBOARD_SHORTCUT_CONFIG_PREF,
      JSON.stringify(legacyConfig),
    );
  }

  const config = signal<KeyboardShortcutConfig>(
    parseConfig(
      Services.prefs.getStringPref(
        KEYBOARD_SHORTCUT_CONFIG_PREF,
        strDefaultConfig,
      ),
    ),
  );

  addDisposer(effect(() => {
    Services.prefs.setStringPref(
      KEYBOARD_SHORTCUT_CONFIG_PREF,
      JSON.stringify(config.value),
    );
  }));

  const configObserver = () => {
    config.value = parseConfig(
      Services.prefs.getStringPref(
        KEYBOARD_SHORTCUT_CONFIG_PREF,
        strDefaultConfig,
      ),
    );
  };

  Services.prefs.addObserver(KEYBOARD_SHORTCUT_CONFIG_PREF, configObserver);
  addDisposer(() => {
    Services.prefs.removeObserver(
      KEYBOARD_SHORTCUT_CONFIG_PREF,
      configObserver,
    );
  });

  return config;
}

export const _enabled: Signal<boolean> = createRootHMR(
  createEnabled,
  import.meta.hot,
);
export const _setEnabled = (v: boolean): void => {
  _enabled.value = v;
};

export const _config: Signal<KeyboardShortcutConfig> = createRootHMR(
  createConfig,
  import.meta.hot,
);
export const _setConfig = (v: KeyboardShortcutConfig): void => {
  _config.value = v;
};

export const isEnabled = () => _enabled.value;
export const setEnabled = (value: boolean) => { _enabled.value = value; };
export const getConfig = () => _config.value;
export const setConfig = (value: KeyboardShortcutConfig) => { _config.value = value; };

export function shortcutToString(shortcut: ShortcutConfig): string {
  const modifiers = [];
  if (shortcut.modifiers.alt) modifiers.push("Alt");
  if (shortcut.modifiers.ctrl) modifiers.push("Ctrl");
  if (shortcut.modifiers.meta) modifiers.push("Meta");
  if (shortcut.modifiers.shift) modifiers.push("Shift");

  return [...modifiers, shortcut.key.toUpperCase()].join("+");
}

export function stringToShortcut(str: string): ShortcutConfig {
  const parts = str.split("+").map((part) => part.toLowerCase());
  const key = parts.pop() || "";
  const modifiers = {
    alt: parts.includes("alt"),
    ctrl: parts.includes("ctrl"),
    meta: parts.includes("meta"),
    shift: parts.includes("shift"),
  };

  return { modifiers, key, action: "" }; // action は後で設定する必要があります
}
