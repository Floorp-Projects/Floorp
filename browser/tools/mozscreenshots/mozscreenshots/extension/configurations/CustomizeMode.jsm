/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["CustomizeMode"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Timer.jsm");

this.CustomizeMode = {

  init(libDir) {},

  configurations: {
    notCustomizing: {
      applyConfig() {
        return new Promise((resolve) => {
          let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
          if (!browserWindow.document.documentElement.hasAttribute("customizing")) {
            resolve("notCustomizing: already not customizing");
            return;
          }
          function onCustomizationEnds() {
            browserWindow.gNavToolbox.removeEventListener("aftercustomization",
                                                          onCustomizationEnds);
            // Wait for final changes
            setTimeout(() => resolve("notCustomizing: onCustomizationEnds"), 500);
          }
          browserWindow.gNavToolbox.addEventListener("aftercustomization",
                                                     onCustomizationEnds);
          browserWindow.gCustomizeMode.exit();
        });
      },
    },

    customizing: {
      applyConfig() {
        return new Promise((resolve) => {
          let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
          if (browserWindow.document.documentElement.hasAttribute("customizing")) {
            resolve("customizing: already customizing");
            return;
          }
          function onCustomizing() {
            browserWindow.gNavToolbox.removeEventListener("customizationready",
                                                          onCustomizing);
            // Wait for final changes
            setTimeout(() => resolve("customizing: onCustomizing"), 500);
          }
          browserWindow.gNavToolbox.addEventListener("customizationready",
                                                     onCustomizing);
          browserWindow.gCustomizeMode.enter();
        });
      },
    },
  },
};
