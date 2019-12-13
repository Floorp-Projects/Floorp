/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that screenshot command works properly

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test_jsterm_screenshot_command.html";

// on some machines, such as macOS, dpr is set to 2. This is expected behavior, however
// to keep tests consistant across OSs we are setting the dpr to 1
const dpr = "--dpr 1";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  ok(hud, "web console opened");

  await testClipboard(hud);
  await testFullpageClipboard(hud);
  await testSelectorClipboard(hud);

  // overflow
  await createScrollbarOverflow();
  await testFullpageClipboardScrollbar(hud);
});

async function testClipboard(hud) {
  const command = `:screenshot --clipboard ${dpr}`;
  await executeScreenshotClipboardCommand(hud, command);
  const contentSize = await getContentSize();
  const imgSize = await getImageSizeFromClipboard();

  is(
    imgSize.width,
    contentSize.innerWidth,
    "Clipboard: Image width matches window size"
  );
  is(
    imgSize.height,
    contentSize.innerHeight,
    "Clipboard: Image height matches window size"
  );
}

async function testFullpageClipboard(hud) {
  const command = `:screenshot --fullpage --clipboard ${dpr}`;
  await executeScreenshotClipboardCommand(hud, command);
  const contentSize = await getContentSize();
  const imgSize = await getImageSizeFromClipboard();

  is(
    imgSize.width,
    contentSize.innerWidth + contentSize.scrollMaxX - contentSize.scrollMinX,
    "Fullpage Clipboard: Image width matches page size"
  );
  is(
    imgSize.height,
    contentSize.innerHeight + contentSize.scrollMaxY - contentSize.scrollMinY,
    "Fullpage Clipboard: Image height matches page size"
  );
}

async function testSelectorClipboard(hud) {
  const command = `:screenshot --selector "img#testImage" --clipboard ${dpr}`;
  await executeScreenshotClipboardCommand(hud, command);

  const imgSize1 = await getImageSizeFromClipboard();
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [imgSize1], function(
    imgSize
  ) {
    const img = content.document.querySelector("#testImage");
    is(
      imgSize.width,
      img.clientWidth,
      "Selector Clipboard: Image width matches element size"
    );
    is(
      imgSize.height,
      img.clientHeight,
      "Selector Clipboard: Image height matches element size"
    );
  });
}

async function testFullpageClipboardScrollbar(hud) {
  const command = `:screenshot --fullpage --clipboard ${dpr}`;
  await executeScreenshotClipboardCommand(hud, command);
  const contentSize = await getContentSize();
  const scrollbarSize = await getScrollbarSize();
  const imgSize = await getImageSizeFromClipboard();

  is(
    imgSize.width,
    contentSize.innerWidth +
      contentSize.scrollMaxX -
      contentSize.scrollMinX -
      scrollbarSize.width,
    "Scroll Fullpage Clipboard: Image width matches page size minus scrollbar size"
  );
  is(
    imgSize.height,
    contentSize.innerHeight +
      contentSize.scrollMaxY -
      contentSize.scrollMinY -
      scrollbarSize.height,
    "Scroll Fullpage Clipboard: Image height matches page size minus scrollbar size"
  );
}

/**
 * Executes the command string and returns a Promise that resolves when the message
 * saying that the screenshot was copied to clipboard is rendered in the console.
 *
 * @param {WebConsole} hud
 * @param {String} command
 */
function executeScreenshotClipboardCommand(hud, command) {
  return executeAndWaitForMessage(
    hud,
    command,
    "Screenshot copied to clipboard."
  );
}

async function createScrollbarOverflow() {
  // Trigger scrollbars by forcing document to overflow
  // This only affects results on OSes with scrollbars that reduce document size
  // (non-floating scrollbars).  With default OS settings, this means Windows
  // and Linux are affected, but Mac is not.  For Mac to exhibit this behavior,
  // change System Preferences -> General -> Show scroll bars to Always.
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    content.document.body.classList.add("overflow");
  });
}

async function getScrollbarSize() {
  const scrollbarSize = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    function() {
      const winUtils = content.windowUtils;
      const scrollbarHeight = {};
      const scrollbarWidth = {};
      winUtils.getScrollbarSize(true, scrollbarWidth, scrollbarHeight);
      return {
        width: scrollbarWidth.value,
        height: scrollbarHeight.value,
      };
    }
  );
  info(`Scrollbar size: ${scrollbarSize.width}x${scrollbarSize.height}`);
  return scrollbarSize;
}

async function getContentSize() {
  const contentSize = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    function() {
      return {
        scrollMaxY: content.scrollMaxY,
        scrollMaxX: content.scrollMaxX,
        scrollMinY: content.scrollMinY,
        scrollMinX: content.scrollMinX,
        innerWidth: content.innerWidth,
        innerHeight: content.innerHeight,
      };
    }
  );

  info(`content size: ${contentSize.innerWidth}x${contentSize.innerHeight}`);
  return contentSize;
}

async function getImageSizeFromClipboard() {
  const clipid = Ci.nsIClipboard;
  const clip = Cc["@mozilla.org/widget/clipboard;1"].getService(clipid);
  const trans = Cc["@mozilla.org/widget/transferable;1"].createInstance(
    Ci.nsITransferable
  );
  const flavor = "image/png";
  trans.init(null);
  trans.addDataFlavor(flavor);

  clip.getData(trans, clipid.kGlobalClipboard);
  const data = {};
  trans.getTransferData(flavor, data);

  ok(data.value, "screenshot exists");

  let image = data.value;

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
  // (which is value of the global `document` here). Doing so might push the
  // toolbox upwards, shrink the content page and fail the fullpage screenshot
  // test.
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [buffer], async function(
    _buffer
  ) {
    const img = content.document.createElement("img");
    const loaded = new Promise(r => {
      img.addEventListener("load", r, { once: true });
    });

    // Build a URL from the buffer passed to the ContentTask
    const url = content.URL.createObjectURL(
      new Blob([_buffer], { type: "image/png" })
    );

    // Load the image
    img.src = url;
    content.document.documentElement.appendChild(img);

    info("Waiting for the clipboard image to load in the content page");
    await loaded;

    // Remove the image and revoke the URL.
    img.remove();
    content.URL.revokeObjectURL(url);

    return {
      width: img.width,
      height: img.height,
    };
  });
}
