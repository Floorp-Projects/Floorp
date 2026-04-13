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
import {
  type SplitViewConfig,
  type SplitViewPaneSizes,
  DEFAULT_CONFIG,
  DEFAULT_PANE_SIZES,
  PREF_SPLIT_VIEW_CONFIG,
  PREF_SPLIT_VIEW_PANE_SIZES,
} from "./types.js";

function deepEquals(a: unknown, b: unknown): boolean {
  return JSON.stringify(a) === JSON.stringify(b);
}

const VALID_LAYOUTS = new Set([
  "horizontal",
  "vertical",
  "grid-2x2",
  "grid-3pane-left-main",
  "grid-3pane-right-main",
  "grid-3pane-top-main",
  "grid-3pane-bottom-main",
]);

function clampRatio(n: unknown): number {
  return typeof n === "number" && Number.isFinite(n)
    ? Math.max(0, Math.min(1, n))
    : 0.5;
}

function parseConfigPref(prefName: string): SplitViewConfig {
  try {
    const raw = JSON.parse(
      Services.prefs.getStringPref(prefName, JSON.stringify(DEFAULT_CONFIG)),
    );
    if (
      typeof raw !== "object" ||
      raw === null ||
      !VALID_LAYOUTS.has(raw.layout) ||
      typeof raw.maxPanes !== "number" ||
      !Number.isInteger(raw.maxPanes) ||
      raw.maxPanes < 2 ||
      raw.maxPanes > 4
    ) {
      return DEFAULT_CONFIG;
    }
    return { layout: raw.layout, maxPanes: raw.maxPanes };
  } catch {
    return DEFAULT_CONFIG;
  }
}

function parsePaneSizesPref(prefName: string): SplitViewPaneSizes {
  try {
    const raw = JSON.parse(
      Services.prefs.getStringPref(
        prefName,
        JSON.stringify(DEFAULT_PANE_SIZES),
      ),
    );
    if (typeof raw !== "object" || raw === null) {
      return DEFAULT_PANE_SIZES;
    }
    return {
      flexRatios:
        Array.isArray(raw.flexRatios) &&
        raw.flexRatios.every((v: unknown) => typeof v === "number")
          ? raw.flexRatios.map(clampRatio)
          : DEFAULT_PANE_SIZES.flexRatios,
      gridColRatio: clampRatio(raw.gridColRatio),
      gridRowRatio: clampRatio(raw.gridRowRatio),
    };
  } catch {
    return DEFAULT_PANE_SIZES;
  }
}

function createSplitViewConfig(): [
  Accessor<SplitViewConfig>,
  Setter<SplitViewConfig>,
] {
  const [config, setConfig] = createSignal<SplitViewConfig>(
    parseConfigPref(PREF_SPLIT_VIEW_CONFIG),
    { equals: deepEquals },
  );

  createEffect(() => {
    Services.prefs.setStringPref(
      PREF_SPLIT_VIEW_CONFIG,
      JSON.stringify(config()),
    );
  });

  const observer = () => {
    setConfig(parseConfigPref(PREF_SPLIT_VIEW_CONFIG));
  };

  Services.prefs.addObserver(PREF_SPLIT_VIEW_CONFIG, observer);
  onCleanup(() => {
    Services.prefs.removeObserver(PREF_SPLIT_VIEW_CONFIG, observer);
  });

  return [config, setConfig];
}

function createSplitViewPaneSizes(): [
  Accessor<SplitViewPaneSizes>,
  Setter<SplitViewPaneSizes>,
] {
  const [paneSizes, setPaneSizes] = createSignal<SplitViewPaneSizes>(
    parsePaneSizesPref(PREF_SPLIT_VIEW_PANE_SIZES),
    { equals: deepEquals },
  );

  createEffect(() => {
    Services.prefs.setStringPref(
      PREF_SPLIT_VIEW_PANE_SIZES,
      JSON.stringify(paneSizes()),
    );
  });

  const observer = () => {
    setPaneSizes(parsePaneSizesPref(PREF_SPLIT_VIEW_PANE_SIZES));
  };

  Services.prefs.addObserver(PREF_SPLIT_VIEW_PANE_SIZES, observer);
  onCleanup(() => {
    Services.prefs.removeObserver(PREF_SPLIT_VIEW_PANE_SIZES, observer);
  });

  return [paneSizes, setPaneSizes];
}

export const [splitViewConfig, setSplitViewConfig] = createRootHMR(
  createSplitViewConfig,
  import.meta.hot,
);

export const [splitViewPaneSizes, setSplitViewPaneSizes] = createRootHMR(
  createSplitViewPaneSizes,
  import.meta.hot,
);
