/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  resolveTabbarStyleFromNative,
  type TabbarStyle,
  VERTICAL_TABS_PREF,
} from "./tabbar-style-sync.ts";

/**
 * Mirror native `sidebar.verticalTabs` changes into the Floorp design config.
 *
 * `getStyle` reads the current Floorp style and `applyStyle` writes a new one;
 * `applyStyle` is only called when the resolved style actually differs. Returns
 * the teardown function for the observer.
 */
export function addVerticalTabsObserver(
  getStyle: () => TabbarStyle,
  applyStyle: (style: TabbarStyle) => void,
): () => void {
  const onPrefChange = () => {
    try {
      if (
        Services.prefs.getPrefType(VERTICAL_TABS_PREF) !==
          Services.prefs.PREF_BOOL
      ) {
        return;
      }
      const isVertical = Services.prefs.getBoolPref(VERTICAL_TABS_PREF, false);
      const current = getStyle();
      const next = resolveTabbarStyleFromNative(isVertical, current);
      if (next !== current) {
        applyStyle(next);
      }
    } catch (e) {
      console.error(
        "[Designs]",
        "Failed to sync the tab bar style from the native vertical tabs pref:",
        e,
      );
    }
  };

  Services.prefs.addObserver(VERTICAL_TABS_PREF, onPrefChange);

  return () => {
    try {
      Services.prefs.removeObserver(VERTICAL_TABS_PREF, onPrefChange);
    } catch (e) {
      console.error(
        "[Designs]",
        "Failed to remove the vertical tabs observer:",
        e,
      );
    }
  };
}
