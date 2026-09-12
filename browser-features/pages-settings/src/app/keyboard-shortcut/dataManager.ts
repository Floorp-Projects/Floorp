/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { useEffect, useState } from "react";
import { rpc } from "../../lib/rpc/rpc.ts";
import {
  createDefaultKeyboardShortcutConfig,
  KEYBOARD_SHORTCUT_SCHEMA_VERSION,
  parseKeyboardShortcutConfig,
  serializeKeyboardShortcutConfig,
  type KeyboardShortcutConfig,
  type ShortcutConfig,
} from "../../types/pref.ts";

declare const ChromeUtils:
  | {
    importESModule: (uri: string) => {
      AppConstants: { platform: string };
    };
  }
  | undefined;

const KEYBOARD_SHORTCUT_ENABLED_PREF = "floorp.keyboardshortcut.enabled";
const KEYBOARD_SHORTCUT_CONFIG_PREF = "floorp.keyboardshortcut.config";

export function getKeyboardShortcutPlatform(): string {
  try {
    if (typeof ChromeUtils !== "undefined") {
      return ChromeUtils.importESModule(
        "resource://gre/modules/AppConstants.sys.mjs",
      ).AppConstants.platform;
    }
  } catch (error) {
    console.error("Failed to read AppConstants.platform", error);
  }

  return typeof navigator !== "undefined" &&
      navigator.platform.toUpperCase().includes("MAC")
    ? "macosx"
    : "other";
}

export const useKeyboardShortcutConfig = () => {
  const [config, setConfig] = useState<KeyboardShortcutConfig>(
    {} as KeyboardShortcutConfig,
  );
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    const loadConfig = async () => {
      try {
        setLoading(true);

        let enabled: boolean;
        try {
          const result = await rpc.getBoolPref(KEYBOARD_SHORTCUT_ENABLED_PREF);
          enabled = result === null ? true : result;
        } catch (e) {
          console.error("Failed to get enabled state", e);
          enabled = true;
        }

        try {
          const platform = getKeyboardShortcutPlatform();
          const configStr = await rpc.getStringPref(
            KEYBOARD_SHORTCUT_CONFIG_PREF,
          );
          const loadedConfig = parseKeyboardShortcutConfig(
            configStr,
            enabled,
            platform,
          );
          const serializedConfig = serializeKeyboardShortcutConfig(
            loadedConfig,
          );

          if (configStr !== serializedConfig) {
            await rpc.setStringPref(
              KEYBOARD_SHORTCUT_CONFIG_PREF,
              serializedConfig,
            );
          }
          setConfig(loadedConfig);
        } catch (parseError) {
          console.error("Failed to load configuration", parseError);
          setConfig(
            createDefaultKeyboardShortcutConfig(
              enabled,
              getKeyboardShortcutPlatform(),
            ),
          );
        }
      } catch (error) {
        console.error("Failed to load configuration", error);
      } finally {
        setLoading(false);
      }
    };

    loadConfig();
  }, []);

  const saveConfig = async (newConfig: KeyboardShortcutConfig) => {
    try {
      const normalizedConfig: KeyboardShortcutConfig = {
        ...newConfig,
        schemaVersion: KEYBOARD_SHORTCUT_SCHEMA_VERSION,
      };
      await rpc.setBoolPref(
        KEYBOARD_SHORTCUT_ENABLED_PREF,
        normalizedConfig.enabled,
      );
      await rpc.setStringPref(
        KEYBOARD_SHORTCUT_CONFIG_PREF,
        serializeKeyboardShortcutConfig(normalizedConfig),
      );

      setConfig(normalizedConfig);
      return true;
    } catch (error) {
      console.error("Failed to save configuration", error);
      return false;
    }
  };

  const updateConfig = (partialConfig: Partial<KeyboardShortcutConfig>) => {
    const newConfig = { ...config, ...partialConfig };
    return saveConfig(newConfig);
  };

  const toggleEnabled = () => updateConfig({ enabled: !config.enabled });

  const addShortcut = (action: string, shortcut: ShortcutConfig) =>
    updateConfig({
      shortcuts: { ...config.shortcuts, [action]: shortcut },
    });

  const updateShortcut = (action: string, shortcut: ShortcutConfig) =>
    updateConfig({
      shortcuts: { ...config.shortcuts, [action]: shortcut },
    });

  const deleteShortcut = (action: string) => {
    const newShortcuts = { ...config.shortcuts };
    delete newShortcuts[action];
    return updateConfig({ shortcuts: newShortcuts });
  };

  return {
    config,
    loading,
    saveConfig,
    updateConfig,
    toggleEnabled,
    addShortcut,
    updateShortcut,
    deleteShortcut,
  };
};

/**
 * Formats a ShortcutConfig into a human-readable string (e.g. "Alt+Ctrl+F2").
 * Mirrors the chrome-side `shortcutToString` so the settings UI can display
 * the same key combination the browser will actually react to.
 */
export function shortcutToString(shortcut: ShortcutConfig): string {
  const modifiers: string[] = [];
  if (shortcut.modifiers.alt) modifiers.push("Alt");
  if (shortcut.modifiers.ctrl) modifiers.push("Ctrl");
  if (shortcut.modifiers.meta) modifiers.push("Meta");
  if (shortcut.modifiers.shift) modifiers.push("Shift");
  return [...modifiers, shortcut.key.toUpperCase()].join("+");
}
