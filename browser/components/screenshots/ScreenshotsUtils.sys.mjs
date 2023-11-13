/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { getFilename } from "chrome://browser/content/screenshots/fileHelpers.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Downloads: "resource://gre/modules/Downloads.sys.mjs",
  FileUtils: "resource://gre/modules/FileUtils.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
});

XPCOMUtils.defineLazyServiceGetters(lazy, {
  AlertsService: ["@mozilla.org/alerts-service;1", "nsIAlertsService"],
});

ChromeUtils.defineLazyGetter(lazy, "screenshotsLocalization", () => {
  return new Localization(["browser/screenshots.ftl"], true);
});

const PanelPosition = "bottomright topright";
const PanelOffsetX = -33;
const PanelOffsetY = -8;
// The max dimension for a canvas is 32,767 https://searchfox.org/mozilla-central/rev/f40d29a11f2eb4685256b59934e637012ea6fb78/gfx/cairo/cairo/src/cairo-image-surface.c#62.
// The max number of pixels for a canvas is 472,907,776 pixels (i.e., 22,528 x 20,992) https://developer.mozilla.org/en-US/docs/Web/HTML/Element/canvas#maximum_canvas_size
// We have to limit screenshots to these dimensions otherwise it will cause an error.
export const MAX_CAPTURE_DIMENSION = 32766;
export const MAX_CAPTURE_AREA = 472907776;
export const MAX_SNAPSHOT_DIMENSION = 1024;

export class ScreenshotsComponentParent extends JSWindowActorParent {
  async receiveMessage(message) {
    let browser = message.target.browsingContext.topFrameElement;
    let region, title;
    switch (message.name) {
      case "Screenshots:CancelScreenshot":
        let { reason } = message.data;
        ScreenshotsUtils.cancel(browser, reason);
        break;
      case "Screenshots:CopyScreenshot":
        ScreenshotsUtils.closePanel(browser);
        ({ region } = message.data);
        await ScreenshotsUtils.copyScreenshotFromRegion(region, browser);
        ScreenshotsUtils.exit(browser);
        break;
      case "Screenshots:DownloadScreenshot":
        ScreenshotsUtils.closePanel(browser);
        ({ title, region } = message.data);
        await ScreenshotsUtils.downloadScreenshotFromRegion(
          title,
          region,
          browser
        );
        ScreenshotsUtils.exit(browser);
        break;
      case "Screenshots:OverlaySelection":
        ScreenshotsUtils.setPerBrowserState(browser, {
          hasOverlaySelection: message.data.hasSelection,
        });
        break;
      case "Screenshots:ShowPanel":
        ScreenshotsUtils.openPanel(browser);
        break;
      case "Screenshots:HidePanel":
        ScreenshotsUtils.closePanel(browser);
        break;
    }
  }

  didDestroy() {
    // When restoring a crashed tab the browser is null
    let browser = this.browsingContext.topFrameElement;
    if (browser) {
      ScreenshotsUtils.exit(browser);
    }
  }
}

export const UIPhases = {
  CLOSED: 0, // nothing showing
  INITIAL: 1, // panel and overlay showing
  OVERLAYSELECTION: 2, // something selected in the overlay
  PREVIEW: 3, // preview dialog showing
};

