/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

const TEST_PAGE = TEST_ROOT + "test-page.html";
const SHORT_TEST_PAGE = TEST_ROOT + "short-test-page.html";

const gScreenshotUISelectors = {
  panelButtons: "#screenshotsPagePanel",
  fullPageButton: "button.full-page",
  visiblePageButton: "button.visible-page",
  copyButton: "button.highlight-button-copy",
};

// MouseEvents is for the mouse events on the Anonymous content
const MouseEvents = {
  mouse: new Proxy(
    {},
    {
      get: (target, name) =>
        async function(x, y, selector = ":root") {
          if (name === "click") {
            this.down(x, y);
            this.up(x, y);
          } else {
            await safeSynthesizeMouseEventInContentPage(selector, x, y, {
              type: "mouse" + name,
            });
          }
        },
    }
  ),
};

const { mouse } = MouseEvents;

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

  async waitForOverlay() {
    let panel = gBrowser.selectedBrowser.ownerDocument.querySelector(
      "#screenshotsPagePanel"
    );
    await BrowserTestUtils.waitForMutationCondition(
      panel,
      { attributes: true },
      () => {
        return BrowserTestUtils.is_visible(panel);
      }
    );
    ok(BrowserTestUtils.is_visible(panel), "Panel buttons are visible");

    await BrowserTestUtils.waitForCondition(async () => {
      let init = await this.isOverlayInitialized();
      return init;
    });
    info("Overlay is visible");
  }

  async waitForOverlayClosed() {
    await BrowserTestUtils.waitForCondition(async () => {
      let init = !(await this.isOverlayInitialized());
      info("Is overlay initialized: " + !init);
      return init;
    });
    info("Overlay is not visible");
  }

  async isOverlayInitialized() {
    return SpecialPowers.spawn(this.browser, [], () => {
      let screenshotsChild = content.windowGlobalChild.getActor(
        "ScreenshotsComponent"
      );
      return screenshotsChild?._overlay?._initialized;
    });
  }

  async getOverlayState() {
    return ContentTask.spawn(this.browser, null, async () => {
      let screenshotsChild = content.windowGlobalChild.getActor(
        "ScreenshotsComponent"
      );
      return screenshotsChild._overlay.stateHandler.getState();
    });
  }

  async waitForStateChange(newState) {
    await BrowserTestUtils.waitForCondition(async () => {
      let state = await this.getOverlayState();
      return state === newState;
    });
  }

  async waitForSelectionBoxSizeChange(currentWidth) {
    await ContentTask.spawn(
      this.browser,
      [currentWidth],
      async ([currWidth]) => {
        let screenshotsChild = content.windowGlobalChild.getActor(
          "ScreenshotsComponent"
        );

        let dimensions = screenshotsChild._overlay.screenshotsContainer.getSelectionLayerDimensions();
        // return dimensions.boxWidth;
        await ContentTaskUtils.waitForCondition(() => {
          dimensions = screenshotsChild._overlay.screenshotsContainer.getSelectionLayerDimensions();
          return dimensions.boxWidth !== currWidth;
        }, "Wait for selection box width change");
      }
    );
  }

  async dragOverlay(startX, startY, endX, endY) {
    await this.waitForStateChange("crosshairs");
    let state = await this.getOverlayState();
    Assert.equal(state, "crosshairs", "The overlay is in the crosshairs state");

    mouse.down(startX, startY);

    await this.waitForStateChange("draggingReady");
    state = await this.getOverlayState();
    Assert.equal(
      state,
      "draggingReady",
      "The overlay is in the draggingReady state"
    );

    mouse.move(endX, endY);

    await this.waitForStateChange("dragging");
    state = await this.getOverlayState();
    Assert.equal(state, "dragging", "The overlay is in the dragging state");

    mouse.up(endX, endY);

    await this.waitForStateChange("selected");
    state = await this.getOverlayState();
    Assert.equal(state, "selected", "The overlay is in the selected state");

    this.endX = endX;
    this.endY = endY;
  }

  async scrollContentWindow(x, y) {
    await ContentTask.spawn(this.browser, [x, y], async ([xPos, yPos]) => {
      content.window.scroll(xPos, yPos);
    });
  }

  clickCopyButton(overrideX = null, overrideY = null) {
    // click copy button with last x and y position from dragOverlay
    // the middle of the copy button is last X - 163 and last Y + 30.
    // Ex. 500, 500 would be 336, 530
    if (overrideX && overrideY) {
      mouse.click(overrideX - 166, overrideY + 30);
    } else {
      mouse.click(this.endX - 166, this.endY + 30);
    }
  }

  clickCancelButton() {
    // click copy button with last x and y position from dragOverlay
    // the middle of the copy button is last X - 230 and last Y + 30.
    // Ex. 500, 500 would be 270, 530
    mouse.click(this.endX - 230, this.endY + 30);
  }

  async zoomBrowser(zoom) {
    await SpecialPowers.spawn(this.browser, [zoom], zoomLevel => {
      const { Layout } = ChromeUtils.import(
        "chrome://mochitests/content/browser/accessible/tests/browser/Layout.jsm"
      );
      Layout.zoomDocument(content.document, zoomLevel);
    });
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

    let promiseChanged = TestUtils.waitForCondition(() => {
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
      return {
        clientHeight: doc.clientHeight,
        clientWidth: doc.clientWidth,
        scrollHeight: doc.scrollHeight,
        scrollWidth: doc.scrollWidth,
      };
    });
  }

  getSelectionLayerDimensions() {
    return ContentTask.spawn(this.browser, null, async () => {
      let screenshotsChild = content.windowGlobalChild.getActor(
        "ScreenshotsComponent"
      );
      Assert.ok(screenshotsChild._overlay._initialized, "The overlay exists");

      return screenshotsChild._overlay.screenshotsContainer.getSelectionLayerDimensions();
    });
  }

  getSelectionBoxDimensions() {
    return ContentTask.spawn(this.browser, null, async () => {
      let screenshotsChild = content.windowGlobalChild.getActor(
        "ScreenshotsComponent"
      );
      Assert.ok(screenshotsChild._overlay._initialized, "The overlay exists");

      return screenshotsChild._overlay.screenshotsContainer.getSelectionLayerBoxDimensions();
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

/**
 * Synthesize a mouse event on an element, after ensuring that it is visible
 * in the viewport.
 *
 * @param {String} selector: The node selector to get the node target for the event.
 * @param {number} x
 * @param {number} y
 * @param {object} options: Options that will be passed to BrowserTestUtils.synthesizeMouse
 */
async function safeSynthesizeMouseEventInContentPage(
  selector,
  x,
  y,
  options = {}
) {
  let context = gBrowser.selectedBrowser.browsingContext;
  BrowserTestUtils.synthesizeMouse(selector, x, y, options, context);
}

add_setup(async () => {
  CustomizableUI.addWidgetToArea(
    "screenshot-button",
    CustomizableUI.AREA_NAVBAR
  );
  let screenshotBtn = document.getElementById("screenshot-button");
  Assert.ok(screenshotBtn, "The screenshots button was added to the nav bar");
});
