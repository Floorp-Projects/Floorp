/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { TabScroll } from "./scroll";
import { TabOpenPosition } from "./openPosition";
import { TabSizeSpecification } from "./sizeSpecification";
import { TabDoubleClickClose } from "./doubleClickClose";
import { TabPinnedTabCustomization } from "./pinnedTabCustomization";
import { noraComponent, NoraComponentBase } from "#features-chrome/utils/base";

@noraComponent(import.meta.hot)
export default class Tab extends NoraComponentBase {
  init() {
    new TabScroll();
    new TabOpenPosition();
    new TabSizeSpecification();
    new TabDoubleClickClose();
    new TabPinnedTabCustomization();
    this.checkAndShowWelcomePage();
  }

  async checkAndShowWelcomePage(): Promise<void> {
    await SessionStore.promiseInitialized;
    try {
      // Show upgrade guide only for users who upgraded from Floorp 11
      const isFromVersion11 = Services.prefs.getBoolPref(
        "floorp.browser.isVersion11",
        false,
      );

      if (isFromVersion11) {
        const window = Services.wm.getMostRecentWindow(
          "navigator:browser",
        ) as Window;
        const gBrowser = window.gBrowser;
        if (gBrowser) {
          // Open welcome page with upgrade parameter
          const t = gBrowser.addTrustedTab("about:welcome?upgrade=12");
          gBrowser.selectedTab = t;

          // Reset the flag so it does not show again
          Services.prefs.setBoolPref("floorp.browser.isVersion11", false);
        }
      }
    } catch (error) {
      console.error("Failed to show welcome page:", error);
    }
  }
}