export var ScreenshotsUtils = {
  browserToScreenshotsState: new WeakMap(),
  initialized: false,
  methodsUsed: {},

  /**
   * Figures out which of various states the screenshots UI is in, for the given browser.
   * @param browser The selected browser
   * @returns One of the `UIPhases` constants
   */
  getUIPhase(browser) {
    let perBrowserState = this.browserToScreenshotsState.get(browser);
    if (perBrowserState?.previewDialog) {
      return UIPhases.PREVIEW;
    }
    const buttonsPanel = this.panelForBrowser(browser);
    if (buttonsPanel && buttonsPanel.state !== "closed") {
      return UIPhases.INITIAL;
    }
    if (perBrowserState?.hasOverlaySelection) {
      return UIPhases.OVERLAYSELECTION;
    }
    return UIPhases.CLOSED;
  },

  resetMethodsUsed() {
    this.methodsUsed = { fullpage: 0, visible: 0 };
  },

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
      this.resetMethodsUsed();
      Services.telemetry.setEventRecordingEnabled("screenshots", true);
      Services.obs.addObserver(this, "menuitem-screenshot");
      this.initialized = true;
      if (Cu.isInAutomation) {
        Services.obs.notifyObservers(null, "screenshots-component-initialized");
      }
    }
  },

  uninitialize() {
    if (this.initialized) {
      Services.obs.removeObserver(this, "menuitem-screenshot");
      this.initialized = false;
    }
  },

  handleEvent(event) {
    // Escape should cancel and exit
    if (event.type === "keydown" && event.key === "Escape") {
      let browser = event.view.gBrowser.selectedBrowser;
      this.cancel(browser, "escape");
    }
  },

  observe(subj, topic, data) {
    let { gBrowser } = subj;
    let browser = gBrowser.selectedBrowser;

    switch (topic) {
      case "menuitem-screenshot": {
        const uiPhase = this.getUIPhase(browser);
        if (uiPhase !== UIPhases.CLOSED) {
          // toggle from already-open to closed
          this.cancel(browser, data);
          return;
        }
        this.start(browser, data);
        break;
      }
    }
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
        "menuitem-screenshot",
        type
      );
    } else {
      Services.obs.notifyObservers(null, "menuitem-screenshot-extension", type);
    }
  },

  /**
   * Creates/gets and returns a Screenshots actor.
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
   * Show the Screenshots UI and start the capture flow
   * @param browser The current browser.
   * @param reason [string] Optional reason string passed along when recording telemetry events
   */
  start(browser, reason = "") {
    const uiPhase = this.getUIPhase(browser);
    switch (uiPhase) {
      case UIPhases.CLOSED:
        this.showPanelAndOverlay(browser, reason);
        break;
      case UIPhases.INITIAL:
        // nothing to do, panel & overlay are already open
        break;
      case UIPhases.PREVIEW: {
        this.closeDialogBox(browser);
        this.showPanelAndOverlay(browser, reason);
        break;
      }
    }
  },

  /**
   * Exit the Screenshots UI for the given browser
   * Closes any of open UI elements (preview dialog, panel, overlay) and cleans up internal state.
   * @param browser The current browser.
   */
  exit(browser) {
    this.closeDialogBox(browser);
    this.closePanel(browser);
    this.closeOverlay(browser);
    this.resetMethodsUsed();
    this.browserToScreenshotsState.delete(browser);
    if (Cu.isInAutomation) {
      Services.obs.notifyObservers(null, "screenshots-exit");
    }
  },

  /**
   * Cancel/abort the screenshots operation for the given browser
   * @param browser The current browser.
   */
  cancel(browser, reason) {
    this.recordTelemetryEvent("canceled", reason, {});
    this.exit(browser);
  },

  /**
   * Update internal UI state associated with the given browser
   * @param browser The current browser.
   * @param nameValues {object} An object with one or more named property values
   */
  setPerBrowserState(browser, nameValues = {}) {
    if (!this.browserToScreenshotsState.has(browser)) {
      // we should really have this state already, created when the preview dialog was opened
      this.browserToScreenshotsState.set(browser, {});
    }
    let perBrowserState = this.browserToScreenshotsState.get(browser);
    Object.assign(perBrowserState, nameValues);
  },

  /**
   * Set a flag so we don't try to exit when preview dialog next closes.
   * @param browser The current browser.
   * @param reason [string] Optional reason string passed along when recording telemetry events
   */
  scheduleRetry(browser, reason) {
    let perBrowserState = this.browserToScreenshotsState.get(browser);
    if (!perBrowserState?.closedPromise) {
      console.warn(
        "Expected perBrowserState with a closedPromise for the preview dialog"
      );
      return;
    }
    this.setPerBrowserState(browser, { exitOnPreviewClose: false });
    perBrowserState?.closedPromise.then(() => {
      this.start(browser, reason);
    });
  },

  /**
   * Open the tab dialog for preview
   * @param browser The current browser
   */
  async openPreviewDialog(browser) {
    let dialogBox = browser.ownerGlobal.gBrowser.getTabDialogBox(browser);
    let { dialog, closedPromise } = await dialogBox.open(
      `chrome://browser/content/screenshots/screenshots.html?browsingContextId=${browser.browsingContext.id}`,
      {
        features: "resizable=no",
        sizeTo: "available",
        allowDuplicateDialogs: false,
      },
      browser
    );
    this.setPerBrowserState(browser, {
      previewDialog: dialog,
      exitOnPreviewClose: true,
      closedPromise: closedPromise.finally(() => {
        this.onDialogClose(browser);
      }),
    });
    return dialog;
  },

  /**
   * Returns the buttons panel for the given browser
   * @param browser The current browser
   * @returns The buttons panel
   */
  panelForBrowser(browser) {
    return browser.ownerDocument.getElementById("screenshotsPagePanel");
  },

  /**
   * Create the buttons container from its template, for this browser
   * @param browser The current browser
   * @returns The buttons panel
   */
  createPanelForBrowser(browser) {
    let buttonsPanel = this.panelForBrowser(browser);
    if (!buttonsPanel) {
      let doc = browser.ownerDocument;
      let template = doc.getElementById("screenshotsPagePanelTemplate");
      let clone = template.content.cloneNode(true);
      template.replaceWith(clone);
    }
    return this.panelForBrowser(browser);
  },

  /**
   * Open the buttons panel.
   * @param browser The current browser
   */
  async openPanel(browser) {
    let buttonsPanel = this.panelForBrowser(browser);
    if (buttonsPanel.state !== "closed") {
      return;
    }

    buttonsPanel.ownerDocument.addEventListener("keydown", this);
    let anchor = browser.ownerDocument.getElementById("navigator-toolbox");
    buttonsPanel.openPopup(anchor, PanelPosition, PanelOffsetX, PanelOffsetY);

    if (buttonsPanel.state !== "open") {
      await new Promise(resolve => {
        buttonsPanel.addEventListener("popupshown", resolve, { once: true });
      });
    }
    buttonsPanel
      .querySelector("screenshots-buttons")
      .focusFirst({ focusVisible: true });
  },

  /**
   * Close the panel
   * @param browser The current browser
   */
  closePanel(browser) {
    let buttonsPanel = this.panelForBrowser(browser);
    if (!buttonsPanel) {
      return;
    }
    if (buttonsPanel.state !== "closed") {
      buttonsPanel.hidePopup();
    }
    buttonsPanel.ownerDocument.removeEventListener("keydown", this);
  },

  /**
   * If the buttons panel exists and is open we will hide both the panel
   * and the overlay. If the overlay is showing, we will hide the overlay.
   * Otherwise create or display the buttons.
   * @param browser The current browser.
   */
  async showPanelAndOverlay(browser, data) {
    let actor = this.getActor(browser);
    actor.sendAsyncMessage("Screenshots:ShowOverlay");
    this.createPanelForBrowser(browser);
    this.recordTelemetryEvent("started", data, {});
    await this.openPanel(browser);
  },

  /**
   * Close the overlay UI, and clear out internal state if there was an overlay selection
   * The overlay lives in the child document; so although closing is actually async, we assume success.
   * @param browser The current browser.
   */
  closeOverlay(browser, options = {}) {
    let actor = this.getActor(browser);
    actor?.sendAsyncMessage("Screenshots:HideOverlay", options);

    if (this.browserToScreenshotsState.has(browser)) {
      this.setPerBrowserState(browser, {
        hasOverlaySelection: false,
      });
    }
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
    let perBrowserState = this.browserToScreenshotsState.get(browser);
    if (perBrowserState?.previewDialog) {
      perBrowserState.previewDialog.close();
      return true;
    }
    return false;
  },

  /**
   * Callback fired when the preview dialog window closes
   * Will exit the screenshots UI if the `exitOnPreviewClose` flag is set for this browser
   * @param browser The associated browser
   */
  onDialogClose(browser) {
    let perBrowserState = this.browserToScreenshotsState.get(browser);
    if (!perBrowserState) {
      return;
    }
    delete perBrowserState.previewDialog;
    if (perBrowserState?.exitOnPreviewClose) {
      this.exit(browser);
    }
  },

  /**
   * Gets the screenshots button if it is visible, otherwise it will get the
   * element that the screenshots button is nested under. If the screenshots
   * button doesn't exist then we will default to the navigator toolbox.
   * @param browser The selected browser
   * @returns The anchor element for the ConfirmationHint
   */
  getWidgetAnchor(browser) {
    let window = browser.ownerGlobal;
    let widgetGroup = window.CustomizableUI.getWidget("screenshot-button");
    let widget = widgetGroup?.forWindow(window);
    let anchor = widget?.anchor;

    // Check if the anchor exists and is visible
    if (!anchor || !window.isElementVisible(anchor.parentNode)) {
      anchor = browser.ownerDocument.getElementById("navigator-toolbox");
    }
    return anchor;
  },

  /**
   * Indicate that the screenshot has been copied via ConfirmationHint.
   * @param browser The selected browser
   */
  showCopiedConfirmationHint(browser) {
    let anchor = this.getWidgetAnchor(browser);

    browser.ownerGlobal.ConfirmationHint.show(
      anchor,
      "confirmation-hint-screenshot-copied"
    );
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

  showAlertMessage(title, message) {
    lazy.AlertsService.showAlertNotification(null, title, message);
  },

  /**
   * The max dimension of any side of a canvas is 32767 and the max canvas area is
   * 124925329. If the width or height is greater or equal to 32766 we will crop the
   * screenshot to the max width. If the area is still too large for the canvas
   * we will adjust the height so we can successfully capture the screenshot.
   * @param {Object} rect The dimensions of the screenshot. The rect will be
   * modified in place
   */
  cropScreenshotRectIfNeeded(rect) {
    let cropped = false;
    let width = rect.width * rect.devicePixelRatio;
    let height = rect.height * rect.devicePixelRatio;

    if (width > MAX_CAPTURE_DIMENSION) {
      width = MAX_CAPTURE_DIMENSION;
      cropped = true;
    }
    if (height > MAX_CAPTURE_DIMENSION) {
      height = MAX_CAPTURE_DIMENSION;
      cropped = true;
    }
    if (width * height > MAX_CAPTURE_AREA) {
      height = Math.floor(MAX_CAPTURE_AREA / width);
      cropped = true;
    }

    rect.width = Math.floor(width / rect.devicePixelRatio);
    rect.height = Math.floor(height / rect.devicePixelRatio);
    rect.right = rect.left + rect.width;
    rect.bottom = rect.top + rect.height;

    if (cropped) {
      let [errorTitle, errorMessage] =
        lazy.screenshotsLocalization.formatMessagesSync([
          { id: "screenshots-too-large-error-title" },
          { id: "screenshots-too-large-error-details" },
        ]);
      this.showAlertMessage(errorTitle.value, errorMessage.value);
      this.recordTelemetryEvent("failed", "screenshot_too_large", null);
    }
  },

  /**
   * Open and add screenshot-ui to the dialog box and then take the screenshot
   * @param browser The current browser.
   * @param type The type of screenshot taken.
   */
  async doScreenshot(browser, type) {
    this.closePanel(browser);
    this.closeOverlay(browser, { doNotResetMethods: true });

    let dialog = await this.openPreviewDialog(browser);
    await dialog._dialogReady;
    let screenshotsUI =
      dialog._frame.contentDocument.createElement("screenshots-ui");
    dialog._frame.contentDocument.body.appendChild(screenshotsUI);

    let rect;
    if (type === "full_page") {
      rect = await this.fetchFullPageBounds(browser);
      this.methodsUsed.fullpage += 1;
    } else {
      rect = await this.fetchVisibleBounds(browser);
      this.methodsUsed.visible += 1;
    }
    this.recordTelemetryEvent("selected", type, {});
    return this.takeScreenshot(browser, dialog, rect);
  },

  /**
   * Take the screenshot and add the image to the dialog box
   * @param browser The current browser.
   * @param dialog The dialog box to show the screenshot preview.
   * @param rect DOMRect containing bounds of the screenshot.
   */
  async takeScreenshot(browser, dialog, rect) {
    let canvas = await this.createCanvas(rect, browser);

    let newImg = dialog._frame.contentDocument.createElement("img");
    let url = canvas.toDataURL();

    newImg.id = "placeholder-image";

    newImg.src = url;
    dialog._frame.contentDocument
      .getElementById("preview-image-div")
      .appendChild(newImg);

    if (Cu.isInAutomation) {
      Services.obs.notifyObservers(null, "screenshots-preview-ready");
    }
  },

  /**
   * Creates a canvas and draws a snapshot of the screenshot on the canvas
   * @param region The bounds of screenshots
   * @param browser The current browser
   * @returns The canvas
   */
  async createCanvas(region, browser) {
    this.cropScreenshotRectIfNeeded(region);

    let { devicePixelRatio } = region;

    let browsingContext = BrowsingContext.get(browser.browsingContext.id);

    let canvas = browser.ownerDocument.createElementNS(
      "http://www.w3.org/1999/xhtml",
      "html:canvas"
    );
    let context = canvas.getContext("2d");

    canvas.width = region.width * devicePixelRatio;
    canvas.height = region.height * devicePixelRatio;

    for (
      let startLeft = region.left;
      startLeft < region.right;
      startLeft += MAX_SNAPSHOT_DIMENSION
    ) {
      for (
        let startTop = region.top;
        startTop < region.bottom;
        startTop += MAX_SNAPSHOT_DIMENSION
      ) {
        let height =
          startTop + MAX_SNAPSHOT_DIMENSION > region.bottom
            ? region.bottom - startTop
            : MAX_SNAPSHOT_DIMENSION;
        let width =
          startLeft + MAX_SNAPSHOT_DIMENSION > region.right
            ? region.right - startLeft
            : MAX_SNAPSHOT_DIMENSION;
        let rect = new DOMRect(startLeft, startTop, width, height);

        let snapshot = await browsingContext.currentWindowGlobal.drawSnapshot(
          rect,
          devicePixelRatio,
          "rgb(255,255,255)"
        );

        context.drawImage(
          snapshot,
          (startLeft - region.left) * devicePixelRatio,
          (startTop - region.top) * devicePixelRatio,
          width * devicePixelRatio,
          height * devicePixelRatio
        );

        snapshot.close();
      }
    }

    return canvas;
  },

  /**
   * Copy the screenshot
   * @param region The bounds of the screenshots
   * @param browser The current browser
   */
  async copyScreenshotFromRegion(region, browser) {
    let canvas = await this.createCanvas(region, browser);
    let url = canvas.toDataURL();

    await this.copyScreenshot(url, browser, {
      object: "overlay_copy",
    });
  },

  /**
   * Copy the image to the clipboard
   * This is called from the preview dialog
   * @param dataUrl The image data
   * @param browser The current browser
   * @param data Telemetry data
   */
  async copyScreenshot(dataUrl, browser, data) {
    // Guard against missing image data.
    if (!dataUrl) {
      return;
    }

    const imageTools = Cc["@mozilla.org/image/tools;1"].getService(
      Ci.imgITools
    );

    const base64Data = dataUrl.replace("data:image/png;base64,", "");

    const image = atob(base64Data);
    const imgDecoded = imageTools.decodeImageFromBuffer(
      image,
      image.length,
      "image/png"
    );

    const transferable = Cc[
      "@mozilla.org/widget/transferable;1"
    ].createInstance(Ci.nsITransferable);
    transferable.init(null);
    transferable.addDataFlavor("image/png");
    transferable.setTransferData("image/png", imgDecoded);

    Services.clipboard.setData(
      transferable,
      null,
      Services.clipboard.kGlobalClipboard
    );

    this.showCopiedConfirmationHint(browser);

    let extra = await this.getActor(browser).sendQuery(
      "Screenshots:GetMethodsUsed"
    );
    this.recordTelemetryEvent("copy", data.object, {
      ...extra,
      ...this.methodsUsed,
    });
    this.resetMethodsUsed();
  },

  /**
   * Download the screenshot
   * @param title The title of the current page
   * @param region The bounds of the screenshot
   * @param browser The current browser
   */
  async downloadScreenshotFromRegion(title, region, browser) {
    let canvas = await this.createCanvas(region, browser);
    let dataUrl = canvas.toDataURL();

    await this.downloadScreenshot(title, dataUrl, browser, {
      object: "overlay_download",
    });
  },

  /**
   * Download the screenshot
   * This is called from the preview dialog
   * @param title The title of the current page or null and getFilename will get the title
   * @param dataUrl The image data
   * @param browser The current browser
   * @param data Telemetry data
   */
  async downloadScreenshot(title, dataUrl, browser, data) {
    // Guard against missing image data.
    if (!dataUrl) {
      return;
    }

    let filename = await getFilename(title, browser);

    const targetFile = new lazy.FileUtils.File(filename);

    // Create download and track its progress.
    try {
      const download = await lazy.Downloads.createDownload({
        source: dataUrl,
        target: targetFile,
      });

      let isPrivate = lazy.PrivateBrowsingUtils.isWindowPrivate(
        browser.ownerGlobal
      );
      const list = await lazy.Downloads.getList(
        isPrivate ? lazy.Downloads.PRIVATE : lazy.Downloads.PUBLIC
      );
      // add the download to the download list in the Downloads list in the Browser UI
      list.add(download);

      // Await successful completion of the save via the download manager
      await download.start();
    } catch (ex) {}

    let extra = await this.getActor(browser).sendQuery(
      "Screenshots:GetMethodsUsed"
    );
    this.recordTelemetryEvent("download", data.object, {
      ...extra,
      ...this.methodsUsed,
    });
    this.resetMethodsUsed();
  },

  recordTelemetryEvent(type, object, args) {
    if (args) {
      for (let key of Object.keys(args)) {
        args[key] = args[key].toString();
      }
    }
    Services.telemetry.recordEvent("screenshots", type, object, null, args);
  },
};
