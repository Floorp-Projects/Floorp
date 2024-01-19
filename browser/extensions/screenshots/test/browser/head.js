"use strict";

const TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);
const TEST_GREEN_PAGE = TEST_ROOT + "green2vh.html";

const gScreenshotUISelectors = {
  preselectIframe: "#firefox-screenshots-preselection-iframe",
  fullPageButton: "button.full-page",
  visiblePageButton: "button.visible",
  previewIframe: "#firefox-screenshots-preview-iframe",
  copyButton: "button.highlight-button-copy",
  downloadButton: "button.highlight-button-download",
};

class ScreenshotsHelper {
  constructor(browser) {
    this.browser = browser;
    this.selector = gScreenshotUISelectors;
  }

  get toolbarButton() {
    return document.getElementById("screenshot-button");
  }

  async triggerUIFromToolbar() {
    let button = this.toolbarButton;
    ok(
      BrowserTestUtils.isVisible(button),
      "The screenshot toolbar button is visible"
    );
    EventUtils.synthesizeMouseAtCenter(button, {});
    // Make sure the Screenshots UI is loaded before yielding
    await this.waitForUIContent(
      this.selector.preselectIframe,
      this.selector.fullPageButton
    );
  }

  async waitForUIContent(iframeSel, elemSel) {
    await SpecialPowers.spawn(
      this.browser,
      [iframeSel, elemSel],
      async function (iframeSelector, elemSelector) {
        info(
          `in waitForUIContent content function, iframeSelector: ${iframeSelector}, elemSelector: ${elemSelector}`
        );
        let iframe;
        await ContentTaskUtils.waitForCondition(() => {
          iframe = content.document.querySelector(iframeSelector);
          if (!iframe || !ContentTaskUtils.isVisible(iframe)) {
            info("in waitForUIContent, no visible iframe yet");
            return false;
          }
          let elem = iframe.contentDocument.querySelector(elemSelector);
          info(
            "in waitForUIContent, got visible elem: " +
              (elem && ContentTaskUtils.isVisible(elem))
          );
          return elem && ContentTaskUtils.isVisible(elem);
        });
        // wait a frame for the screenshots UI to finish any init
        await new content.Promise(res => content.requestAnimationFrame(res));
      }
    );
  }

  async clickUIElement(iframeSel, elemSel) {
    await SpecialPowers.spawn(
      this.browser,
      [iframeSel, elemSel],
      async function (iframeSelector, elemSelector) {
        info(
          `in clickScreenshotsUIElement content function, iframeSelector: ${iframeSelector}, elemSelector: ${elemSelector}`
        );
        const EventUtils = ContentTaskUtils.getEventUtils(content);
        let iframe = content.document.querySelector(iframeSelector);
        let elem = iframe.contentDocument.querySelector(elemSelector);
        info(`Found the thing to click: ${elemSelector}: ${!!elem}`);

        EventUtils.synthesizeMouseAtCenter(elem, {}, iframe.contentWindow);
        await new content.Promise(res => content.requestAnimationFrame(res));
      }
    );
  }

  waitForToolbarButtonDeactivation() {
    return BrowserTestUtils.waitForCondition(() => {
      return !this.toolbarButton.style.cssText.includes("icon-highlight");
    });
  }

  getContentDimensions() {
    return SpecialPowers.spawn(this.browser, [], async function () {
      let doc = content.document;
      let rect = doc.documentElement.getBoundingClientRect();
      return {
        clientHeight: doc.documentElement.clientHeight,
        clientWidth: doc.documentElement.clientWidth,
        currentURI: doc.documentURI,
        documentHeight: Math.round(rect.height),
        documentWidth: Math.round(rect.width),
      };
    });
  }
}

function waitForRawClipboardChange() {
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
    xferable.getTransferData(flavor, data);
  } catch (e) {}
  data = data.value || null;
  return data;
}

/**
 * A helper that returns the size of the image that was just put into the clipboard by the
 * :screenshot command.
 * @return The {width, height} dimension object.
 */
async function getImageSizeFromClipboard(browser) {
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
  return SpecialPowers.spawn(browser, [buffer], async function (_buffer) {
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

    img.remove();
    content.URL.revokeObjectURL(url);

    // TODO: could get pixel data as well so we can check colors at specific locations
    return {
      width: img.width,
      height: img.height,
    };
  });
}

add_setup(async function common_initialize() {
  // Ensure Screenshots is initially enabled for all tests
  const addon = await AddonManager.getAddonByID("screenshots@mozilla.org");
  const isEnabled = addon.enabled;
  if (!isEnabled) {
    await addon.enable({ allowSystemAddons: true });
    registerCleanupFunction(async () => {
      await addon.disable({ allowSystemAddons: true });
    });
  }
  // Add the Screenshots button to the toolbar for all tests
  CustomizableUI.addWidgetToArea(
    "screenshot-button",
    CustomizableUI.AREA_NAVBAR
  );
  registerCleanupFunction(() =>
    CustomizableUI.removeWidgetFromArea("screenshot-button")
  );
  let screenshotBtn = document.getElementById("screenshot-button");
  Assert.ok(screenshotBtn, "The screenshots button was added to the nav bar");
});
