/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["LightweightThemes"];

ChromeUtils.import("resource://gre/modules/LightweightThemeManager.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/Timer.jsm");

var LightweightThemes = {
  init(libDir) {
    // convert -size 3000x200 canvas:#333 black_theme.png
    let blackImage = libDir.clone();
    blackImage.append("black_theme.png");
    this._blackImageURL = Services.io.newFileURI(blackImage).spec;

    // convert -size 3000x200 canvas:#eee white_theme.png
    let whiteImage = libDir.clone();
    whiteImage.append("white_theme.png");
    this._whiteImageURL = Services.io.newFileURI(whiteImage).spec;
  },

  configurations: {
    noLWT: {
      selectors: [],
      async applyConfig() {
        LightweightThemeManager.currentTheme = null;
      },
    },

    darkLWT: {
      selectors: [],
      applyConfig() {
        LightweightThemeManager.setLocalTheme({
          id:          "black",
          name:        "black",
          headerURL:   LightweightThemes._blackImageURL,
          textcolor:   "#eeeeee",
          accentcolor: "#111111",
        });

        // Wait for LWT listener
        return new Promise(resolve => {
          setTimeout(() => {
            resolve();
          }, 500);
        });
      },
    },

    lightLWT: {
      selectors: [],
      applyConfig() {
        LightweightThemeManager.setLocalTheme({
          id:          "white",
          name:        "white",
          headerURL:   LightweightThemes._whiteImageURL,
          textcolor:   "#111111",
          accentcolor: "#eeeeee",
        });
        // Wait for LWT listener
        return new Promise(resolve => {
          setTimeout(() => {
            resolve();
          }, 500);
        });
      },
    },

    compactLight: {
      selectors: [],
      applyConfig() {
        LightweightThemeManager.currentTheme = LightweightThemeManager.getUsedTheme("firefox-compact-light@mozilla.org");
      },
    },

    compactDark: {
      selectors: [],
      applyConfig() {
        LightweightThemeManager.currentTheme = LightweightThemeManager.getUsedTheme("firefox-compact-dark@mozilla.org");
      },
    },
  },
};
