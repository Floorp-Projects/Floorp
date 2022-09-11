/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-env mozilla/browser-window */

"use strict";

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ScreenshotsOverlayChild:
    "resource:///modules/ScreenshotsOverlayChild.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(lazy, {
  DeferredTask: "resource://gre/modules/DeferredTask.jsm",
});

export class ScreenshotsComponentChild extends JSWindowActorChild {
  receiveMessage(message) {
    switch (message.name) {
      case "Screenshots:ShowOverlay":
        return this.startScreenshotsOverlay();
      case "Screenshots:HideOverlay":
        return this.endScreenshotsOverlay();
      case "Screenshots:getFullPageBounds":
        return this.getFullPageBounds();
      case "Screenshots:getVisibleBounds":
        return this.getVisibleBounds();
    }
    return null;
  }

  handleEvent(event) {
    switch (event.type) {
      case "keydown":
        if (event.key === "Escape") {
          this.requestCancelScreenshot();
        }
        break;
      case "beforeunload":
        this.requestCancelScreenshot();
        break;
      case "resize":
        if (!this._resizeTask && this._overlay?._initialized) {
          this._resizeTask = new lazy.DeferredTask(() => {
            this._overlay.updateScreenshotsSize("resize");
          }, 16);
        }
        this._resizeTask.arm();
        break;
      case "scroll":
        if (!this._scrollTask && this._overlay?._initialized) {
          this._scrollTask = new lazy.DeferredTask(() => {
            this._overlay.updateScreenshotsSize("scroll");
          }, 16);
        }
        this._scrollTask.arm();
        break;
    }
  }

  /**
   * Send a request to cancel the screenshot to the parent process
   */
  requestCancelScreenshot() {
    this.sendAsyncMessage("Screenshots:CancelScreenshot", null);
  }

  requestCopyScreenshot(box) {
    this.sendAsyncMessage("Screenshots:CopyScreenshot", box);
  }

  requestDownloadScreenshot(box) {
    this.sendAsyncMessage("Screenshots:DownloadScreenshot", {
      title: this.getTitle(),
      downloadBox: box,
    });
  }

  getTitle() {
    return this.document.title;
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
      this._overlay ||
      (this._overlay = new lazy.ScreenshotsOverlayChild.AnonymousContentOverlay(
        this.document,
        this
      ));
    this.document.addEventListener("keydown", this);
    this.document.ownerGlobal.addEventListener("beforeunload", this);
    this.contentWindow.addEventListener("resize", this);
    this.contentWindow.addEventListener("scroll", this);
    overlay.initialize();
    return true;
  }

  /**
   * Remove the screenshots overlay.
   *
   * @returns {Boolean}
   *   true when the overlay has been removed otherwise false
   */
  endScreenshotsOverlay() {
    this.document.removeEventListener("keydown", this);
    this.document.ownerGlobal.removeEventListener("beforeunload", this);
    this.contentWindow.removeEventListener("resize", this);
    this.contentWindow.removeEventListener("scroll", this);
    this._overlay?.tearDown();
    this._resizeTask?.disarm();
    this._scrollTask?.disarm();
    return true;
  }

  didDestroy() {
    this._resizeTask?.disarm();
    this._scrollTask?.disarm();
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
    let doc = this.document.documentElement;
    let rect = new DOMRect(
      doc.clientLeft,
      doc.clientTop,
      doc.scrollWidth,
      doc.scrollHeight
    );
    let devicePixelRatio = this.document.ownerGlobal.devicePixelRatio;
    return { devicePixelRatio, rect };
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
    let doc = this.document.documentElement;
    let rect = new DOMRect(
      doc.scrollLeft,
      doc.scrollTop,
      doc.clientWidth,
      doc.clientHeight
    );
    let devicePixelRatio = this.document.ownerGlobal.devicePixelRatio;
    return { devicePixelRatio, rect };
  }
}
