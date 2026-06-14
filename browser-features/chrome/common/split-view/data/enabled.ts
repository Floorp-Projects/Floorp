/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { type Accessor, createSignal, onCleanup, type Setter } from "solid-js";
import { createRootHMR } from "@nora/solid-xul";
import {
  PREF_BROWSER_SPLIT_VIEW_ENABLED,
  PREF_SPLIT_VIEW_ENABLED,
} from "./types.js";

function readEnabledFromPrefs(): boolean {
  const floorpEnabled = Services.prefs.getBoolPref(
    PREF_SPLIT_VIEW_ENABLED,
    true,
  );
  const browserEnabled = Services.prefs.getBoolPref(
    PREF_BROWSER_SPLIT_VIEW_ENABLED,
    false,
  );
  return floorpEnabled && browserEnabled;
}

function createSplitViewEnabled(): [Accessor<boolean>, Setter<boolean>] {
  const [enabled, setEnabled] = createSignal(readEnabledFromPrefs());

  const observer = () => {
    setEnabled(readEnabledFromPrefs());
  };

  Services.prefs.addObserver(PREF_SPLIT_VIEW_ENABLED, observer);
  Services.prefs.addObserver(PREF_BROWSER_SPLIT_VIEW_ENABLED, observer);
  onCleanup(() => {
    Services.prefs.removeObserver(PREF_SPLIT_VIEW_ENABLED, observer);
    Services.prefs.removeObserver(PREF_BROWSER_SPLIT_VIEW_ENABLED, observer);
  });

  return [enabled, setEnabled];
}

export const [_splitViewEnabled, _setSplitViewEnabled] = createRootHMR(
  createSplitViewEnabled,
  import.meta.hot,
);

/** Both floorp and browser split-view prefs must be true. */
export function isSplitViewEnabled(): boolean {
  return readEnabledFromPrefs();
}

/** Reactive accessor for split-view enabled state. */
export function splitViewEnabled(): boolean {
  return _splitViewEnabled();
}

/** Sync both enabled prefs (user-facing toggle). */
export function setSplitViewEnabled(value: boolean): void {
  Services.prefs.setBoolPref(PREF_SPLIT_VIEW_ENABLED, value);
  Services.prefs.setBoolPref(PREF_BROWSER_SPLIT_VIEW_ENABLED, value);
}
