// eslint-disable no-unused-vars
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { useEffect, useState } from "react";
import { rpc } from "../../lib/rpc/rpc.ts";
import type {
  GestureAction,
  GestureDirection,
  MouseGestureConfig,
} from "../../types/pref.ts";

const MOUSE_GESTURE_ENABLED_PREF = "floorp.mousegesture.enabled";
const MOUSE_GESTURE_CONFIG_PREF = "floorp.mousegesture.config";

export const useMouseGestureConfig = () => {
  const [config, setConfig] = useState<MouseGestureConfig>({} as MouseGestureConfig);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    const loadConfig = async () => {
      try {
        setLoading(true);

        let enabled: boolean;
        try {
          const result = await rpc.getBoolPref(MOUSE_GESTURE_ENABLED_PREF);
          enabled = result === null ? true : result;
        } catch (e) {
          console.error("Failed to get enabled state", e);
          enabled = true;
        }

        try {
          const configStr = await rpc.getStringPref(MOUSE_GESTURE_CONFIG_PREF);
          if (configStr) {
            const parsedConfig = JSON.parse(configStr);
            // Backward compatibility defaults
            const showLabel = parsedConfig.showLabel ?? true;
            setConfig({ ...parsedConfig, showLabel, enabled });
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

  const saveConfig = async (newConfig: MouseGestureConfig) => {
    console.log("saveConfig", newConfig);
    try {
      await rpc.setBoolPref(MOUSE_GESTURE_ENABLED_PREF, newConfig.enabled);
      const configToSave = { ...newConfig };
      // deno-lint-ignore no-unused-vars
      const { enabled, ...configWithoutEnabled } = configToSave;
      console.log("Saving configWithoutEnabled", configWithoutEnabled);
      await rpc.setStringPref(
        MOUSE_GESTURE_CONFIG_PREF,
        JSON.stringify(configWithoutEnabled),
      );

      setConfig(newConfig);
      return true;
    } catch (error) {
      console.error("Failed to save configuration", error);
      return false;
    }
  };

  const updateConfig = (partialConfig: Partial<MouseGestureConfig>) => {
    const newConfig = { ...config, ...partialConfig };
    return saveConfig(newConfig);
  };

  const toggleEnabled = () => updateConfig({ enabled: !config.enabled });

  const addAction = (action: GestureAction) =>
    updateConfig({ actions: [...config.actions, action] });

  const updateAction = (index: number, action: GestureAction) => {
    const newActions = [...config.actions];
    newActions[index] = action;
    return updateConfig({ actions: newActions });
  };

  const deleteAction = (index: number) => {
    const newActions = [...config.actions];
    newActions.splice(index, 1);
    return updateConfig({ actions: newActions });
  };

  return {
    config,
    loading,
    saveConfig,
    updateConfig,
    toggleEnabled,
    addAction,
    updateAction,
    deleteAction,
  };
};

export const patternToString = (pattern: GestureDirection[]) => {
  const directionMap = {
    up: "↑",
    down: "↓",
    left: "←",
    right: "→",
    upRight: "↗",
    upLeft: "↖",
    downRight: "↘",
    downLeft: "↙",
  };
  return pattern.map((p) => directionMap[p]).join("");
};

export const stringToPattern = (str: string): GestureDirection[] => {
  const directionMap = {
    "↑": "up",
    "↓": "down",
    "←": "left",
    "→": "right",
    "↗": "upRight",
    "↖": "upLeft",
    "↘": "downRight",
    "↙": "downLeft",
  } as const;

  return [...str].map((char) => {
    const direction = directionMap[char as keyof typeof directionMap];
    if (!direction) {
      throw new Error(`Invalid direction character: ${char}`);
    }
    return direction;
  });
};
