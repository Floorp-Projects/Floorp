/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-button.mjs";

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "screenshotsLocalization", () => {
  return new Localization(["browser/screenshots.ftl"], true);
});

ChromeUtils.defineESModuleGetters(lazy, {
  AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
  ScreenshotsUtils: "resource:///modules/ScreenshotsUtils.sys.mjs",
  ShortcutUtils: "resource://gre/modules/ShortcutUtils.sys.mjs",
});

class ScreenshotsPreview extends MozLitElement {
  static queries = {
    retryButtonEl: "#retry",
    cancelButtonEl: "#cancel",
    copyButtonEl: "#copy",
    downloadButtonEl: "#download",
    previewImg: "#preview-image",
    buttons: { all: "moz-button" },
  };

  constructor() {
    super();
    // we get passed the <browser> as a param via TabDialogBox.open()
    this.openerBrowser = window.arguments[0];

    let [downloadKey, copyKey] =
      lazy.screenshotsLocalization.formatMessagesSync([
        { id: "screenshots-component-download-key" },
        { id: "screenshots-component-copy-key" },
      ]);

    this.downloadKey = downloadKey.value;
    this.copyKey = copyKey.value;
  }

  connectedCallback() {
    super.connectedCallback();

    window.addEventListener("keydown", this, true);

    this.updateL10nAttributes();
  }

  async updateL10nAttributes() {
    let accelString = lazy.ShortcutUtils.getModifierString("accel");
    let copyShorcut = accelString + this.copyKey;
    let downloadShortcut = accelString + this.downloadKey;

    await this.updateComplete;

    document.l10n.setAttributes(
      this.copyButtonEl,
      "screenshots-component-copy-button-2",
      { shortcut: copyShorcut }
    );

    document.l10n.setAttributes(
      this.downloadButtonEl,
      "screenshots-component-download-button-2",
      { shortcut: downloadShortcut }
    );
  }

  close() {
    window.removeEventListener("keydown", this, true);
    URL.revokeObjectURL(this.previewImg.src);
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
        lazy.ScreenshotsUtils.scheduleRetry(
          this.openerBrowser,
          "preview_retry"
        );
        this.close();
        break;
      case "cancel":
        this.close();
        lazy.ScreenshotsUtils.recordTelemetryEvent(
          "canceled",
          "preview_cancel",
          {}
        );
        break;
      case "copy":
        this.saveToClipboard();
        break;
      case "download":
        this.saveToFile();
        break;
    }
  }

  handleKeydown(event) {
    switch (event.key) {
      case this.copyKey.toLowerCase():
        if (this.getAccelKey(event)) {
          event.preventDefault();
          event.stopPropagation();
          this.saveToClipboard();
        }
        break;
      case this.downloadKey.toLowerCase():
        if (this.getAccelKey(event)) {
          event.preventDefault();
          event.stopPropagation();
          this.saveToFile();
        }
        break;
    }
  }

  /**
   * If the image is complete and the height is greater than 0, we can resolve.
   * Otherwise wait for a load event on the image and resolve then.
   * @returns {Promise<String>} Resolves that resolves to the preview image src
   *                            once the image is loaded.
   */
  async imageLoadedPromise() {
    await this.updateComplete;
    if (this.previewImg.complete && this.previewImg.height > 0) {
      return Promise.resolve(this.previewImg.src);
    }

    return new Promise(resolve => {
      function onImageLoaded(event) {
        resolve(event.target.src);
      }
      this.previewImg.addEventListener("load", onImageLoaded, { once: true });
    });
  }

  getAccelKey(event) {
    if (lazy.AppConstants.platform === "macosx") {
      return event.metaKey;
    }
    return event.ctrlKey;
  }

  /**
   * Enable all the buttons. This will only happen when the download button is
   * clicked and the file picker is closed without saving the image.
   */
  enableButtons() {
    this.buttons.forEach(button => (button.disabled = false));
  }

  /**
   * Disable all the buttons so they can't be clicked multiple times before
   * successfully copying or downloading the image.
   */
  disableButtons() {
    this.buttons.forEach(button => (button.disabled = true));
  }

  async saveToFile() {
    // Disable buttons so they can't by clicked again while waiting for the
    // image to load.
    this.disableButtons();

    // Wait for the image to be loaded before we save it
    let imageSrc = await this.imageLoadedPromise();
    let downloadSucceeded = await lazy.ScreenshotsUtils.downloadScreenshot(
      null,
      imageSrc,
      this.openerBrowser,
      { object: "preview_download" }
    );

    if (downloadSucceeded) {
      this.close();
    } else {
      this.enableButtons();
    }
  }

  async saveToClipboard() {
    // Disable buttons so they can't by clicked again while waiting for the
    // image to load
    this.disableButtons();

    // Wait for the image to be loaded before we copy it
    let imageSrc = await this.imageLoadedPromise();
    await lazy.ScreenshotsUtils.copyScreenshot(imageSrc, this.openerBrowser, {
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
      this.copyButtonEl.focus({ focusVisible: true });
    } else {
      this.downloadButtonEl.focus({ focusVisible: true });
    }
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/screenshots/screenshots-preview.css"
      />
      <div class="image-view">
        <div class="preview-buttons">
          <moz-button
            id="retry"
            data-l10n-id="screenshots-component-retry-button"
            iconSrc="chrome://global/skin/icons/reload.svg"
            @click=${this.handleClick}
          ></moz-button>
          <moz-button
            id="cancel"
            data-l10n-id="screenshots-component-cancel-button"
            iconSrc="chrome://global/skin/icons/close.svg"
            @click=${this.handleClick}
          ></moz-button>
          <moz-button
            id="copy"
            data-l10n-id="screenshots-component-copy-button-2"
            data-l10n-args='{ "shortcut": "" }'
            iconSrc="chrome://global/skin/icons/edit-copy.svg"
            @click=${this.handleClick}
          ></moz-button>
          <moz-button
            id="download"
            type="primary"
            data-l10n-id="screenshots-component-download-button-2"
            data-l10n-args='{ "shortcut": "" }'
            iconSrc="chrome://browser/skin/downloads/downloads.svg"
            @click=${this.handleClick}
          ></moz-button>
        </div>
        <div id="preview-image-container">
          <img id="preview-image" />
        </div>
      </div>
    `;
  }
}

customElements.define("screenshots-preview", ScreenshotsPreview);
