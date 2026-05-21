/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { signal, effect } from "@preact/signals";
import type { Signal } from "@preact/signals";
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

function makePrefSignal<T>(
  parsePref: () => T,
  prefName: string,
  equals: (a: T, b: T) => boolean,
): Signal<T> {
  const sig = signal<T>(parsePref());

  effect(() => {
    const v = sig.value;
    Services.prefs.setStringPref(prefName, JSON.stringify(v));
  });

  const observer = () => {
    const next = parsePref();
    if (!equals(sig.value, next)) sig.value = next;
  };
  Services.prefs.addObserver(prefName, observer);

  import.meta.hot?.dispose(() => {
    Services.prefs.removeObserver(prefName, observer);
  });

  return sig;
}

export const splitViewConfig: Signal<SplitViewConfig> = makePrefSignal(
  () => parseConfigPref(PREF_SPLIT_VIEW_CONFIG),
  PREF_SPLIT_VIEW_CONFIG,
  deepEquals,
);
export const setSplitViewConfig = (v: SplitViewConfig) => { splitViewConfig.value = v; };

export const splitViewPaneSizes: Signal<SplitViewPaneSizes> = makePrefSignal(
  () => parsePaneSizesPref(PREF_SPLIT_VIEW_PANE_SIZES),
  PREF_SPLIT_VIEW_PANE_SIZES,
  deepEquals,
);
export const setSplitViewPaneSizes = (v: SplitViewPaneSizes) => { splitViewPaneSizes.value = v; };
