/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ScreenshotsUtils"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const PanelPosition = "bottomright topright";
const PanelOffsetX = -33;
const PanelOffsetY = -8;

var ScreenshotsUtils = {
  initialize() {
    Services.obs.addObserver(this, "menuitem-screenshot");
    Services.obs.addObserver(this, "screenshots-take-screenshot");
  },
  observe(subj, topic, data) {
    let { gBrowser } = subj;
    let browser = gBrowser.selectedBrowser;

    let currDialogBox = browser.tabDialogBox;

    switch (topic) {
      case "menuitem-screenshot":
        // if dialog box exists then find the correct dialog box and close it
        if (currDialogBox) {
          let manager = currDialogBox.getTabDialogManager();
          let dialogs = manager.hasDialogs && manager.dialogs;
          if (dialogs.length) {
            for (let dialog of dialogs) {
              if (
                dialog._openedURL.endsWith(
                  `browsingContextId=${browser.browsingContext.id}`
                ) &&
                dialog._openedURL.includes("screenshots.html")
              ) {
                dialog.close();
                return null;
              }
            }
          }
        }
        // only toggle the buttons if no dialog box is found because
        // if dialog box is found then the buttons are hidden and we return early
        // else no dialog box is found and we need to toggle the buttons
        this.togglePreview(browser);
        break;
      case "screenshots-take-screenshot":
        // need to toggle because take visible button was clicked
        // and we need to hide the buttons
        this.togglePreview(browser);

        // init UI as a tab dialog box
        let dialogBox = gBrowser.getTabDialogBox(browser);

        return dialogBox.open(
          `chrome://browser/content/screenshots/screenshots.html?browsingContextId=${browser.browsingContext.id}`,
          {
            features: "resizable=no",
            sizeTo: "available",
            allowDuplicateDialogs: false,
          }
        );
    }
    return null;
  },
  notify(window, type) {
    if (Services.prefs.getBoolPref("screenshots.browser.component.enabled")) {
      Services.obs.notifyObservers(
        window.event.currentTarget.ownerGlobal,
        "menuitem-screenshot"
      );
    } else {
      Services.obs.notifyObservers(null, "menuitem-screenshot-extension", type);
    }
  },
  togglePreview(browser) {
    let buttonsPanel = browser.ownerDocument.querySelector(
      "#screenshotsPagePanel"
    );
    if (buttonsPanel && buttonsPanel.state !== "closed") {
      buttonsPanel.hidePopup();
    } else {
      this.createOrDisplayButtons(browser);
    }
  },
  createOrDisplayButtons(browser) {
    let doc = browser.ownerDocument;
    let buttonsPanel = doc.querySelector("#screenshotsPagePanel");
    if (!buttonsPanel) {
      let template = doc.querySelector("#screenshotsPagePanelTemplate");
      let clone = template.content.cloneNode(true);
      template.replaceWith(clone);
      buttonsPanel = doc.querySelector("#screenshotsPagePanel");
    }
    let anchor = doc.querySelector("#navigator-toolbox");
    buttonsPanel.openPopup(anchor, PanelPosition, PanelOffsetX, PanelOffsetY);
  },
};
