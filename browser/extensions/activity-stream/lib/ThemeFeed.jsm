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

  _parseRGB(colorString) {
    let rgb = colorString.match(/^rgba?\((\d+), (\d+), (\d+)/);
    rgb.shift();
    return rgb.map(x => parseInt(x, 10));
  }

  updateTheme(data) {
    if (data && data.window) {
      // We only update newtab theme if the theme activated isn't window specific.
      // We'll be able to do better in the future: see Bug 1444459
      return;
    }

    // Jump through some hoops to check if the current theme has light or dark
    // text. If light, then we enable our dark (background) theme.
    const textcolor = (data && data.textcolor) || "black";
    const window = Services.wm.getMostRecentWindow("navigator:browser");
    const dummy = window.document.createElement("dummy");
    dummy.style.color = textcolor;
    const [r, g, b] = this._parseRGB(window.getComputedStyle(dummy).color);
    const luminance = 0.2125 * r + 0.7154 * g + 0.0721 * b;
    const className = (luminance <= 110) ? "" : "dark-theme";
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
