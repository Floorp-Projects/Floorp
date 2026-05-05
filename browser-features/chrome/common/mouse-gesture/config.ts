/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { signal } from "@preact/signals";
import type { Signal } from "@preact/signals";
import { effect } from "@preact/signals";
import { addDisposer, createRootHMR } from "#features-chrome/utils/base";
import * as t from "io-ts";
import { isRight } from "fp-ts/Either";

export const MOUSE_GESTURE_ENABLED_PREF = "floorp.mousegesture.enabled";
export const MOUSE_GESTURE_CONFIG_PREF = "floorp.mousegesture.config";

/**
 * Gesture direction types matching the existing preference format.
 */
export const GestureDirectionCodec = t.union([
  t.literal("up"),
  t.literal("down"),
  t.literal("left"),
  t.literal("right"),
  t.literal("upRight"),
  t.literal("upLeft"),
  t.literal("downRight"),
  t.literal("downLeft"),
]);
export type GestureDirection = t.TypeOf<typeof GestureDirectionCodec>;
export type GesturePattern = GestureDirection[];

/**
 * A gesture action maps a pattern to an action name.
 */
export const GestureActionCodec = t.type({
  pattern: t.array(GestureDirectionCodec),
  action: t.string,
});
export type GestureAction = t.TypeOf<typeof GestureActionCodec>;

const ContextMenuCodec = t.type({
  minDistance: t.number,
  preventionTimeout: t.number,
});

const RockerActionsCodec = t.type({
  leftRight: t.string,
  rightLeft: t.string,
});
export type RockerActions = t.TypeOf<typeof RockerActionsCodec>;

const MouseGestureConfigRequired = t.type({
  rockerGesturesEnabled: t.boolean,
  wheelGesturesEnabled: t.boolean,
  sensitivity: t.number,
  showTrail: t.boolean,
  showLabel: t.boolean,
  trailColor: t.string,
  trailWidth: t.number,
  contextMenu: ContextMenuCodec,
  actions: t.array(GestureActionCodec),
  rockerActions: RockerActionsCodec,
});

const MouseGestureConfigOptional = t.partial({
  enabled: t.boolean,
});

export const MouseGestureConfigCodec = t.intersection([
  MouseGestureConfigRequired,
  MouseGestureConfigOptional,
]);
export type MouseGestureConfig = t.TypeOf<typeof MouseGestureConfigCodec>;

const MIN_CONTEXT_MENU_DISTANCE = 5;

const clamp = (value: number, min: number, max: number) =>
  Math.min(Math.max(value, min), max);

const BASE_DEFAULT_CONFIG: MouseGestureConfig = {
  enabled: false,
  rockerGesturesEnabled: true,
  wheelGesturesEnabled: true,
  sensitivity: 40,
  showTrail: true,
  showLabel: true,
  trailColor: "#37ff00",
  trailWidth: 6,
  contextMenu: {
    minDistance: MIN_CONTEXT_MENU_DISTANCE,
    preventionTimeout: 200,
  },
  actions: [
    { pattern: ["left"], action: "gecko-back" },
    { pattern: ["right"], action: "gecko-forward" },
    { pattern: ["up", "down"], action: "gecko-reload" },
    { pattern: ["down", "right"], action: "gecko-close-tab" },
    { pattern: ["down", "up"], action: "gecko-open-new-tab" },
    { pattern: ["up"], action: "gecko-scroll-to-top" },
    { pattern: ["down"], action: "gecko-scroll-to-bottom" },
    { pattern: ["left", "down"], action: "gecko-scroll-up" },
    { pattern: ["right", "down"], action: "gecko-scroll-down" },
  ],
  rockerActions: {
    leftRight: "gecko-forward",
    rightLeft: "gecko-back",
  },
};

