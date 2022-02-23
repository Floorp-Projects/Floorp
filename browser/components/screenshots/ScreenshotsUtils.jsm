/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ScreenshotsUtils", "ScreenshotsComponentParent"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const PanelPosition = "bottomright topright";
const PanelOffsetX = -33;
const PanelOffsetY = -8;

class ScreenshotsComponentParent extends JSWindowActorParent {
  receiveMessage(message) {
    switch (message.name) {
      case "Screenshots:CancelScreenshot":
        let browser = message.target.browsingContext.topFrameElement;
        ScreenshotsUtils.closePanel(browser);
    }
  }
}

var ScreenshotsUtils = {
  initialized: false,
  initialize() {
    if (!this.initialized) {
      if (
        !Services.prefs.getBoolPref(
          "screenshots.browser.component.enabled",
          false
        )
      ) {
        return;
      }
      Services.obs.addObserver(this, "menuitem-screenshot");
      Services.obs.addObserver(this, "screenshots-take-screenshot");
      this.initialized = true;
      if (Cu.isInAutomation) {
        Services.obs.notifyObservers(null, "screenshots-component-initialized");
      }
    }
  },
  uninitialize() {
    if (this.initialized) {
      Services.obs.removeObserver(this, "menuitem-screenshot");
      Services.obs.removeObserver(this, "screenshots-take-screenshot");
      this.initialized = false;
    }
  },
  observe(subj, topic, data) {
    let { gBrowser } = subj;
    let browser = gBrowser.selectedBrowser;

    let zoom = subj.ZoomManager.getZoomForBrowser(browser);

    switch (topic) {
      case "menuitem-screenshot":
        let success = this.closeDialogBox(browser);
        if (!success || data === "retry") {
          // only toggle the buttons if no dialog box is found because
          // if dialog box is found then the buttons are hidden and we return early
          // else no dialog box is found and we need to toggle the buttons
          // or if retry because the dialog box was closed and we need to show the panel
          this.togglePreview(browser);
        }
        break;
      case "screenshots-take-screenshot":
        // need to close the preview because screenshot was taken
        this.closePanel(browser);

        // init UI as a tab dialog box
        let dialogBox = gBrowser.getTabDialogBox(browser);

        let { dialog } = dialogBox.open(
          `chrome://browser/content/screenshots/screenshots.html?browsingContextId=${browser.browsingContext.id}`,
          {
            features: "resizable=no",
            sizeTo: "available",
            allowDuplicateDialogs: false,
          }
        );
        this.doScreenshot(browser, dialog, zoom, data);
    }
    return null;
  },
  /**
   * Notify screenshots when screenshot command is used.
   * @param window The current window the screenshot command was used.
   * @param type The type of screenshot taken. Used for telemetry.
   */
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
  /**
   * Creates and returns a Screenshots actor.
   * @param browser The current browser.
   * @returns JSWindowActor The screenshot actor.
   */
  getActor(browser) {
    let actor = browser.browsingContext.currentWindowGlobal.getActor(
      "ScreenshotsComponent"
    );
    return actor;
  },
  /**
   * Open the panel buttons and call child actor to open the overlay
   * @param browser The current browser
   */
  openPanel(browser) {
    let actor = this.getActor(browser);
    actor.sendQuery("Screenshots:ShowOverlay");
    this.createOrDisplayButtons(browser);
  },
  /**
   * Close the panel and call child actor to close the overlay
   * @param browser The current browser
   */
  closePanel(browser) {
    let buttonsPanel = browser.ownerDocument.querySelector(
      "#screenshotsPagePanel"
    );
    if (buttonsPanel && buttonsPanel.state !== "closed") {
      buttonsPanel.hidePopup();
    }
    let actor = this.getActor(browser);
    actor.sendQuery("Screenshots:HideOverlay");
  },
  /**
   * If the buttons panel exists and is open we will hide both the panel
   * popup and the overlay.
   * Otherwise create or display the buttons.
   * @param browser The current browser.
   */
  togglePreview(browser) {
    let buttonsPanel = browser.ownerDocument.querySelector(
      "#screenshotsPagePanel"
    );
    if (buttonsPanel && buttonsPanel.state !== "closed") {
      buttonsPanel.hidePopup();
      let actor = this.getActor(browser);
      return actor.sendQuery("Screenshots:HideOverlay");
    }
    let actor = this.getActor(browser);
    actor.sendQuery("Screenshots:ShowOverlay");
    return this.createOrDisplayButtons(browser);
  },
  /**
   * Gets the screenshots dialog box
   * @param browser The selected browser
   * @returns Screenshots dialog box if it exists otherwise null
   */
  getDialog(browser) {
    let currTabDialogBox = browser.tabDialogBox;
    let browserContextId = browser.browsingContext.id;
    if (currTabDialogBox) {
      currTabDialogBox.getTabDialogManager();
      let manager = currTabDialogBox.getTabDialogManager();
      let dialogs = manager.hasDialogs && manager.dialogs;
      if (dialogs.length) {
        for (let dialog of dialogs) {
          if (
            dialog._openedURL.endsWith(
              `browsingContextId=${browserContextId}`
            ) &&
            dialog._openedURL.includes("screenshots.html")
          ) {
            return dialog;
          }
        }
      }
    }
    return null;
  },
  /**
   * Closes the dialog box it it exists
   * @param browser The selected browser
   */
  closeDialogBox(browser) {
    let dialog = this.getDialog(browser);
    if (dialog) {
      dialog.close();
      return true;
    }
    return false;
  },
  /**
   * If the buttons panel does not exist then we will replace the buttons
   * panel template with the buttons panel then open the buttons panel and
   * show the screenshots overaly.
   * @param browser The current browser.
   */
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
  /**
   * Gets the full page bounds from the screenshots child actor.
   * @param browser The current browser.
   * @returns { object }
   *    Contains the full page bounds from the screenshots child actor.
   */
  fetchFullPageBounds(browser) {
    let actor = this.getActor(browser);
    return actor.sendQuery("Screenshots:getFullPageBounds");
  },
  /**
   * Gets the visible bounds from the screenshots child actor.
   * @param browser The current browser.
   * @returns { object }
   *    Contains the visible bounds from the screenshots child actor.
   */
  fetchVisibleBounds(browser) {
    let actor = this.getActor(browser);
    return actor.sendQuery("Screenshots:getVisibleBounds");
  },
  /**
   * Add screenshot-ui to the dialog box and then take the screenshot
   * @param browser The current browser.
   * @param dialog The dialog box to show the screenshot preview.
   * @param zoom The current zoom level.
   * @param type The type of screenshot taken.
   */
  async doScreenshot(browser, dialog, zoom, type) {
    await dialog._dialogReady;
    let screenshotsUI = dialog._frame.contentDocument.createElement(
      "screenshots-ui"
    );
    dialog._frame.contentDocument.body.appendChild(screenshotsUI);

    let rect;
    if (type === "full-page") {
      ({ rect } = await this.fetchFullPageBounds(browser));
    } else {
      ({ rect } = await this.fetchVisibleBounds(browser));
    }
    return this.takeScreenshot(browser, dialog, rect, zoom);
  },
  /**
   * Take the screenshot and add the image to the dialog box
   * @param browser The current browser.
   * @param dialog The dialog box to show the screenshot preview.
   * @param rect DOMRect containing bounds of the screenshot.
   * @param zoom The current zoom level.
   */
  async takeScreenshot(browser, dialog, rect, zoom) {
    let browsingContext = BrowsingContext.get(browser.browsingContext.id);

    let snapshot = await browsingContext.currentWindowGlobal.drawSnapshot(
      rect,
      zoom,
      "rgb(255,255,255)"
    );

    let canvas = dialog._frame.contentDocument.createElementNS(
      "http://www.w3.org/1999/xhtml",
      "html:canvas"
    );
    let context = canvas.getContext("2d");

    canvas.width = snapshot.width;
    canvas.height = snapshot.height;

    context.drawImage(snapshot, 0, 0);

    canvas.toBlob(function(blob) {
      let newImg = dialog._frame.contentDocument.createElement("img");
      let url = URL.createObjectURL(blob);

      newImg.id = "placeholder-image";

      newImg.src = url;
      dialog._frame.contentDocument
        .getElementById("preview-image-div")
        .appendChild(newImg);

      if (Cu.isInAutomation) {
        Services.obs.notifyObservers(null, "screenshots-preview-ready");
      }
    });

    snapshot.close();
  },
};
