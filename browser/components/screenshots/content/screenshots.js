/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-env mozilla/browser-window */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
  ScreenshotsUtils: "resource:///modules/ScreenshotsUtils.sys.mjs",
  ShortcutUtils: "resource://gre/modules/ShortcutUtils.sys.mjs",
});

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "screenshotsLocalization", () => {
  return new Localization(["browser/screenshots.ftl"], true);
});

class ScreenshotsPreview extends HTMLElement {
  constructor() {
    super();
    // we get passed the <browser> as a param via TabDialogBox.open()
    this.openerBrowser = window.arguments[0];

    window.ensureCustomElements("moz-button");

    let [downloadKey, copyKey] =
      lazy.screenshotsLocalization.formatMessagesSync([
        { id: "screenshots-component-download-key" },
        { id: "screenshots-component-copy-key" },
      ]);

    this.downloadKey = downloadKey.value;
    this.copyKey = copyKey.value;
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

    let accelString = ShortcutUtils.getModifierString("accel");
    let copyShorcut = accelString + this.copyKey;
    let downloadShortcut = accelString + this.downloadKey;

    document.l10n.setAttributes(
      this._cancelButton,
      AppConstants.platform === "macosx"
        ? "screenshots-component-cancel-mac-button"
        : "screenshots-component-cancel-button"
    );

    document.l10n.setAttributes(
      this._copyButton,
      "screenshots-component-copy-button",
      { shortcut: copyShorcut }
    );

    document.l10n.setAttributes(
      this._downloadButton,
      "screenshots-component-download-button",
      { shortcut: downloadShortcut }
    );

    window.addEventListener("keydown", this, true);
  }

  close() {
    URL.revokeObjectURL(document.getElementById("placeholder-image").src);
    window.close();
  }

  handleEvent(event) {
    switch (event.type) {
      case "click":
        this.handleClick(event);
        break;
      case "keydown":
        this.handleKeydown(event);
        break;
    }
  }

  handleClick(event) {
    switch (event.target.id) {
      case "retry":
        ScreenshotsUtils.scheduleRetry(this.openerBrowser, "preview_retry");
        this.close();
        break;
      case "cancel":
        this.close();
        ScreenshotsUtils.recordTelemetryEvent("canceled", "preview_cancel", {});
        break;
      case "copy":
        this.saveToClipboard(
          this.ownerDocument.getElementById("placeholder-image").src
        );
        break;
      case "download":
        this.saveToFile(
          this.ownerDocument.getElementById("placeholder-image").src
        );
        break;
    }
  }

  handleKeydown(event) {
    switch (event.key) {
      case this.copyKey.toLowerCase():
        if (this.getAccelKey(event)) {
          event.preventDefault();
          event.stopPropagation();
          this.saveToClipboard(
            this.ownerDocument.getElementById("placeholder-image").src
          );
        }
        break;
      case this.downloadKey.toLowerCase():
        if (this.getAccelKey(event)) {
          event.preventDefault();
          event.stopPropagation();
          this.saveToFile(
            this.ownerDocument.getElementById("placeholder-image").src
          );
        }
        break;
    }
  }

  getAccelKey(event) {
    if (AppConstants.platform === "macosx") {
      return event.metaKey;
    }
    return event.ctrlKey;
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
customElements.define("screenshots-preview", ScreenshotsPreview);
