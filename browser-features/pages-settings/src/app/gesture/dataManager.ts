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
import {
  createDefaultMouseGestureConfig,
  createGestureConfigPersistence,
  type GestureConfigPersistence,
  MOUSE_GESTURE_CONFIG_PREF,
  MOUSE_GESTURE_ENABLED_PREF,
  type MouseGestureConfigUpdate,
  parseMouseGestureConfig,
} from "./configPersistence.ts";

export const useMouseGestureConfig = () => {
  const [config, setConfig] = useState<MouseGestureConfig>(
    () => createDefaultMouseGestureConfig(false),
  );
  const [loading, setLoading] = useState(true);
  const [pending, setPending] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const persistenceRef = useRef<GestureConfigPersistence | null>(null);

  useEffect(() => {
    let cancelled = false;
    let unsubscribe: (() => void) | null = null;

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

        let loadedConfig = createDefaultMouseGestureConfig(enabled);
        try {
          const configStr = await rpc.getStringPref(MOUSE_GESTURE_CONFIG_PREF);
          loadedConfig = parseMouseGestureConfig(configStr, enabled);
        } catch (parseError) {
          console.error("Failed to load configuration", parseError);
        }

        if (cancelled) return;
        const persistence = createGestureConfigPersistence(loadedConfig, {
          writeEnabled: (nextEnabled) =>
            rpc.setBoolPref(MOUSE_GESTURE_ENABLED_PREF, nextEnabled),
          writeConfig: (serializedConfig) =>
            rpc.setStringPref(
              MOUSE_GESTURE_CONFIG_PREF,
              serializedConfig,
            ),
        });
        persistenceRef.current = persistence;
        unsubscribe = persistence.subscribe((snapshot) => {
          if (cancelled) return;
          setConfig(snapshot.config);
          setPending(snapshot.pending);
          setError(snapshot.error);
        });
      } catch (error) {
        console.error("Failed to load configuration", error);
      } finally {
        if (!cancelled) setLoading(false);
      }
    };

    loadConfig();
    return () => {
      cancelled = true;
      unsubscribe?.();
      persistenceRef.current = null;
    };
  }, []);

  const updateConfig = (update: MouseGestureConfigUpdate) =>
    persistenceRef.current?.updateConfig(update) ?? Promise.resolve(false);

  const toggleEnabled = () =>
    persistenceRef.current?.updateEnabled((enabled) => !enabled) ??
      Promise.resolve(false);

  const addAction = (action: GestureAction) =>
    updateConfig((current) => ({
      actions: [...current.actions, action],
    }));

  const updateAction = (index: number, action: GestureAction) =>
    updateConfig((current) => {
      const actions = [...current.actions];
      actions[index] = action;
      return { actions };
    });

  const deleteAction = (index: number) =>
    updateConfig((current) => ({
      actions: current.actions.filter((_, actionIndex) =>
        actionIndex !== index
      ),
    }));

  const updateRockerAction = (
    rockerType: "leftRight" | "rightLeft",
    action: string,
  ) =>
    updateConfig((current) => ({
      rockerActions: {
        ...current.rockerActions,
        [rockerType]: action,
      },
    }));

  const updateWheelAction = (
    wheelType: "scrollUp" | "scrollDown",
    action: string,
  ) =>
    updateConfig((current) => ({
      wheelActions: {
        ...current.wheelActions,
        [wheelType]: action,
      },
    }));

  return {
    config,
    loading,
    pending,
    error,
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
