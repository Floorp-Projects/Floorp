/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-env mozilla/browser-window */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  DeferredTask: "resource://gre/modules/DeferredTask.sys.mjs",
  ScreenshotsOverlay: "resource:///modules/ScreenshotsOverlayChild.sys.mjs",
});

export class ScreenshotsComponentChild extends JSWindowActorChild {
  #resizeTask;
  #scrollTask;
  #overlay;

  static OVERLAY_EVENTS = [
    "click",
    "pointerdown",
    "pointermove",
    "pointerup",
    "keyup",
    "keydown",
  ];

  get overlay() {
    return this.#overlay;
  }

  receiveMessage(message) {
    switch (message.name) {
      case "Screenshots:ShowOverlay":
        return this.startScreenshotsOverlay();
      case "Screenshots:HideOverlay":
        return this.endScreenshotsOverlay(message.data);
      case "Screenshots:isOverlayShowing":
        return this.overlay?.initialized;
      case "Screenshots:getFullPageBounds":
        return this.getFullPageBounds();
      case "Screenshots:getVisibleBounds":
        return this.getVisibleBounds();
      case "Screenshots:getDocumentTitle":
        return this.getDocumentTitle();
      case "Screenshots:GetMethodsUsed":
        return this.getMethodsUsed();
    }
    return null;
  }

