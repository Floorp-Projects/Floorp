/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  Downloads: "resource://gre/modules/Downloads.jsm",
  FileUtils: "resource://gre/modules/FileUtils.jsm",
});

class ScreenshotsUI extends HTMLElement {
  constructor() {
    super();
  }
  connectedCallback() {
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

    this._cancelButton = this.querySelector(".highlight-button-cancel");
    this._cancelButton.addEventListener("click", this);
    this._copyButton = this.querySelector(".highlight-button-copy");
    this._copyButton.addEventListener("click", this);
    this._downloadButton = this.querySelector(".highlight-button-download");
    this._downloadButton.addEventListener("click", this);
  }

  async handleEvent(event) {
    if (event.type == "click" && event.target == this._cancelButton) {
      window.close();
    } else if (event.type == "click" && event.target == this._copyButton) {
      this.saveToClipboard(
        this.ownerDocument.getElementById("placeholder-image").src
      );
    } else if (event.type == "click" && event.target == this._downloadButton) {
      await this.saveToFile(
        this.ownerDocument.getElementById("placeholder-image").src
      );
    }
  }

  //Adapted from devtools/client/shared/screenshot.js
  async saveToFile(src) {
    let filename = this.getFilename();

    // Guard against missing image data.
    if (!src) {
      return;
    }

    // Check there is a .png extension to filename
    if (!filename.match(/.png$/i)) {
      filename += ".png";
    }

    const downloadsDir = await Downloads.getPreferredDownloadsDirectory();
    const downloadsDirExists = await IOUtils.exists(downloadsDir);
    if (downloadsDirExists) {
      // If filename is absolute, it will override the downloads directory and
      // still be applied as expected.
      filename = PathUtils.join(downloadsDir, filename);
    }

    const sourceURI = Services.io.newURI(src);
    const targetFile = new FileUtils.File(filename);

    // Create download and track its progress.
    try {
      const download = await Downloads.createDownload({
        source: sourceURI,
        target: targetFile,
      });
      const list = await Downloads.getList(Downloads.ALL);
      // add the download to the download list in the Downloads list in the Browser UI
      list.add(download);

      window.close();

      // Await successful completion of the save via the download manager
      await download.start();
    } catch (ex) {}
  }

  //Adapted from devtools/client/shared/screenshot.js
  saveToClipboard(src) {
    try {
      const imageTools = Cc["@mozilla.org/image/tools;1"].getService(
        Ci.imgITools
      );

      var img = new Image();
      img.crossOrigin = "Anonymous";
      img.onload = function() {
        var canvas = document.createElement("canvas");
        var ctx = canvas.getContext("2d");
        var dataURL;
        canvas.height = this.naturalHeight;
        canvas.width = this.naturalWidth;
        ctx.drawImage(this, 0, 0);
        dataURL = canvas.toDataURL("image/png");
        const base64Data = dataURL.replace("data:image/png;base64,", "");

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

        window.close();
      };
      img.src = src;
    } catch (ex) {}
  }

  getFilename() {
    let filenameTitle = "Dummy Page"; /* TODO: retrieve title froom image */
    const date = new Date(Date.now());
    // eslint-disable-next-line no-control-regex
    filenameTitle = filenameTitle.replace(/[:\\<>/!@&?"*.|\x00-\x1F]/g, " ");
    filenameTitle = filenameTitle.replace(/\s{1,4000}/g, " ");
    const currentDateTime = new Date(
      date.getTime() - date.getTimezoneOffset() * 60 * 1000
    ).toISOString();
    const filenameDate = currentDateTime.substring(0, 10);
    const filenameTime = currentDateTime.substring(11, 19).replace(/:/g, "-");
    let clipFilename = `Screenshot ${filenameDate} at ${filenameTime} ${filenameTitle}`;
    const clipFilenameBytesSize = clipFilename.length * 2; // JS STrings are UTF-16
    if (clipFilenameBytesSize > 251) {
      // 255 bytes (Usual filesystems max) - 4 for the ".png" file extension string
      const excedingchars = (clipFilenameBytesSize - 246) / 2; // 251 - 5 for ellipsis "[...]"
      clipFilename = clipFilename.substring(
        0,
        clipFilename.length - excedingchars
      );
      clipFilename = clipFilename + "[...]";
    }
    let extension = ".png";
    return clipFilename + extension;
  }
}
customElements.define("screenshots-ui", ScreenshotsUI);
