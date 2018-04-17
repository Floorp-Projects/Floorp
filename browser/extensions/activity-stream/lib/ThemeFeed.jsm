/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.import("resource://gre/modules/LightweightThemeManager.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

const {actionCreators: ac, actionTypes: at} = ChromeUtils.import("resource://activity-stream/common/Actions.jsm", {});

const THEME_UPDATE_EVENT = "lightweight-theme-styling-update";

this.ThemeFeed = class ThemeFeed {
  init() {
    Services.obs.addObserver(this, THEME_UPDATE_EVENT);
    this.updateTheme(LightweightThemeManager.currentThemeForDisplay);
  }

  uninit() {
    Services.obs.removeObserver(this, THEME_UPDATE_EVENT);
  }

  observe(subject, topic, data) {
    if (topic === THEME_UPDATE_EVENT) {
      this.updateTheme(JSON.parse(data));
    }
  }

  updateTheme(data) {
    if (data && data.window) {
      // We only update newtab theme if the theme activated isn't window specific.
      // We'll be able to do better in the future: see Bug 1444459
      return;
    }

    // If the theme is the built-in Dark theme, then activate our dark theme.
    const isDarkTheme = data && data.id === "firefox-compact-dark@mozilla.org";
    const className = isDarkTheme ? "dark-theme" : "";
    this.store.dispatch(ac.BroadcastToContent({type: at.THEME_UPDATE, data: {className}}));
  }

  onAction(action) {
    switch (action.type) {
      case at.INIT:
        this.init();
        break;
      case at.UNINIT:
        this.uninit();
        break;
    }
  }
};

const EXPORTED_SYMBOLS = ["ThemeFeed", "THEME_UPDATE_EVENT"];