const normalizeConfig = (config: Record<string, unknown>): MouseGestureConfig => {
  const sensitivity = clamp(
    Number.isFinite(config?.sensitivity)
      ? (config.sensitivity as number)
      : BASE_DEFAULT_CONFIG.sensitivity,
    1,
    100,
  );

  const contextMenu = {
    ...BASE_DEFAULT_CONFIG.contextMenu,
    ...(config?.contextMenu as Record<string, unknown> | undefined),
    minDistance: Math.max(
      ((config?.contextMenu as Record<string, unknown> | undefined)
        ?.minDistance as number) ?? BASE_DEFAULT_CONFIG.contextMenu.minDistance,
      MIN_CONTEXT_MENU_DISTANCE,
    ),
  };

  const rockerActions = {
    ...BASE_DEFAULT_CONFIG.rockerActions,
    ...(config?.rockerActions as Record<string, unknown> | undefined),
  };

  return {
    ...BASE_DEFAULT_CONFIG,
    ...config,
    sensitivity,
    contextMenu,
    actions: Array.isArray(config?.actions)
      ? (config.actions as GestureAction[])
      : BASE_DEFAULT_CONFIG.actions,
    rockerActions,
  } as MouseGestureConfig;
};

export const defaultConfig = normalizeConfig(BASE_DEFAULT_CONFIG);
export const strDefaultConfig = JSON.stringify(defaultConfig);

function createEnabled(): Signal<boolean> {
  const enabled = signal(
    Services.prefs.getBoolPref(MOUSE_GESTURE_ENABLED_PREF, defaultConfig.enabled ?? false),
  );

  addDisposer(effect(() => {
    Services.prefs.setBoolPref(MOUSE_GESTURE_ENABLED_PREF, enabled.value);
  }));

  const enabledObserver = () => {
    enabled.value = Services.prefs.getBoolPref(
      MOUSE_GESTURE_ENABLED_PREF,
      defaultConfig.enabled ?? false,
    );
  };

  Services.prefs.addObserver(MOUSE_GESTURE_ENABLED_PREF, enabledObserver);
  addDisposer(() => {
    Services.prefs.removeObserver(MOUSE_GESTURE_ENABLED_PREF, enabledObserver);
  });

  return enabled;
}

function createConfig(): Signal<MouseGestureConfig> {
  const parseConfig = (jsonStr: string): MouseGestureConfig => {
    try {
      const parsed = JSON.parse(jsonStr);
      const result = MouseGestureConfigCodec.decode(parsed);
      if (isRight(result)) {
        return normalizeConfig(result.right);
      }
      console.warn("Mouse gesture config validation failed, recovering partial data");
      return normalizeConfig(parsed);
    } catch (e) {
      console.error("Failed to parse mouse gesture config, using default", e);
      return defaultConfig;
    }
  };

  const config = signal<MouseGestureConfig>(
    parseConfig(
      Services.prefs.getStringPref(MOUSE_GESTURE_CONFIG_PREF, strDefaultConfig),
    ),
  );

  addDisposer(effect(() => {
    Services.prefs.setStringPref(MOUSE_GESTURE_CONFIG_PREF, JSON.stringify(config.value));
  }));

  const configObserver = () => {
    try {
      config.value = parseConfig(
        Services.prefs.getStringPref(MOUSE_GESTURE_CONFIG_PREF, strDefaultConfig),
      );
    } catch (e) {
      console.error("Failed to parse mouse gesture config:", e);
      config.value = defaultConfig;
      Services.prefs.setStringPref(MOUSE_GESTURE_CONFIG_PREF, strDefaultConfig);
    }
  };

  Services.prefs.addObserver(MOUSE_GESTURE_CONFIG_PREF, configObserver);
  addDisposer(() => {
    Services.prefs.removeObserver(MOUSE_GESTURE_CONFIG_PREF, configObserver);
  });

  return config;
}

export const _enabled: Signal<boolean> = createRootHMR(createEnabled, import.meta.hot);
export const _setEnabled = (v: boolean): void => {
  _enabled.value = v;
};

export const _config: Signal<MouseGestureConfig> = createRootHMR(createConfig, import.meta.hot);
export const _setConfig = (v: MouseGestureConfig): void => {
  _config.value = v;
};

export const isEnabled = () => _enabled.value;
export const setEnabled = (value: boolean) => { _enabled.value = value; };
export const getConfig = () => _config.value;
export const setConfig = (value: MouseGestureConfig) => { _config.value = normalizeConfig(value); };

export function patternToString(pattern: GesturePattern): string {
  return pattern.join("-");
}

export function stringToPattern(str: string): GesturePattern {
  return str.split("-") as GesturePattern;
}
