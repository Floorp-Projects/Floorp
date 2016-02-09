/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["LightweightThemes"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/LightweightThemeManager.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Timer.jsm");

this.LightweightThemes = {
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
      applyConfig: Task.async(function*() {
        LightweightThemeManager.currentTheme = null;
      }),
    },

    darkLWT: {
      applyConfig() {
        LightweightThemeManager.setLocalTheme({
          id:          "black",
          name:        "black",
          headerURL:   LightweightThemes._blackImageURL,
          footerURL:   LightweightThemes._blackImageURL,
          textcolor:   "#eeeeee",
          accentcolor: "#111111",
        });

        // Wait for LWT listener
        return new Promise(resolve => {
          setTimeout(() => {
            resolve("darkLWT");
          }, 500);
        });
      },

      verifyConfig: verifyConfigHelper,
    },

    lightLWT: {
      applyConfig() {
        LightweightThemeManager.setLocalTheme({
          id:          "white",
          name:        "white",
          headerURL:   LightweightThemes._whiteImageURL,
          footerURL:   LightweightThemes._whiteImageURL,
          textcolor:   "#111111",
          accentcolor: "#eeeeee",
        });
        // Wait for LWT listener
        return new Promise(resolve => {
          setTimeout(() => {
            resolve("lightLWT");
          }, 500);
        });
      },

      verifyConfig: verifyConfigHelper,
    },

  },
};


function verifyConfigHelper() {
  return new Promise((resolve, reject) => {
    let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
    if (browserWindow.document.documentElement.hasAttribute("lwtheme")) {
      resolve("verifyConfigHelper");
    } else {
      reject("The @lwtheme attribute wasn't present so themes may not be available");
    }
  });
}
