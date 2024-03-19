/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { setTimeout } from "resource://gre/modules/Timer.sys.mjs";
import { BrowserTestUtils } from "resource://testing-common/BrowserTestUtils.sys.mjs";

export var WindowSize = {
  init() {
    Services.prefs.setBoolPref("browser.fullscreen.autohide", false);
  },

  configurations: {
    maximized: {
      selectors: [":root"],
      async applyConfig() {
        let browserWindow =
          Services.wm.getMostRecentWindow("navigator:browser");
        await toggleFullScreen(browserWindow, false);

        // Wait for the Lion fullscreen transition to end as there doesn't seem to be an event
        // and trying to maximize while still leaving fullscreen doesn't work.
        await new Promise(resolve => {
          setTimeout(function waitToLeaveFS() {
            browserWindow.maximize();
            resolve();
          }, 5000);
        });
      },
    },

    normal: {
      selectors: [":root"],
      async applyConfig() {
        let browserWindow =
          Services.wm.getMostRecentWindow("navigator:browser");
        await toggleFullScreen(browserWindow, false);
        browserWindow.restore();
        await new Promise(resolve => {
          setTimeout(resolve, 5000);
        });
      },
    },

    fullScreen: {
      selectors: [":root"],
      async applyConfig() {
        let browserWindow =
          Services.wm.getMostRecentWindow("navigator:browser");
        await toggleFullScreen(browserWindow, true);
        // OS X Lion fullscreen transition takes a while
        await new Promise(resolve => {
          setTimeout(resolve, 5000);
        });
      },
    },
  },
};

function toggleFullScreen(browserWindow, wantsFS) {
  browserWindow.fullScreen = wantsFS;
  return BrowserTestUtils.waitForCondition(() => {
    return (
      wantsFS ==
      browserWindow.document.documentElement.hasAttribute("inFullscreen")
    );
  }, "waiting for @inFullscreen change");
}
