/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  type Accessor,
  createEffect,
  createSignal,
  onCleanup,
  type Setter,
} from "solid-js";
import { createRootHMR } from "@nora/solid-xul";
import * as t from "io-ts";
import { isRight } from "fp-ts/Either";
import { getActionDisplayName } from "./utils/gestures";

export const MOUSE_GESTURE_ENABLED_PREF = "floorp.mousegesture.enabled";
export const MOUSE_GESTURE_CONFIG_PREF = "floorp.mousegesture.config";

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

export const GestureActionCodec = t.type({
  pattern: t.array(GestureDirectionCodec),
  action: t.string,
});
export type GestureAction = t.TypeOf<typeof GestureActionCodec>;

const ContextMenuCodec = t.type({
  minDistance: t.number,
  preventionTimeout: t.number,
});

const MouseGestureConfigRequired = t.type({
  rockerGesturesEnabled: t.boolean,
  sensitivity: t.number,
  showTrail: t.boolean,
  showLabel: t.boolean,
  trailColor: t.string,
  trailWidth: t.number,
  contextMenu: ContextMenuCodec,
  actions: t.array(GestureActionCodec),
});

const MouseGestureConfigOptional = t.partial({
  enabled: t.boolean,
});

export const MouseGestureConfigCodec = t.intersection([
  MouseGestureConfigRequired,
  MouseGestureConfigOptional,
]);
export type MouseGestureConfig = t.TypeOf<typeof MouseGestureConfigCodec>;

export const defaultConfig: MouseGestureConfig = {
  enabled: false,
  rockerGesturesEnabled: true,
  sensitivity: 40,
  showTrail: true,
  showLabel: true,
  trailColor: "#37ff00",
  trailWidth: 6,
  contextMenu: {
    minDistance: 5,
    preventionTimeout: 200,
  },
  actions: [
    {
      pattern: ["left"],
      action: "gecko-back",
    },
    {
      pattern: ["right"],
      action: "gecko-forward",
    },
    {
      pattern: ["up", "down"],
      action: "gecko-reload",
    },
    {
      pattern: ["down", "right"],
      action: "gecko-close-tab",
    },
    {
      pattern: ["down", "up"],
      action: "gecko-open-new-tab",
    },
    {
      pattern: ["up"],
      action: "gecko-scroll-to-top",
    },
    {
      pattern: ["down"],
      action: "gecko-scroll-to-bottom",
    },
    {
      pattern: ["left", "down"],
      action: "gecko-scroll-up",
    },
    {
      pattern: ["right", "down"],
      action: "gecko-scroll-down",
    },
  ],
};

export const strDefaultConfig = JSON.stringify(defaultConfig);

function createEnabled(): [Accessor<boolean>, Setter<boolean>] {
  const [enabled, setEnabled] = createSignal(
    Services.prefs.getBoolPref(
      MOUSE_GESTURE_ENABLED_PREF,
      defaultConfig.enabled,
    ),
  );

  createEffect(() => {
    Services.prefs.setBoolPref(MOUSE_GESTURE_ENABLED_PREF, enabled());
  });

  const enabledObserver = () => {
    setEnabled(
      Services.prefs.getBoolPref(
        MOUSE_GESTURE_ENABLED_PREF,
        defaultConfig.enabled,
      ),
    );
  };

  Services.prefs.addObserver(MOUSE_GESTURE_ENABLED_PREF, enabledObserver);
  onCleanup(() => {
    Services.prefs.removeObserver(MOUSE_GESTURE_ENABLED_PREF, enabledObserver);
  });

  return [enabled, setEnabled];
}

function createConfig(): [
  Accessor<MouseGestureConfig>,
  Setter<MouseGestureConfig>,
] {
  const parseConfig = (jsonStr: string): MouseGestureConfig => {
    const parsed = JSON.parse(jsonStr);
    const result = MouseGestureConfigCodec.decode(parsed);
    if (isRight(result)) {
      return result.right;
    }
    console.error(
      "Failed to decode mouse gesture configuration, using default",
    );
    return defaultConfig;
  };

  const [config, setConfig] = createSignal<MouseGestureConfig>(
    parseConfig(
      Services.prefs.getStringPref(MOUSE_GESTURE_CONFIG_PREF, strDefaultConfig),
    ),
  );

  createEffect(() => {
    Services.prefs.setStringPref(
      MOUSE_GESTURE_CONFIG_PREF,
      JSON.stringify(config()),
    );
  });

  const configObserver = () => {
    try {
      setConfig(
        parseConfig(
          Services.prefs.getStringPref(
            MOUSE_GESTURE_CONFIG_PREF,
            strDefaultConfig,
          ),
        ),
      );
    } catch (e) {
      console.error("Failed to parse mouse gesture configuration:", e);
      setConfig(defaultConfig);
      Services.prefs.setStringPref(MOUSE_GESTURE_CONFIG_PREF, strDefaultConfig);
    }
  };

  Services.prefs.addObserver(MOUSE_GESTURE_CONFIG_PREF, configObserver);
  onCleanup(() => {
    Services.prefs.removeObserver(MOUSE_GESTURE_CONFIG_PREF, configObserver);
  });

  return [config, setConfig];
}

export const [_enabled, _setEnabled] = createRootHMR(
  createEnabled,
  import.meta.hot,
);
export const [_config, _setConfig] = createRootHMR(
  createConfig,
  import.meta.hot,
);

export const isEnabled = () => _enabled();
export const setEnabled = (value: boolean) => _setEnabled(value);
export const getConfig = () => _config();
export const setConfig = (value: MouseGestureConfig) => _setConfig(value);

export function patternToString(pattern: GesturePattern): string {
  return pattern.join("-");
}

export function stringToPattern(str: string): GesturePattern {
  return str.split("-") as GesturePattern;
}

export function getGestureActionName(gestureAction: GestureAction): string {
  return getActionDisplayName(gestureAction.action);
}
