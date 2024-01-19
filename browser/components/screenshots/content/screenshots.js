/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-env mozilla/browser-window */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ScreenshotsUtils: "resource:///modules/ScreenshotsUtils.sys.mjs",
});

class ScreenshotsUI extends HTMLElement {
  constructor() {
    super();
    // we get passed the <browser> as a param via TabDialogBox.open()
    this.openerBrowser = window.arguments[0];
  }
  async connectedCallback() {
    this.initialize();
  }

  initialize() {
    if (this._initialized) {
      return;
    }
    this._initialized = true;
    let template = this.ownerDocument.getElementById(
      "screenshots-dialog-template"
    );
    let templateContent = template.content;
    this.appendChild(templateContent.cloneNode(true));

    this._retryButton = this.querySelector("#retry");
    this._retryButton.addEventListener("click", this);
    this._cancelButton = this.querySelector("#cancel");
    this._cancelButton.addEventListener("click", this);
    this._copyButton = this.querySelector("#copy");
    this._copyButton.addEventListener("click", this);
    this._downloadButton = this.querySelector("#download");
    this._downloadButton.addEventListener("click", this);
  }

  close() {
    URL.revokeObjectURL(document.getElementById("placeholder-image").src);
    window.close();
  }

  async handleEvent(event) {
    if (event.type == "click" && event.currentTarget == this._cancelButton) {
      this.close();
      ScreenshotsUtils.recordTelemetryEvent("canceled", "preview_cancel", {});
    } else if (
      event.type == "click" &&
      event.currentTarget == this._copyButton
    ) {
      this.saveToClipboard(
        this.ownerDocument.getElementById("placeholder-image").src
      );
    } else if (
      event.type == "click" &&
      event.currentTarget == this._downloadButton
    ) {
      await this.saveToFile(
        this.ownerDocument.getElementById("placeholder-image").src
      );
    } else if (
      event.type == "click" &&
      event.currentTarget == this._retryButton
    ) {
      ScreenshotsUtils.scheduleRetry(this.openerBrowser, "preview_retry");
      this.close();
    }
  }

  async saveToFile(dataUrl) {
    await ScreenshotsUtils.downloadScreenshot(
      null,
      dataUrl,
      this.openerBrowser,
      { object: "preview_download" }
    );
    this.close();
  }

  async saveToClipboard(dataUrl) {
    await ScreenshotsUtils.copyScreenshot(dataUrl, this.openerBrowser, {
      object: "preview_copy",
    });
    this.close();
  }

  /**
   * Set the focus to the most recent saved method.
   * This will default to the download button.
   * @param {String} buttonToFocus
   */
  focusButton(buttonToFocus) {
    if (buttonToFocus === "copy") {
      this._copyButton.focus({ focusVisible: true });
    } else {
      this._downloadButton.focus({ focusVisible: true });
    }
  }
}
customElements.define("screenshots-ui", ScreenshotsUI);
