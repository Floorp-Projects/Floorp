/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);
const { UrlbarTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/UrlbarTestUtils.sys.mjs"
);

const TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

const TEST_PAGE = TEST_ROOT + "test-page.html";
const SHORT_TEST_PAGE = TEST_ROOT + "short-test-page.html";
const LARGE_TEST_PAGE = TEST_ROOT + "large-test-page.html";
const IFRAME_TEST_PAGE = TEST_ROOT + "iframe-test-page.html";
const RESIZE_TEST_PAGE = TEST_ROOT + "test-page-resize.html";

const { MAX_CAPTURE_DIMENSION, MAX_CAPTURE_AREA } = ChromeUtils.importESModule(
  "resource:///modules/ScreenshotsUtils.sys.mjs"
);

const gScreenshotUISelectors = {
  panel: "#screenshotsPagePanel",
  fullPageButton: "button.full-page",
  visiblePageButton: "button.visible-page",
  copyButton: "button.#copy",
};

// MouseEvents is for the mouse events on the Anonymous content
const MouseEvents = {
  mouse: new Proxy(
    {},
    {
      get: (target, name) =>
        async function (x, y, options = {}, browser) {
          if (name === "click") {
            this.down(x, y, options, browser);
            this.up(x, y, options, browser);
          } else {
            await safeSynthesizeMouseEventInContentPage(
              ":root",
              x,
              y,
              {
                type: "mouse" + name,
                ...options,
              },
              browser
            );
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
    return this.browser.ownerDocument.getElementById("screenshot-button");
  }

  get panel() {
    return this.browser.ownerDocument.querySelector(this.selector.panel);
  }

  /**
   * Click the screenshots button in the toolbar
   */
  triggerUIFromToolbar() {
    let button = this.toolbarButton;
    ok(
      BrowserTestUtils.isVisible(button),
      "The screenshot toolbar button is visible"
    );
    button.click();
  }

  async getPanelButton(selector) {
    let panel = await this.waitForPanel();
    let screenshotsButtons = panel.querySelector("screenshots-buttons");
    ok(screenshotsButtons, "Found the screenshots-buttons");
    let button = screenshotsButtons.shadowRoot.querySelector(selector);
    ok(button, `Found ${selector} button`);
    return button;
  }

  async waitForPanel() {
    let panel = this.panel;
    await BrowserTestUtils.waitForCondition(async () => {
      if (!panel) {
        panel = this.panel;
      }
      return panel && BrowserTestUtils.isVisible(panel);
    });
    return panel;
  }

  async waitForOverlay() {
    const panel = await this.waitForPanel();
    ok(BrowserTestUtils.isVisible(panel), "Panel buttons are visible");

    await BrowserTestUtils.waitForCondition(async () => {
      let init = await this.isOverlayInitialized();
      return init;
    });
    info("Overlay is visible");
  }

  async waitForPanelClosed() {
    let panel = this.panel;
    if (!panel) {
      info("waitForPanelClosed: Panel doesnt exist");
      return;
    }
    if (panel.hidden) {
      info("waitForPanelClosed: panel is already hidden");
      return;
    }
    info("waitForPanelClosed: waiting for the panel to become hidden");
    await BrowserTestUtils.waitForMutationCondition(
      panel,
      { attributes: true },
      () => {
        return BrowserTestUtils.isHidden(panel);
      }
    );
    ok(BrowserTestUtils.isHidden(panel), "Panel buttons are hidden");
    info("waitForPanelClosed, panel is hidden: " + panel.hidden);
  }

  async waitForOverlayClosed() {
    await this.waitForPanelClosed();
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
      return screenshotsChild?.overlay?.initialized;
    });
  }

  waitForStateChange(newState) {
    return SpecialPowers.spawn(this.browser, [newState], async state => {
      let screenshotsChild = content.windowGlobalChild.getActor(
        "ScreenshotsComponent"
      );

      await ContentTaskUtils.waitForCondition(() => {
        info(`got ${screenshotsChild.overlay.state}. expected ${state}`);
        return screenshotsChild.overlay.state === state;
      }, `Wait for overlay state to be ${state}`);

      return screenshotsChild.overlay.state;
    });
  }

  async assertStateChange(newState) {
    let currentState = await this.waitForStateChange(newState);

    is(
      currentState,
      newState,
      `The current state is ${currentState}, expected ${newState}`
    );
  }

  getHoverElementRect() {
    return ContentTask.spawn(this.browser, null, async () => {
      let screenshotsChild = content.windowGlobalChild.getActor(
        "ScreenshotsComponent"
      );
      return screenshotsChild.overlay.hoverElementRegion.dimensions;
    });
  }

  isHoverElementRegionValid() {
    return ContentTask.spawn(this.browser, null, async () => {
      let screenshotsChild = content.windowGlobalChild.getActor(
        "ScreenshotsComponent"
      );
      return screenshotsChild.overlay.hoverElementRegion.isRegionValid;
    });
  }

  async waitForHoverElementRect(expectedWidth, expectedHeight) {
    return SpecialPowers.spawn(
      this.browser,
      [expectedWidth, expectedHeight],
      async (width, height) => {
        let screenshotsChild = content.windowGlobalChild.getActor(
          "ScreenshotsComponent"
        );
        let dimensions;
        await ContentTaskUtils.waitForCondition(() => {
          dimensions = screenshotsChild.overlay.hoverElementRegion.dimensions;
          return dimensions.width === width && dimensions.height === height;
        }, "The hover element region is the expected width and height");
        return dimensions;
      }
    );
  }

  async waitForSelectionRegionSizeChange(currentWidth) {
    await ContentTask.spawn(
      this.browser,
      [currentWidth],
      async ([currWidth]) => {
        let screenshotsChild = content.windowGlobalChild.getActor(
          "ScreenshotsComponent"
        );

        let dimensions = screenshotsChild.overlay.selectionRegion.dimensions;
        await ContentTaskUtils.waitForCondition(() => {
          dimensions = screenshotsChild.overlay.selectionRegion.dimensions;
          return dimensions.width !== currWidth;
        }, "Wait for selection box width change");
      }
    );
  }

  /**
   * This will drag an overlay starting at the given startX and startY coordinates and ending
   * at the given endX and endY coordinates.
   *
   * endY should be at least 70px from the bottom of window and endX should be at least
   * 265px from the left of the window. If these requirements are not met then the
   * overlay buttons (cancel, copy, download) will be positioned different from the default
   * and the methods to click the overlay buttons will not work unless the updated
   * position coordinates are supplied.
   * See https://searchfox.org/mozilla-central/rev/af78418c4b5f2c8721d1a06486cf4cf0b33e1e8d/browser/components/screenshots/ScreenshotsOverlayChild.sys.mjs#1789,1798
   * for how the overlay buttons are positioned when the overlay rect is near the bottom or
   * left edge of the window.
   *
   * Note: The distance of the rect should be greater than 40 to enter in the "dragging" state.
   * See https://searchfox.org/mozilla-central/rev/af78418c4b5f2c8721d1a06486cf4cf0b33e1e8d/browser/components/screenshots/ScreenshotsOverlayChild.sys.mjs#809
   * @param {Number} startX The starting X coordinate. The left edge of the overlay rect.
   * @param {Number} startY The starting Y coordinate. The top edge of the overlay rect.
   * @param {Number} endX The end X coordinate. The right edge of the overlay rect.
   * @param {Number} endY The end Y coordinate. The bottom edge of the overlay rect.
   */
  async dragOverlay(
    startX,
    startY,
    endX,
    endY,
    expectedStartingState = "crosshairs"
  ) {
    await this.assertStateChange(expectedStartingState);

    mouse.down(startX, startY);

    await Promise.any([
      this.waitForStateChange("draggingReady"),
      this.waitForStateChange("resizing"),
    ]);
    Assert.ok(true, "The overlay is in the draggingReady or resizing state");

    mouse.move(endX, endY);

    await Promise.any([
      this.waitForStateChange("dragging"),
      this.waitForStateChange("resizing"),
    ]);
    Assert.ok(true, "The overlay is in the dragging or resizing state");
    // We intentionally turn off this a11y check, because the following mouse
    // event is emitted at the end of the dragging event. Its keyboard
    // accessible alternative is provided and tested elsewhere, therefore
    // this rule check shall be ignored by a11y_checks suite.
    AccessibilityUtils.setEnv({ mustHaveAccessibleRule: false });
    mouse.up(endX, endY);

    await this.assertStateChange("selected");
    AccessibilityUtils.resetEnv();

    this.endX = endX;
    this.endY = endY;
  }

  async moveOverlayViaKeyboard(mover, events) {
    await SpecialPowers.spawn(
      this.browser,
      [mover, events],
      async (moverToFocus, eventsArr) => {
        let screenshotsChild = content.windowGlobalChild.getActor(
          "ScreenshotsComponent"
        );

        let overlay = screenshotsChild.overlay;

        switch (moverToFocus) {
          case "highlight":
            overlay.highlightEl.focus({ focusVisible: true });
            break;
          case "mover-bottomLeft":
            overlay.bottomLeftMover.focus({ focusVisible: true });
            break;
          case "mover-bottomRight":
            overlay.bottomRightMover.focus({ focusVisible: true });
            break;
          case "mover-topLeft":
            overlay.topLeftMover.focus({ focusVisible: true });
            break;
          case "mover-topRight":
            overlay.topRightMover.focus({ focusVisible: true });
            break;
        }
        screenshotsChild.overlay.highlightEl.focus();

        for (let event of eventsArr) {
          EventUtils.synthesizeKey(
            event.key,
            { type: "keydown", ...event.options },
            content
          );

          await ContentTaskUtils.waitForCondition(
            () => overlay.state === "resizing",
            "Wait for overlay state to be resizing"
          );

          EventUtils.synthesizeKey(
            event.key,
            { type: "keyup", ...event.options },
            content
          );

          await ContentTaskUtils.waitForCondition(
            () => overlay.state === "selected",
            "Wait for overlay state to be selected"
          );
        }
      }
    );
  }

  async scrollContentWindow(x, y) {
    let promise = BrowserTestUtils.waitForContentEvent(this.browser, "scroll");
    let contentDims = await this.getContentDimensions();
    await ContentTask.spawn(
      this.browser,
      [x, y, contentDims],
      async ([xPos, yPos, cDims]) => {
        content.window.scroll(xPos, yPos);

        info(JSON.stringify(cDims, null, 2));
        const scrollbarHeight = {};
        const scrollbarWidth = {};
        content.window.windowUtils.getScrollbarSize(
          false,
          scrollbarWidth,
          scrollbarHeight
        );

        await ContentTaskUtils.waitForCondition(() => {
          function isCloseEnough(a, b, diff) {
            return Math.abs(a - b) <= diff;
          }

          info(
            `scrollbarWidth: ${scrollbarWidth.value}, scrollbarHeight: ${scrollbarHeight.value}`
          );
          info(
            `scrollX: ${content.window.scrollX}, scrollY: ${content.window.scrollY}, scrollMaxX: ${content.window.scrollMaxX}, scrollMaxY: ${content.window.scrollMaxY}`
          );

          // Sometimes (read intermittently) the scroll width/height will be
          // off by the width/height of the scrollbar when we are expecting the
          // page to be scrolled to the very end. To mitigate this, we check
          // that the below differences are within the scrollbar width/height.
          return (
            (content.window.scrollX === xPos ||
              isCloseEnough(
                cDims.clientWidth + content.window.scrollX,
                cDims.scrollWidth,
                scrollbarWidth.value + 1
              )) &&
            (content.window.scrollY === yPos ||
              isCloseEnough(
                cDims.clientHeight + content.window.scrollY,
                cDims.scrollHeight,
                scrollbarHeight.value + 1
              ))
          );
        }, `Waiting for window to scroll to ${xPos}, ${yPos}`);
      }
    );
    await promise;
  }

  async waitForScrollTo(x, y) {
    await ContentTask.spawn(this.browser, [x, y], async ([xPos, yPos]) => {
      await ContentTaskUtils.waitForCondition(() => {
        info(
          `Got scrollX: ${content.window.scrollX}. scrollY: ${content.window.scrollY}`
        );
        return (
          content.window.scrollX === xPos && content.window.scrollY === yPos
        );
      }, `Waiting for window to scroll to ${xPos}, ${yPos}`);
    });
  }

  async resizeContentWindow(width, height) {
    this.browser.ownerGlobal.resizeTo(width, height);
    await TestUtils.waitForCondition(
      () => window.outerHeight === height && window.outerWidth === width,
      "Waiting for window to resize"
    );
  }

  async clickDownloadButton() {
    let { centerX: x, centerY: y } = await ContentTask.spawn(
      this.browser,
      null,
      async () => {
        let screenshotsChild = content.windowGlobalChild.getActor(
          "ScreenshotsComponent"
        );
        let { left, top, width, height } =
          screenshotsChild.overlay.downloadButton.getBoundingClientRect();
        let centerX = left + width / 2;
        let centerY = top + height / 2;
        return { centerX, centerY };
      }
    );

    info(`clicking download button at ${x}, ${y}`);
    mouse.click(x, y);
  }

  async clickCopyButton() {
    let { centerX: x, centerY: y } = await ContentTask.spawn(
      this.browser,
      null,
      async () => {
        let screenshotsChild = content.windowGlobalChild.getActor(
          "ScreenshotsComponent"
        );
        let { left, top, width, height } =
          screenshotsChild.overlay.copyButton.getBoundingClientRect();
        let centerX = left + width / 2;
        let centerY = top + height / 2;
        return { centerX, centerY };
      }
    );

    info(`clicking copy button at ${x}, ${y}`);
    mouse.click(x, y);
  }

  async clickCancelButton() {
    let { centerX: x, centerY: y } = await ContentTask.spawn(
      this.browser,
      null,
      async () => {
        let screenshotsChild = content.windowGlobalChild.getActor(
          "ScreenshotsComponent"
        );
        let { left, top, width, height } =
          screenshotsChild.overlay.cancelButton.getBoundingClientRect();
        let centerX = left + width / 2;
        let centerY = top + height / 2;
        return { centerX, centerY };
      }
    );

    info(`clicking cancel button at ${x}, ${y}`);
    mouse.click(x, y, {}, this.browser);
  }

  async clickPreviewCancelButton() {
    let { centerX: x, centerY: y } = await ContentTask.spawn(
      this.browser,
      null,
      async () => {
        let screenshotsChild = content.windowGlobalChild.getActor(
          "ScreenshotsComponent"
        );
        let { left, top, width, height } =
          screenshotsChild.overlay.previewCancelButton.getBoundingClientRect();
        let centerX = left + width / 2;
        let centerY = top + height / 2;
        return { centerX, centerY };
      }
    );

    info(`clicking cancel button at ${x}, ${y}`);
    mouse.click(x, y);
  }

  escapeKeyInContent() {
    return SpecialPowers.spawn(this.browser, [], () => {
      EventUtils.synthesizeKey("KEY_Escape", {}, content);
    });
  }

  getTestPageElementRect(elementId = "testPageElement") {
    return ContentTask.spawn(this.browser, [elementId], async id => {
      let ele = content.document.getElementById(id);
      return ele.getBoundingClientRect();
    });
  }

  getOverlaySelectionSizeText(elementId = "testPageElement") {
    return ContentTask.spawn(this.browser, [elementId], async () => {
      let screenshotsChild = content.windowGlobalChild.getActor(
        "ScreenshotsComponent"
      );
      return screenshotsChild.overlay.selectionSize.textContent;
    });
  }

  async clickTestPageElement(elementId = "testPageElement") {
    let rect = await this.getTestPageElementRect(elementId);
    let dims = await this.getContentDimensions();

    let x = Math.floor(rect.x + dims.scrollX + rect.width / 2);
    let y = Math.floor(rect.y + dims.scrollY + rect.height / 2);

    mouse.move(x, y);
    await this.waitForHoverElementRect(rect.width, rect.height);
    mouse.down(x, y);
    await this.assertStateChange("draggingReady");
    mouse.up(x, y);
    await this.assertStateChange("selected");
  }

  async zoomBrowser(zoom) {
    let promise = BrowserTestUtils.waitForContentEvent(this.browser, "resize");
    await SpecialPowers.spawn(this.browser, [zoom], zoomLevel => {
      const { Layout } = ChromeUtils.importESModule(
        "chrome://mochitests/content/browser/accessible/tests/browser/Layout.sys.mjs"
      );
      Layout.zoomDocument(content.document, zoomLevel);
    });
    await promise;
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

  assertPanelVisible() {
    info("assertPanelVisible, panel.hidden:" + this.panel?.hidden);
    Assert.ok(
      BrowserTestUtils.isVisible(this.panel),
      "Screenshots panel is visible"
    );
  }

  assertPanelNotVisible() {
    info("assertPanelNotVisible, panel.hidden:" + this.panel?.hidden);
    Assert.ok(
      !this.panel || BrowserTestUtils.isHidden(this.panel),
      "Screenshots panel is not visible"
    );
  }

  /**
   * Copied from screenshots extension
   * Returns a promise that resolves when the clipboard data has changed
   * Otherwise rejects
   */
  waitForRawClipboardChange(epectedWidth, expectedHeight) {
    const initialClipboardData = Date.now().toString();
    SpecialPowers.clipboardCopyString(initialClipboardData);

    return TestUtils.waitForCondition(
      async () => {
        let data;
        try {
          data = await this.getImageSizeAndColorFromClipboard();
        } catch (e) {
          console.log("Failed to get image/png clipboard data:", e);
          return false;
        }
        if (
          data &&
          initialClipboardData !== data &&
          data.height === expectedHeight &&
          data.width === epectedWidth
        ) {
          return data;
        }
        return false;
      },
      "Waiting for screenshot to copy to clipboard",
      200
    );
  }

  /**
   * Gets the client and scroll demensions on the window
   * @returns { Object }
   *   clientHeight The visible height
   *   clientWidth The visible width
   *   scrollHeight The scrollable height
   *   scrollWidth The scrollable width
   *   scrollX The scroll x position
   *   scrollY The scroll y position
   */
  getContentDimensions() {
    return SpecialPowers.spawn(this.browser, [], async function () {
      let {
        innerWidth,
        innerHeight,
        scrollMaxX,
        scrollMaxY,
        scrollX,
        scrollY,
      } = content.window;
      let width = innerWidth + scrollMaxX;
      let height = innerHeight + scrollMaxY;

      const scrollbarHeight = {};
      const scrollbarWidth = {};
      content.window.windowUtils.getScrollbarSize(
        false,
        scrollbarWidth,
        scrollbarHeight
      );
      width -= scrollbarWidth.value;
      height -= scrollbarHeight.value;
      innerWidth -= scrollbarWidth.value;
      innerHeight -= scrollbarHeight.value;

      return {
        clientHeight: innerHeight,
        clientWidth: innerWidth,
        scrollHeight: height,
        scrollWidth: width,
        scrollX,
        scrollY,
      };
    });
  }

  async getScreenshotsOverlayDimensions() {
    return ContentTask.spawn(this.browser, null, async () => {
      let screenshotsChild = content.windowGlobalChild.getActor(
        "ScreenshotsComponent"
      );
      Assert.ok(screenshotsChild.overlay.initialized, "The overlay exists");

      let screenshotsContainer = screenshotsChild.overlay.screenshotsContainer;

      await ContentTaskUtils.waitForCondition(() => {
        return !screenshotsContainer.hasAttribute("resizing");
      }, "Waiting for overlay to be done resizing");

      info(
        `${screenshotsContainer.style.width} ${
          screenshotsContainer.style.height
        } ${screenshotsContainer.hasAttribute("resizing")}`
      );

      return {
        scrollWidth: screenshotsContainer.scrollWidth,
        scrollHeight: screenshotsContainer.scrollHeight,
      };
    });
  }

  async waitForSelectionLayerDimensionChange(oldWidth, oldHeight) {
    await ContentTask.spawn(
      this.browser,
      [oldWidth, oldHeight],
      async ([prevWidth, prevHeight]) => {
        let screenshotsChild = content.windowGlobalChild.getActor(
          "ScreenshotsComponent"
        );

        await ContentTaskUtils.waitForCondition(() => {
          let screenshotsContainer =
            screenshotsChild.overlay.screenshotsContainer;
          info(
            `old height: ${prevHeight}. new height: ${screenshotsContainer.scrollHeight}.\nold width: ${prevWidth}. new width: ${screenshotsContainer.scrollWidth}`
          );
          return (
            screenshotsContainer.scrollHeight !== prevHeight &&
            screenshotsContainer.scrollWidth !== prevWidth
          );
        }, "Wait for selection box width change");
      }
    );
  }

  waitForOverlaySizeChangeTo(width, height) {
    return ContentTask.spawn(
      this.browser,
      [width, height],
      async ([newWidth, newHeight]) => {
        await ContentTaskUtils.waitForCondition(() => {
          let {
            innerHeight,
            innerWidth,
            scrollMaxY,
            scrollMaxX,
            scrollMinY,
            scrollMinX,
          } = content.window;
          let scrollWidth = innerWidth + scrollMaxX - scrollMinX;
          let scrollHeight = innerHeight + scrollMaxY - scrollMinY;

          const scrollbarHeight = {};
          const scrollbarWidth = {};
          content.window.windowUtils.getScrollbarSize(
            false,
            scrollbarWidth,
            scrollbarHeight
          );
          scrollWidth -= scrollbarWidth.value;
          scrollHeight -= scrollbarHeight.value;
          info(
            `${scrollHeight}, ${newHeight}, ${scrollWidth}, ${newWidth}, ${content.window.scrollMaxX}`
          );
          return scrollHeight === newHeight && scrollWidth === newWidth;
        }, "Wait for document size change");
      }
    );
  }

  getSelectionRegionDimensions() {
    return ContentTask.spawn(this.browser, null, async () => {
      let screenshotsChild = content.windowGlobalChild.getActor(
        "ScreenshotsComponent"
      );
      Assert.ok(screenshotsChild.overlay.initialized, "The overlay exists");

      return screenshotsChild.overlay.selectionRegion.dimensions;
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
    if (!image) {
      return false;
    }

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
    info(
      `${binaryStream.readArrayBuffer(
        available,
        buffer
      )} read, ${available} available`
    );

    // We are going to load the image in the content page to measure its size.
    // We don't want to insert the image directly in the browser's document
    // which could mess all sorts of things up
    return SpecialPowers.spawn(
      this.browser,
      [buffer],
      async function (_buffer) {
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
      }
    );
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
  Services.clipboard.getData(
    xferable,
    whichClipboard,
    SpecialPowers.wrap(window).browsingContext.currentWindowContext
  );
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
  options = {},
  browser
) {
  let context;
  if (!browser) {
    context = gBrowser.selectedBrowser.browsingContext;
  } else {
    context = browser.browsingContext;
  }
  BrowserTestUtils.synthesizeMouse(selector, x, y, options, context);
}

add_setup(async () => {
  CustomizableUI.addWidgetToArea(
    "screenshot-button",
    CustomizableUI.AREA_NAVBAR,
    0
  );
  let screenshotBtn = document.getElementById("screenshot-button");
  Assert.ok(screenshotBtn, "The screenshots button was added to the nav bar");
});

function getContentDevicePixelRatio(browser) {
  return SpecialPowers.spawn(browser, [], async function () {
    return content.window.devicePixelRatio;
  });
}

async function clearAllTelemetryEvents() {
  // Clear everything.
  info("Clearing all telemetry events");
  await TestUtils.waitForCondition(() => {
    Services.telemetry.clearEvents();
    let events = Services.telemetry.snapshotEvents(
      Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
      true
    );
    let content = events.content;
    let parent = events.parent;

    return (!content && !parent) || (!content.length && !parent.length);
  });
}

async function waitForScreenshotsEventCount(count, process = "parent") {
  await TestUtils.waitForCondition(
    () => {
      let events = TelemetryTestUtils.getEvents(
        { category: "screenshots" },
        { process }
      );

      info(`Got ${events?.length} event(s)`);
      info(`Actual events: ${JSON.stringify(events, null, 2)}`);
      return events.length === count ? events : null;
    },
    `Waiting for ${count} ${process} event(s).`,
    200,
    100
  );
}

async function assertScreenshotsEvents(
  expectedEvents,
  process = "parent",
  clearEvents = true
) {
  info(`Expected events: ${JSON.stringify(expectedEvents, null, 2)}`);
  // Make sure we have recorded the correct number of events
  await waitForScreenshotsEventCount(expectedEvents.length, process);

  TelemetryTestUtils.assertEvents(
    expectedEvents,
    { category: "screenshots" },
    { clear: clearEvents, process }
  );
}
