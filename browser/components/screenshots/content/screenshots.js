/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-env mozilla/browser-window */

"use strict";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  Downloads: "resource://gre/modules/Downloads.sys.mjs",
  FileUtils: "resource://gre/modules/FileUtils.sys.mjs",
  ScreenshotsUtils: "resource:///modules/ScreenshotsUtils.sys.mjs",
});

class ScreenshotsUI extends HTMLElement {
  constructor() {
    super();
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

    this._retryButton = this.querySelector(".highlight-button-retry");
    this._retryButton.addEventListener("click", this);
    this._cancelButton = this.querySelector(".highlight-button-cancel");
    this._cancelButton.addEventListener("click", this);
    this._copyButton = this.querySelector(".highlight-button-copy");
    this._copyButton.addEventListener("click", this);
    this._downloadButton = this.querySelector(".highlight-button-download");
    this._downloadButton.addEventListener("click", this);
  }

  close() {
    URL.revokeObjectURL(document.getElementById("placeholder-image").src);
    window.close();
  }

  async handleEvent(event) {
    if (event.type == "click" && event.currentTarget == this._cancelButton) {
      this.close();
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
      Services.obs.notifyObservers(
        window.parent.ownerGlobal,
        "menuitem-screenshot",
        "retry"
      );
    }
  }

  async saveToFile(dataUrl) {
    await ScreenshotsUtils.downloadScreenshot(
      null,
      dataUrl,
      window.parent.ownerGlobal.gBrowser.selectedBrowser
    );

    this.close();
  }

  saveToClipboard(dataUrl) {
    ScreenshotsUtils.copyScreenshot(dataUrl);

    this.close();
  }
}
customElements.define("screenshots-ui", ScreenshotsUI);
