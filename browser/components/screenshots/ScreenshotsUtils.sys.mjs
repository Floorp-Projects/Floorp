/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { getFilename } from "chrome://browser/content/screenshots/fileHelpers.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const SCREENSHOTS_LAST_SCREENSHOT_METHOD_PREF =
  "screenshots.browser.component.last-screenshot-method";
const SCREENSHOTS_LAST_SAVED_METHOD_PREF =
  "screenshots.browser.component.last-saved-method";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Downloads: "resource://gre/modules/Downloads.sys.mjs",
  FileUtils: "resource://gre/modules/FileUtils.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
});

XPCOMUtils.defineLazyServiceGetters(lazy, {
  AlertsService: ["@mozilla.org/alerts-service;1", "nsIAlertsService"],
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "SCREENSHOTS_LAST_SAVED_METHOD",
  SCREENSHOTS_LAST_SAVED_METHOD_PREF,
  "download"
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "SCREENSHOTS_LAST_SCREENSHOT_METHOD",
  SCREENSHOTS_LAST_SCREENSHOT_METHOD_PREF,
  "visible"
);

ChromeUtils.defineLazyGetter(lazy, "screenshotsLocalization", () => {
  return new Localization(["browser/screenshots.ftl"], true);
});

// The max dimension for a canvas is 32,767 https://searchfox.org/mozilla-central/rev/f40d29a11f2eb4685256b59934e637012ea6fb78/gfx/cairo/cairo/src/cairo-image-surface.c#62.
// The max number of pixels for a canvas is 472,907,776 pixels (i.e., 22,528 x 20,992) https://developer.mozilla.org/en-US/docs/Web/HTML/Element/canvas#maximum_canvas_size
// We have to limit screenshots to these dimensions otherwise it will cause an error.
export const MAX_CAPTURE_DIMENSION = 32766;
export const MAX_CAPTURE_AREA = 472907776;
export const MAX_SNAPSHOT_DIMENSION = 1024;

export class ScreenshotsComponentParent extends JSWindowActorParent {
  async receiveMessage(message) {
    let region, title;
    let browser = message.target.browsingContext.topFrameElement;
    // ignore message from child actors with no associated browser element
    if (!browser) {
      return;
    }
    if (
      ScreenshotsUtils.getUIPhase(browser) == UIPhases.CLOSED &&
      !ScreenshotsUtils.browserToScreenshotsState.has(browser)
    ) {
      // We've already exited or never opened and there's no UI or state that could
      // handle this message. We additionally check for screenshot-state to ensure we
      // don't ignore an overlay message when there is no current selection - which
      // otherwise looks like the UIPhases.CLOSED state.
      return;
    }

    switch (message.name) {
      case "Screenshots:CancelScreenshot": {
        let { reason } = message.data;
        ScreenshotsUtils.cancel(browser, reason);
        break;
      }
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
          overlayState: message.data.overlayState,
        });
        break;
      case "Screenshots:ShowPanel":
        ScreenshotsUtils.openPanel(browser);
        break;
      case "Screenshots:HidePanel":
        ScreenshotsUtils.closePanel(browser);
        break;
      case "Screenshots:MoveFocusToParent":
        ScreenshotsUtils.focusPanel(browser, message.data);
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

export class ScreenshotsHelperParent extends JSWindowActorParent {
  receiveMessage(message) {
    switch (message.name) {
      case "ScreenshotsHelper:GetElementRectFromPoint": {
        let cxt = BrowsingContext.get(message.data.bcId);
        return cxt.currentWindowGlobal
          .getActor("ScreenshotsHelper")
          .sendQuery("ScreenshotsHelper:GetElementRectFromPoint", message.data);
      }
    }
    return null;
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
    if (buttonsPanel && !buttonsPanel.hidden) {
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
    switch (event.type) {
      case "keydown":
        this.handleKeyDownEvent(event);
        break;
      case "TabSelect":
        this.handleTabSelect(event);
        break;
      case "SwapDocShells":
        this.handleDocShellSwapEvent(event);
        break;
      case "EndSwapDocShells":
        this.handleEndDocShellSwapEvent(event);
        break;
    }
  },

  handleKeyDownEvent(event) {
    let browser =
      event.view.browsingContext.topChromeWindow.gBrowser.selectedBrowser;
    if (!browser) {
      return;
    }

    switch (event.key) {
      case "Escape":
        // The chromeEventHandler in the child actor will handle events that
        // don't match this
        if (event.target.parentElement === this.panelForBrowser(browser)) {
          this.cancel(browser, "escape");
        }
        break;
      case "ArrowLeft":
      case "ArrowUp":
      case "ArrowRight":
      case "ArrowDown":
        this.handleArrowKeyDown(event, browser);
        break;
      case "Tab":
        this.maybeLockFocus(event);
        break;
    }
  },

  /**
   * When we swap docshells for a given screenshots browser, we need to update
   * the browserToScreenshotsState WeakMap to the correct browser. If the old
   * browser is in a state other than OVERLAYSELECTION, we will close
   * screenshots.
   *
   * @param {Event} event The SwapDocShells event
   */
  handleDocShellSwapEvent(event) {
    let oldBrowser = event.target;
    let newBrowser = event.detail;

    const currentUIPhase = this.getUIPhase(oldBrowser);
    if (currentUIPhase === UIPhases.OVERLAYSELECTION) {
      newBrowser.addEventListener("SwapDocShells", this);
      newBrowser.addEventListener("EndSwapDocShells", this);
      oldBrowser.removeEventListener("SwapDocShells", this);

      let perBrowserState =
        this.browserToScreenshotsState.get(oldBrowser) || {};
      this.browserToScreenshotsState.set(newBrowser, perBrowserState);
      this.browserToScreenshotsState.delete(oldBrowser);

      this.getActor(oldBrowser).sendAsyncMessage(
        "Screenshots:RemoveEventListeners"
      );
    } else {
      this.cancel(oldBrowser, "navigation");
    }
  },

  /**
   * When we swap docshells for a given screenshots browser, we need to add the
   * event listeners to the new browser because we removed event listeners in
   * handleDocShellSwapEvent.
   *
   * We attach the overlay event listeners to this.docShell.chromeEventHandler
   * in ScreenshotsComponentChild.sys.mjs which is the browser when the page is
   * loaded via the parent process (about:config, about:robots, etc) and when
   * this is the case, we lose the event listeners on the original browser.
   * To fix this, we remove the event listeners on the old browser and add the
   * event listeners to the new browser when a SwapDocShells occurs.
   *
   * @param {Event} event The EndSwapDocShells event
   */
  handleEndDocShellSwapEvent(event) {
    let browser = event.target;
    this.getActor(browser).sendAsyncMessage("Screenshots:AddEventListeners");
    browser.removeEventListener("EndSwapDocShells", this);
  },

  /**
   * When we receive a TabSelect event, we will close screenshots in the
   * previous tab if the previous tab was in the initial state.
   *
   * @param {Event} event The TabSelect event
   */
  handleTabSelect(event) {
    let previousTab = event.detail.previousTab;
    if (this.getUIPhase(previousTab.linkedBrowser) === UIPhases.INITIAL) {
      this.cancel(previousTab.linkedBrowser, "navigation");
    }
  },

  /**
   * If the overlay state is crosshairs or dragging, move the native cursor
   * respective to the arrow key pressed.
   * @param {Event} event A keydown event
   * @param {Browser} browser The selected browser
   * @returns
   */
  handleArrowKeyDown(event, browser) {
    // Wayland doesn't support `sendNativeMouseEvent` so just return
    if (Services.appinfo.isWayland) {
      return;
    }

    let { overlayState } = this.browserToScreenshotsState.get(browser);

    if (!["crosshairs", "dragging"].includes(overlayState)) {
      return;
    }

    let left = 0;
    let top = 0;
    let exponent = event.shiftKey ? 1 : 0;
    switch (event.key) {
      case "ArrowLeft":
        left -= 10 ** exponent;
        break;
      case "ArrowUp":
        top -= 10 ** exponent;
        break;
      case "ArrowRight":
        left += 10 ** exponent;
        break;
      case "ArrowDown":
        top += 10 ** exponent;
        break;
      default:
        return;
    }

    // Clear and move focus to browser so the child actor can capture events
    this.clearContentFocus(browser);
    Services.focus.clearFocus(browser.ownerGlobal);
    Services.focus.setFocus(browser, 0);

    let x = {};
    let y = {};
    let win = browser.ownerGlobal;
    win.windowUtils.getLastOverWindowPointerLocationInCSSPixels(x, y);

    this.moveCursor(
      {
        left: (x.value + left) * win.devicePixelRatio,
        top: (y.value + top) * win.devicePixelRatio,
      },
      browser
    );
  },

  /**
   * Move the native cursor to the given position. Clamp the position to the
   * window just in case.
   * @param {Object} position An object containing the left and top position
   * @param {Browser} browser The selected browser
   */
  moveCursor(position, browser) {
    let { left, top } = position;
    let win = browser.ownerGlobal;

    const windowLeft = win.mozInnerScreenX * win.devicePixelRatio;
    const windowTop = win.mozInnerScreenY * win.devicePixelRatio;
    const contentTop =
      (win.mozInnerScreenY + (win.innerHeight - browser.clientHeight)) *
      win.devicePixelRatio;
    const windowRight =
      (win.mozInnerScreenX + win.innerWidth) * win.devicePixelRatio;
    const windowBottom =
      (win.mozInnerScreenY + win.innerHeight) * win.devicePixelRatio;

    left += windowLeft;
    top += windowTop;

    // Clamp left and top to content dimensions
    let parsedLeft = Math.round(
      Math.min(Math.max(left, windowLeft), windowRight)
    );
    let parsedTop = Math.round(
      Math.min(Math.max(top, contentTop), windowBottom)
    );

    win.windowUtils.sendNativeMouseEvent(
      parsedLeft,
      parsedTop,
      win.windowUtils.NATIVE_MOUSE_MESSAGE_MOVE,
      0,
      0,
      win.document.documentElement
    );
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
   *
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
      case UIPhases.CLOSED: {
        this.captureFocusedElement(browser, "previousFocusRef");
        this.showPanelAndOverlay(browser, reason);
        browser.addEventListener("SwapDocShells", this);
        let gBrowser = browser.getTabBrowser();
        gBrowser.tabContainer.addEventListener("TabSelect", this);
        browser.ownerDocument.addEventListener("keydown", this);
        break;
      }
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
    this.captureFocusedElement(browser, "currentFocusRef");
    this.closeDialogBox(browser);
    this.closePanel(browser);
    this.closeOverlay(browser);
    this.resetMethodsUsed();
    this.attemptToRestoreFocus(browser);

    browser.removeEventListener("SwapDocShells", this);
    const gBrowser = browser.getTabBrowser();
    gBrowser.tabContainer.removeEventListener("TabSelect", this);
    browser.ownerDocument.removeEventListener("keydown", this);

    this.browserToScreenshotsState.delete(browser);
    if (Cu.isInAutomation) {
      Services.obs.notifyObservers(null, "screenshots-exit");
    }
  },

  /**
   * Cancel/abort the screenshots operation for the given browser
   *
   * @param browser The current browser.
   */
  cancel(browser, reason) {
    this.recordTelemetryEvent("canceled", reason, {});
    this.exit(browser);
  },

  /**
   * Update internal UI state associated with the given browser
   *
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

  maybeLockFocus(event) {
    let browser = event.view.gBrowser.selectedBrowser;

    if (!Services.focus.focusedElement) {
      event.preventDefault();
      this.focusPanel(browser);
      return;
    }

    let target = event.explicitOriginalTarget;

    if (!target.closest("moz-button-group")) {
      return;
    }

    let isElementFirst = !!target.nextElementSibling;

    if (
      (isElementFirst && event.shiftKey) ||
      (!isElementFirst && !event.shiftKey)
    ) {
      event.preventDefault();
      this.moveFocusToContent(browser);
    }
  },

  focusPanel(browser, { direction } = {}) {
    let buttonsPanel = this.panelForBrowser(browser);
    if (direction) {
      buttonsPanel
        .querySelector("screenshots-buttons")
        .focusButton(direction === "forward" ? "first" : "last");
    } else {
      buttonsPanel
        .querySelector("screenshots-buttons")
        .focusButton(lazy.SCREENSHOTS_LAST_SCREENSHOT_METHOD);
    }
  },

  moveFocusToContent(browser) {
    this.getActor(browser).sendAsyncMessage("Screenshots:MoveFocusToContent");
  },

  clearContentFocus(browser) {
    this.getActor(browser).sendAsyncMessage("Screenshots:ClearFocus");
  },

  /**
   * Attempt to place focus on the element that had focus before screenshots UI was shown
   *
   * @param browser The current browser.
   */
  attemptToRestoreFocus(browser) {
    const document = browser.ownerDocument;
    const window = browser.ownerGlobal;

    const doFocus = () => {
      // Move focus it back to where it was previously.
      prevFocus.setAttribute("refocused-by-panel", true);
      try {
        let fm = Services.focus;
        fm.setFocus(prevFocus, fm.FLAG_NOSCROLL);
      } catch (e) {
        prevFocus.focus();
      }
      prevFocus.removeAttribute("refocused-by-panel");
      let focusedElement;
      try {
        focusedElement = document.commandDispatcher.focusedElement;
        if (!focusedElement) {
          focusedElement = document.activeElement;
        }
      } catch (ex) {
        focusedElement = document.activeElement;
      }
    };

    let perBrowserState = this.browserToScreenshotsState.get(browser) || {};
    let prevFocus = perBrowserState.previousFocusRef?.get();
    let currentFocus = perBrowserState.currentFocusRef?.get();
    delete perBrowserState.currentFocusRef;

    // Avoid changing focus if focus changed during exit - perhaps exit was caused
    // by a user action which resulted in focus moving
    let nowFocus;
    try {
      nowFocus = document.commandDispatcher.focusedElement;
    } catch (e) {
      nowFocus = document.activeElement;
    }
    if (nowFocus && nowFocus != currentFocus) {
      return;
    }

    let dialog = this.getDialog(browser);
    let panel = this.panelForBrowser(browser);

    if (prevFocus) {
      // Try to restore focus
      try {
        if (document.commandDispatcher.focusedWindow != window) {
          // Focus has already been set to a different window
          return;
        }
      } catch (ex) {}

      if (!currentFocus) {
        doFocus();
        return;
      }
      while (currentFocus) {
        if (
          (dialog && currentFocus == dialog) ||
          (panel && currentFocus == panel) ||
          currentFocus == browser
        ) {
          doFocus();
          return;
        }
        currentFocus = currentFocus.parentNode;
        if (
          currentFocus &&
          currentFocus.nodeType == currentFocus.DOCUMENT_FRAGMENT_NODE &&
          currentFocus.host
        ) {
          // focus was in a shadowRoot, we'll try the host",
          currentFocus = currentFocus.host;
        }
      }
      doFocus();
    }
  },

  /**
   * Set a flag so we don't try to exit when preview dialog next closes.
   *
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
   *
   * @param browser The current browser
   */
  async openPreviewDialog(browser) {
    let dialogBox = browser.ownerGlobal.gBrowser.getTabDialogBox(browser);
    let { dialog, closedPromise } = await dialogBox.open(
      `chrome://browser/content/screenshots/screenshots-preview.html?browsingContextId=${browser.browsingContext.id}`,
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
   * Take a weak-reference to whatever element currently has focus and associate it with
   * the UI state for this browser.
   *
   * @param browser The current browser.
   * @param {string} stateRefName The property name for this element reference.
   */
  captureFocusedElement(browser, stateRefName) {
    let document = browser.ownerDocument;
    let focusedElement;
    try {
      focusedElement = document.commandDispatcher.focusedElement;
      if (!focusedElement) {
        focusedElement = document.activeElement;
      }
    } catch (ex) {
      focusedElement = document.activeElement;
    }
    this.setPerBrowserState(browser, {
      [stateRefName]: Cu.getWeakReference(focusedElement),
    });
  },

  /**
   * Returns the buttons panel for the given browser if the panel exists.
   * Otherwise creates the buttons panel and returns the buttons panel.
   * @param browser The current browser
   * @returns The buttons panel
   */
  panelForBrowser(browser) {
    let buttonsPanel = browser.ownerDocument.getElementById(
      "screenshotsPagePanel"
    );
    if (!buttonsPanel) {
      let doc = browser.ownerDocument;
      let template = doc.getElementById("screenshotsPagePanelTemplate");
      let fragmentClone = template.content.cloneNode(true);
      buttonsPanel = fragmentClone.firstElementChild;
      template.replaceWith(buttonsPanel);

      let anchor = browser.ownerDocument.querySelector("#navigator-toolbox");
      anchor.appendChild(buttonsPanel);
    }

    return (
      buttonsPanel ??
      browser.ownerDocument.getElementById("screenshotsPagePanel")
    );
  },

  /**
   * Open the buttons panel.
   * @param browser The current browser
   */
  openPanel(browser) {
    let buttonsPanel = this.panelForBrowser(browser);
    if (!buttonsPanel.hidden) {
      return null;
    }
    buttonsPanel.hidden = false;

    return new Promise(resolve => {
      browser.ownerGlobal.requestAnimationFrame(() => {
        buttonsPanel
          .querySelector("screenshots-buttons")
          .focusButton(lazy.SCREENSHOTS_LAST_SCREENSHOT_METHOD);
        resolve();
      });
    });
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
    buttonsPanel.hidden = true;
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
    this.recordTelemetryEvent("started", data, {});
    this.openPanel(browser);
  },

  /**
   * Close the overlay UI, and clear out internal state if there was an overlay selection
   * The overlay lives in the child document; so although closing is actually async, we assume success.
   * @param browser The current browser.
   */
  closeOverlay(browser, options = {}) {
    // If the actor has been unregistered (e.g. if the component enabled pref is flipped false)
    // its possible getActor will throw an exception. That's ok.
    let actor;
    try {
      actor = this.getActor(browser);
    } catch (ex) {}
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
      let manager = currTabDialogBox.getTabDialogManager();
      let dialogs = manager.hasDialogs && manager.dialogs;
      if (dialogs.length) {
        for (let dialog of dialogs) {
          if (
            dialog._openedURL.endsWith(
              `browsingContextId=${browserContextId}`
            ) &&
            dialog._openedURL.includes("screenshots-preview.html")
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
    if (
      !anchor ||
      !anchor.isConnected ||
      !window.isElementVisible(anchor.parentNode)
    ) {
      // Use the hamburger button if the screenshots button isn't available
      anchor = browser.ownerDocument.getElementById("PanelUI-menu-button");
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
    let screenshotsPreviewEl = dialog._frame.contentDocument.querySelector(
      "screenshots-preview"
    );

    screenshotsPreviewEl.focusButton(lazy.SCREENSHOTS_LAST_SAVED_METHOD);

    let rect;
    let lastUsedMethod;
    if (type === "full_page") {
      rect = await this.fetchFullPageBounds(browser);
      lastUsedMethod = "fullpage";
    } else {
      rect = await this.fetchVisibleBounds(browser);
      lastUsedMethod = "visible";
    }

    Services.prefs.setStringPref(
      SCREENSHOTS_LAST_SCREENSHOT_METHOD_PREF,
      lastUsedMethod
    );
    this.methodsUsed[lastUsedMethod] += 1;
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

    let url = canvas.toDataURL();
    let screenshotsPreviewEl = dialog._frame.contentDocument.querySelector(
      "screenshots-preview"
    );

    screenshotsPreviewEl.previewImg.src = url;

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
    region.left = Math.round(region.left);
    region.right = Math.round(region.right);
    region.top = Math.round(region.top);
    region.bottom = Math.round(region.bottom);
    region.width = Math.round(region.right - region.left);
    region.height = Math.round(region.bottom - region.top);

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

    const snapshotSize = Math.floor(MAX_SNAPSHOT_DIMENSION * devicePixelRatio);

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

        // The `left` and `top` need to be a multiple of the `snapshotSize` to
        // prevent gaps/lines from appearing in the screenshot.
        // If devicePixelRatio is 0.3, snapshotSize would be 307 after flooring
        // from 307.2. Therefore every fifth snapshot would have a start of
        // 307.2 * 5 or 1536 which is not a multiple of 307 and would cause a
        // gap/line in the snapshot.
        let left = Math.floor((startLeft - region.left) * devicePixelRatio);
        let top = Math.floor((startTop - region.top) * devicePixelRatio);
        context.drawImage(
          snapshot,
          left - (left % snapshotSize),
          top - (top % snapshotSize),
          Math.floor(width * devicePixelRatio),
          Math.floor(height * devicePixelRatio)
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
    // Internal consumers expect the image data to be stored as a
    // nsIInputStream. On Linux and Windows, pasted data is directly
    // retrieved from the system's native clipboard, and made available
    // as a nsIInputStream.
    //
    // On macOS, nsClipboard::GetNativeClipboardData (nsClipboard.mm) uses
    // a cached copy of nsITransferable if available, e.g. when the copy
    // was initiated by the same browser instance. To make sure that a
    // nsIInputStream is returned instead of the cached imgIContainer,
    // the image is exported as as `kNativeImageMime`. Data associated
    // with this type is converted to a platform-specific image format
    // when written to the clipboard. The type is not used when images
    // are read from the clipboard (on all platforms, not just macOS).
    // This forces nsClipboard::GetNativeClipboardData to fall back to
    // the native clipboard, and return the image as a nsITransferable.
    transferable.addDataFlavor("application/x-moz-nativeimage");
    transferable.setTransferData("application/x-moz-nativeimage", imgDecoded);

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

    Services.prefs.setStringPref(SCREENSHOTS_LAST_SAVED_METHOD_PREF, "copy");
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
   * @returns true if the download succeeds, otherwise false
   */
  async downloadScreenshot(title, dataUrl, browser, data) {
    // Guard against missing image data.
    if (!dataUrl) {
      return false;
    }

    let { filename, accepted } = await getFilename(title, browser);

    if (!accepted) {
      return false;
    }

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
    } catch (ex) {
      console.error(
        `Failed to create download using filename: ${filename} (length: ${
          new Blob([filename]).size
        })`
      );

      return false;
    }

    let extra = await this.getActor(browser).sendQuery(
      "Screenshots:GetMethodsUsed"
    );
    this.recordTelemetryEvent("download", data.object, {
      ...extra,
      ...this.methodsUsed,
    });
    this.resetMethodsUsed();

    Services.prefs.setStringPref(
      SCREENSHOTS_LAST_SAVED_METHOD_PREF,
      "download"
    );

    return true;
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
