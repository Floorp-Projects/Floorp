/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { useEffect, useRef, useState } from "react";
import { rpc } from "../../lib/rpc/rpc.ts";
import type {
  GestureAction,
  GestureDirection,
  MouseGestureConfig,
} from "../../types/pref.ts";

const MOUSE_GESTURE_ENABLED_PREF = "floorp.mousegesture.enabled";
const MOUSE_GESTURE_CONFIG_PREF = "floorp.mousegesture.config";

export const useMouseGestureConfig = () => {
  const [config, setConfig] = useState<MouseGestureConfig>(
    {} as MouseGestureConfig,
  );
  // Every updater below reads its "current" config from this ref, not from
  // `config` directly. Two rapid updates (e.g. changing both wheel-action
  // selectors before the first async save resolves) would otherwise both
  // build on the same stale `config` closure, and whichever save resolves
  // last would silently discard the other's change. The ref is updated
  // synchronously in `applyConfig`, before any await, so each subsequent
  // call always builds on the latest requested config.
  const configRef = useRef<MouseGestureConfig>(config);
  const applyConfig = (newConfig: MouseGestureConfig) => {
    configRef.current = newConfig;
    setConfig(newConfig);
  };
  // Persistence writes (rpc.setBoolPref/setStringPref) are queued through
  // this ref-backed promise chain so they always land in invocation order.
  // Without it, two saves in flight could complete out of order, and an
  // older save resolving last would overwrite a newer wheel/rocker-action
  // choice in the actual persisted pref - silently reverting it on the next
  // page load, even though in-memory state briefly showed the newer value.
  const saveQueueRef = useRef<Promise<boolean>>(Promise.resolve(true));
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    const loadConfig = async () => {
      try {
        setLoading(true);

        let enabled: boolean;
        try {
          const result = await rpc.getBoolPref(MOUSE_GESTURE_ENABLED_PREF);
          enabled = result === null ? false : result;
        } catch (e) {
          console.error("Failed to get enabled state", e);
          enabled = false;
        }

        const defaultConfig: MouseGestureConfig = {
          enabled,
          rockerGesturesEnabled: true,
          wheelGesturesEnabled: true,
          sensitivity: 40,
          showTrail: true,
          showLabel: true,
          trailColor: "#37ff00",
          trailWidth: 6,
          contextMenu: {
            minDistance: 12,
            preventionTimeout: 200,
          },
          actions: [],
          rockerActions: {
            leftRight: "gecko-forward",
            rightLeft: "gecko-back",
          },
          wheelActions: {
            scrollUp: "gecko-show-previous-tab",
            scrollDown: "gecko-show-next-tab",
          },
        };

        try {
          const configStr = await rpc.getStringPref(MOUSE_GESTURE_CONFIG_PREF);
          if (configStr) {
            const parsedConfig = JSON.parse(configStr);
            // Merge defaults with parsed config
            applyConfig({
              ...defaultConfig,
              ...parsedConfig,
              contextMenu: {
                ...defaultConfig.contextMenu,
                ...parsedConfig.contextMenu,
              },
              enabled,
            });
          } else {
            applyConfig(defaultConfig);
          }
        } catch (parseError) {
          console.error("Failed to parse configuration", parseError);
          applyConfig(defaultConfig);
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

      // If a newer update has already advanced configRef past this call's
      // snapshot (e.g. this save resolved after a later one, out of order),
      // committing newConfig to state now would revert that newer change.
      if (configRef.current === newConfig) {
        setConfig(newConfig);
      }
      return true;
    } catch (error) {
      console.error("Failed to save configuration", error);
      return false;
    }
  };

  const updateConfig = (partialConfig: Partial<MouseGestureConfig>) => {
    const newConfig = { ...configRef.current, ...partialConfig };
    // Apply optimistically so the UI reflects the choice immediately,
    // rather than lagging until the save round-trip completes.
    applyConfig(newConfig);

    const queuedSave = saveQueueRef.current.then(() => saveConfig(newConfig));
    saveQueueRef.current = queuedSave;
    return queuedSave;
  };

  const toggleEnabled = () =>
    updateConfig({ enabled: !configRef.current.enabled });

  const addAction = (action: GestureAction) =>
    updateConfig({ actions: [...configRef.current.actions, action] });

  const updateAction = (index: number, action: GestureAction) => {
    const newActions = [...configRef.current.actions];
    newActions[index] = action;
    return updateConfig({ actions: newActions });
  };

  const deleteAction = (index: number) => {
    const newActions = [...configRef.current.actions];
    newActions.splice(index, 1);
    return updateConfig({ actions: newActions });
  };

  const updateRockerAction = (rockerType: "leftRight" | "rightLeft", action: string) => {
    return updateConfig({
      rockerActions: {
        ...configRef.current.rockerActions,
        [rockerType]: action,
      },
    });
  };

  const updateWheelAction = (wheelType: "scrollUp" | "scrollDown", action: string) => {
    return updateConfig({
      wheelActions: {
        ...configRef.current.wheelActions,
        [wheelType]: action,
      },
    });
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
    updateRockerAction,
    updateWheelAction,
  };
};

export const patternToString = (pattern: GestureDirection[]) => {
  const directionMap: Record<GestureDirection, string> = {
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