  handleEvent(event) {
    switch (event.type) {
      case "click":
      case "pointerdown":
      case "pointermove":
      case "pointerup":
      case "keyup":
      case "keydown":
        if (!this.overlay?.initialized) {
          return;
        }
        this.overlay.handleEvent(event);
        break;
      case "beforeunload":
        this.requestCancelScreenshot("navigation");
        break;
      case "resize":
        if (!this.#resizeTask && this.overlay?.initialized) {
          this.#resizeTask = new lazy.DeferredTask(() => {
            this.overlay.updateScreenshotsOverlayDimensions("resize");
          }, 16);
        }
        this.#resizeTask.arm();
        break;
      case "scroll":
        if (!this.#scrollTask && this.overlay?.initialized) {
          this.#scrollTask = new lazy.DeferredTask(() => {
            this.overlay.updateScreenshotsOverlayDimensions("scroll");
          }, 16);
        }
        this.#scrollTask.arm();
        break;
      case "visibilitychange":
        if (
          event.target.visibilityState === "hidden" &&
          this.overlay?.state === "crosshairs"
        ) {
          this.requestCancelScreenshot("navigation");
        }
        break;
      case "Screenshots:Close":
        this.requestCancelScreenshot(event.detail.reason);
        break;
      case "Screenshots:Copy":
        this.requestCopyScreenshot(event.detail.region);
        break;
      case "Screenshots:Download":
        this.requestDownloadScreenshot(event.detail.region);
        break;
      case "Screenshots:OverlaySelection":
        let { hasSelection } = event.detail;
        this.sendOverlaySelection({ hasSelection });
        break;
      case "Screenshots:RecordEvent":
        let { eventName, reason, args } = event.detail;
        this.recordTelemetryEvent(eventName, reason, args);
        break;
      case "Screenshots:ShowPanel":
        this.showPanel();
        break;
      case "Screenshots:HidePanel":
        this.hidePanel();
        break;
    }
  }

  /**
   * Send a request to cancel the screenshot to the parent process
   */
  requestCancelScreenshot(reason) {
    this.sendAsyncMessage("Screenshots:CancelScreenshot", {
      closeOverlay: false,
      reason,
    });
    this.endScreenshotsOverlay();
  }

  /**
   * Send a request to copy the screenshots
   * @param {Object} region The region dimensions of the screenshot to be copied
   */
  requestCopyScreenshot(region) {
    region.devicePixelRatio = this.contentWindow.devicePixelRatio;
    this.sendAsyncMessage("Screenshots:CopyScreenshot", { region });
    this.endScreenshotsOverlay({ doNotResetMethods: true });
  }

  /**
   * Send a request to download the screenshots
   * @param {Object} region The region dimensions of the screenshot to be downloaded
   */
  requestDownloadScreenshot(region) {
    region.devicePixelRatio = this.contentWindow.devicePixelRatio;
    this.sendAsyncMessage("Screenshots:DownloadScreenshot", {
      title: this.getDocumentTitle(),
      region,
    });
    this.endScreenshotsOverlay({ doNotResetMethods: true });
  }

  showPanel() {
    this.sendAsyncMessage("Screenshots:ShowPanel");
  }

  hidePanel() {
    this.sendAsyncMessage("Screenshots:HidePanel");
  }

  getDocumentTitle() {
    return this.document.title;
  }

  sendOverlaySelection(data) {
    this.sendAsyncMessage("Screenshots:OverlaySelection", data);
  }

  getMethodsUsed() {
    let methodsUsed = this.#overlay.methodsUsed;
    this.#overlay.resetMethodsUsed();
    return methodsUsed;
  }

  /**
   * Resolves when the document is ready to have an overlay injected into it.
   *
   * @returns {Promise}
   * @resolves {Boolean} true when document is ready or rejects
   */
  documentIsReady() {
    const document = this.document;
    // Some pages take ages to finish loading - if at all.
    // We want to respond to enable the screenshots UI as soon that is possible
    function readyEnough() {
      return (
        document.readyState !== "uninitialized" && document.documentElement
      );
    }

    if (readyEnough()) {
      return Promise.resolve();
    }
    return new Promise((resolve, reject) => {
      function onChange(event) {
        if (event.type === "pagehide") {
          document.removeEventListener("readystatechange", onChange);
          this.contentWindow.removeEventListener("pagehide", onChange);
          reject(new Error("document unloaded before it was ready"));
        } else if (readyEnough()) {
          document.removeEventListener("readystatechange", onChange);
          this.contentWindow.removeEventListener("pagehide", onChange);
          resolve();
        }
      }
      document.addEventListener("readystatechange", onChange);
      this.contentWindow.addEventListener("pagehide", onChange, { once: true });
    });
  }

  addOverlayEventListeners() {
    let chromeEventHandler = this.docShell.chromeEventHandler;
    for (let event of ScreenshotsComponentChild.OVERLAY_EVENTS) {
      chromeEventHandler.addEventListener(event, this, true);
    }
  }

  /**
   * Wait until the document is ready and then show the screenshots overlay
   *
   * @returns {Boolean} true when document is ready and the overlay is shown
   * otherwise false
   */
  async startScreenshotsOverlay() {
    try {
      await this.documentIsReady();
    } catch (ex) {
      console.warn(`ScreenshotsComponentChild: ${ex.message}`);
      return false;
    }
    await this.documentIsReady();
    let overlay =
      this.overlay ||
      (this.#overlay = new lazy.ScreenshotsOverlay(this.document));
    this.document.ownerGlobal.addEventListener("beforeunload", this);
    this.contentWindow.addEventListener("resize", this);
    this.contentWindow.addEventListener("scroll", this);
    this.contentWindow.addEventListener("visibilitychange", this);
    this.addOverlayEventListeners();

    overlay.initialize();
    return true;
  }

  removeOverlayEventListeners() {
    let chromeEventHandler = this.docShell.chromeEventHandler;
    for (let event of ScreenshotsComponentChild.OVERLAY_EVENTS) {
      chromeEventHandler.removeEventListener(event, this, true);
    }
  }

  /**
   * Removes event listeners and the screenshots overlay.
   */
  endScreenshotsOverlay(options = {}) {
    this.document.ownerGlobal.removeEventListener("beforeunload", this);
    this.contentWindow.removeEventListener("resize", this);
    this.contentWindow.removeEventListener("scroll", this);
    this.contentWindow.removeEventListener("visibilitychange", this);
    this.removeOverlayEventListeners();

    this.overlay?.tearDown(options);
    this.#resizeTask?.disarm();
    this.#scrollTask?.disarm();
  }

  didDestroy() {
    this.#resizeTask?.disarm();
    this.#scrollTask?.disarm();
  }

  /**
   * Gets the full page bounds for a full page screenshot.
   *
   * @returns { object }
   *   The device pixel ratio and a DOMRect of the scrollable content bounds.
   *
   *   devicePixelRatio (float):
   *      The device pixel ratio of the screen
   *
   *   rect (object):
   *      top (int):
   *        The scroll top position for the content window.
   *
   *      left (int):
   *        The scroll left position for the content window.
   *
   *      width (int):
   *        The scroll width of the content window.
   *
   *      height (int):
   *        The scroll height of the content window.
   */
  getFullPageBounds() {
    let {
      scrollMinX,
      scrollMinY,
      scrollWidth,
      scrollHeight,
      devicePixelRatio,
    } = this.#overlay.windowDimensions.dimensions;
    let rect = {
      left: scrollMinX,
      top: scrollMinY,
      right: scrollWidth,
      bottom: scrollHeight,
      width: scrollWidth,
      height: scrollHeight,
      devicePixelRatio,
    };
    return rect;
  }

  /**
   * Gets the visible page bounds for a visible screenshot.
   *
   * @returns { object }
   *   The device pixel ratio and a DOMRect of the current visible
   *   content bounds.
   *
   *   devicePixelRatio (float):
   *      The device pixel ratio of the screen
   *
   *   rect (object):
   *      top (int):
   *        The top position for the content window.
   *
   *      left (int):
   *        The left position for the content window.
   *
   *      width (int):
   *        The width of the content window.
   *
   *      height (int):
   *        The height of the content window.
   */
  getVisibleBounds() {
    let { scrollX, scrollY, clientWidth, clientHeight, devicePixelRatio } =
      this.#overlay.windowDimensions.dimensions;
    let rect = {
      left: scrollX,
      top: scrollY,
      right: scrollX + clientWidth,
      bottom: scrollY + clientHeight,
      width: clientWidth,
      height: clientHeight,
      devicePixelRatio,
    };
    return rect;
  }

  recordTelemetryEvent(type, object, args = {}) {
    Services.telemetry.recordEvent("screenshots", type, object, null, args);
  }
}
