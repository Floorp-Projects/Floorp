/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { useEffect, useState } from "react";
import { rpc } from "../../lib/rpc/rpc.ts";
import type {
  KeyboardShortcutConfig,
  ShortcutConfig,
} from "../../types/pref.ts";

const KEYBOARD_SHORTCUT_ENABLED_PREF = "floorp.keyboardshortcut.enabled";
const KEYBOARD_SHORTCUT_CONFIG_PREF = "floorp.keyboardshortcut.config";

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
          const configStr = await rpc.getStringPref(
            KEYBOARD_SHORTCUT_CONFIG_PREF,
          );
          if (configStr) {
            const parsedConfig = JSON.parse(configStr);
            setConfig({ ...parsedConfig, enabled });
          } else {
            throw new Error("Configuration not found");
          }
        } catch (parseError) {
          console.error("Failed to parse configuration", parseError);
          throw parseError;
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
      await rpc.setBoolPref(KEYBOARD_SHORTCUT_ENABLED_PREF, newConfig.enabled);
      const configToSave = { ...newConfig };
      const { enabled, ...configWithoutEnabled } = configToSave;
      await rpc.setStringPref(
        KEYBOARD_SHORTCUT_CONFIG_PREF,
        JSON.stringify(configWithoutEnabled),
      );

      setConfig(newConfig);
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
