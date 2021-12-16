/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-env mozilla/browser-window */

"use strict";

var EXPORTED_SYMBOLS = ["ScreenshotsComponentChild"];

class ScreenshotsComponentChild extends JSWindowActorChild {
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
  async startScreenshotsOverlay(details = {}) {
    try {
      await this.documentIsReady();
    } catch (ex) {
      console.warn(`ScreenshotsComponentChild: ${ex.message}`);
      return false;
    }
    return true;
  }

  /**
   * Remove the screenshots overlay.
   *
   * @returns {Boolean}
   *   true when the overlay has been removed otherwise false
   */
  endScreenshotsOverlay() {
    // this function will be implemented soon
    return true;
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
      doc.scrollTop,
      doc.scrollLeft,
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
      doc.clientTop,
      doc.clientLeft,
      doc.clientWidth,
      doc.clientHeight
    );
    let devicePixelRatio = this.document.ownerGlobal.devicePixelRatio;
    return { devicePixelRatio, rect };
  }
}
