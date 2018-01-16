/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["Toolbars"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");

this.Toolbars = {
  init(libDir) {},

  configurations: {
    onlyNavBar: {
      selectors: ["#navigator-toolbox"],
      async applyConfig() {
        let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
        let personalToolbar = browserWindow.document.getElementById("PersonalToolbar");
        browserWindow.setToolbarVisibility(personalToolbar, false);
        toggleMenubarIfNecessary(false);
      },
    },

    allToolbars: {
      selectors: ["#navigator-toolbox"],
      async applyConfig() { // Boookmarks and menubar
        let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
        let personalToolbar = browserWindow.document.getElementById("PersonalToolbar");
        browserWindow.setToolbarVisibility(personalToolbar, true);
        toggleMenubarIfNecessary(true);
      },

      async verifyConfig() {
        let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
        if (browserWindow.fullScreen) {
          return "The bookmark toolbar and menubar are not shown in fullscreen.";
        }
        return undefined;
      },
    },

  },
};


// helpers

function toggleMenubarIfNecessary(visible) {
  let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
  // The menubar is not shown on OS X or while in fullScreen
  if (Services.appinfo.OS != "Darwin" /* && !browserWindow.fullScreen*/) {
    let menubar = browserWindow.document.getElementById("toolbar-menubar");
    browserWindow.setToolbarVisibility(menubar, visible);
  }
}
