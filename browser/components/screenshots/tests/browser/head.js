/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

const TEST_PAGE = TEST_ROOT + "test-page.html";

const gScreenshotUISelectors = {
  panelButtons: "#screenshotsPagePanel",
  fullPageButton: "button.full-page",
  visiblePageButton: "button.visible-page",
  copyButton: "button.highlight-button-copy",
};

class ScreenshotsHelper {
  constructor(browser) {
    this.browser = browser;
    this.selector = gScreenshotUISelectors;
  }

  get toolbarButton() {
    return document.getElementById("screenshot-button");
  }

  /**
   * Click the screenshots button in the toolbar
   */
  triggerUIFromToolbar() {
    let button = this.toolbarButton;
    ok(
      BrowserTestUtils.is_visible(button),
      "The screenshot toolbar button is visible"
    );
    button.click();
  }

  /**
   * Gets the dialog box
   * @returns The dialog box
   */
  getDialog() {
    let currDialogBox = this.browser.tabDialogBox;
    let manager = currDialogBox.getTabDialogManager();
    let dialogs = manager.hasDialogs && manager.dialogs;
    return dialogs[0];
  }

  /**
   * Copied from screenshots extension
   * Returns a promise that resolves when the clipboard data has changed
   * Otherwise rejects
   */
  waitForRawClipboardChange() {
    const initialClipboardData = Date.now().toString();
    SpecialPowers.clipboardCopyString(initialClipboardData);

    let promiseChanged = BrowserTestUtils.waitForCondition(() => {
      let data;
      try {
        data = getRawClipboardData("image/png");
      } catch (e) {
        console.log("Failed to get image/png clipboard data:", e);
        return false;
      }
      return data && initialClipboardData !== data;
    });
    return promiseChanged;
  }

  /**
   * Gets the client and scroll demensions on the window
   * @returns { Object }
   *   clientHeight The visible height
   *   clientWidth The visible width
   *   scrollHeight The scrollable height
   *   scrollWidth The scrollable width
   */
  getContentDimensions() {
    return SpecialPowers.spawn(this.browser, [], async function() {
      let doc = content.document.documentElement;
      // let rect = doc.documentElement.getBoundingClientRect();
      return {
        clientHeight: doc.clientHeight,
        clientWidth: doc.clientWidth,
        scrollHeight: doc.scrollHeight,
        scrollWidth: doc.scrollWidth,
      };
    });
  }

  /**
   * Clicks an element on the screen
   * @param eleSel The selector for the element to click
   */
  async clickUIElement(eleSel) {
    await SpecialPowers.spawn(this.browser, [eleSel], async function(
      eleSelector
    ) {
      info(
        `in clickScreenshotsUIElement content function, eleSelector: ${eleSelector}`
      );
      const EventUtils = ContentTaskUtils.getEventUtils(content);
      let ele = content.document.querySelector(eleSelector);
      info(`Found the thing to click: ${eleSelector}: ${!!ele}`);

      EventUtils.synthesizeMouseAtCenter(ele, {});
      // wait a frame for the screenshots UI to finish any init
      await new content.Promise(res => content.requestAnimationFrame(res));
    });
  }

  /**
   * Copied from screenshots extension
   * A helper that returns the size of the image that was just put into the clipboard by the
   * :screenshot command.
   * @return The {width, height, color} dimension and color object.
   */
  async getImageSizeAndColorFromClipboard() {
    let flavor = "image/png";
    let image = getRawClipboardData(flavor);
    ok(image, "screenshot data exists on the clipboard");

    // Due to the differences in how images could be stored in the clipboard the
    // checks below are needed. The clipboard could already provide the image as
    // byte streams or as image container. If it's not possible obtain a
    // byte stream, the function throws.

    if (image instanceof Ci.imgIContainer) {
      image = Cc["@mozilla.org/image/tools;1"]
        .getService(Ci.imgITools)
        .encodeImage(image, flavor);
    }

    if (!(image instanceof Ci.nsIInputStream)) {
      throw new Error("Unable to read image data");
    }

    const binaryStream = Cc["@mozilla.org/binaryinputstream;1"].createInstance(
      Ci.nsIBinaryInputStream
    );
    binaryStream.setInputStream(image);
    const available = binaryStream.available();
    const buffer = new ArrayBuffer(available);
    is(
      binaryStream.readArrayBuffer(available, buffer),
      available,
      "Read expected amount of data"
    );

    // We are going to load the image in the content page to measure its size.
    // We don't want to insert the image directly in the browser's document
    // which could mess all sorts of things up
    return SpecialPowers.spawn(this.browser, [buffer], async function(_buffer) {
      const img = content.document.createElement("img");
      const loaded = new Promise(r => {
        img.addEventListener("load", r, { once: true });
      });
      const url = content.URL.createObjectURL(
        new Blob([_buffer], { type: "image/png" })
      );

      img.src = url;
      content.document.documentElement.appendChild(img);

      info("Waiting for the clipboard image to load in the content page");
      await loaded;

      let canvas = content.document.createElementNS(
        "http://www.w3.org/1999/xhtml",
        "html:canvas"
      );
      let context = canvas.getContext("2d");
      canvas.width = img.width;
      canvas.height = img.height;
      context.drawImage(img, 0, 0);
      let topLeft = context.getImageData(0, 0, 1, 1);
      let topRight = context.getImageData(img.width - 1, 0, 1, 1);
      let bottomLeft = context.getImageData(0, img.height - 1, 1, 1);
      let bottomRight = context.getImageData(
        img.width - 1,
        img.height - 1,
        1,
        1
      );

      img.remove();
      content.URL.revokeObjectURL(url);

      return {
        width: img.width,
        height: img.height,
        color: {
          topLeft: topLeft.data,
          topRight: topRight.data,
          bottomLeft: bottomLeft.data,
          bottomRight: bottomRight.data,
        },
      };
    });
  }
}

/**
 * Get the raw clipboard data
 * @param flavor Type of data to get from clipboard
 * @returns The data from the clipboard
 */
function getRawClipboardData(flavor) {
  const whichClipboard = Services.clipboard.kGlobalClipboard;
  const xferable = Cc["@mozilla.org/widget/transferable;1"].createInstance(
    Ci.nsITransferable
  );
  xferable.init(null);
  xferable.addDataFlavor(flavor);
  Services.clipboard.getData(xferable, whichClipboard);
  let data = {};
  try {
    // xferable.getTransferData(flavor, data);
    xferable.getAnyTransferData({}, data);
    info(JSON.stringify(data, null, 2));
  } catch (e) {
    info(e);
  }
  data = data.value || null;
  return data;
}
