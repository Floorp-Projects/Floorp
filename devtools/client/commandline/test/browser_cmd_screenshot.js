/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* global helpers, btoa, whenDelayedStartupFinished, OpenBrowserWindow */

// Test that screenshot command works properly
const TEST_URI = "http://example.com/browser/devtools/client/commandline/" +
                 "test/browser_cmd_screenshot.html";

var FileUtils = (Cu.import("resource://gre/modules/FileUtils.jsm", {})).FileUtils;

function test() {
  return Task.spawn(spawnTest).then(finish, helpers.handleError);
}

function* spawnTest() {
  waitForExplicitFinish();

  info("RUN TEST: non-private window");
  let normWin = yield addWindow({ private: false });
  yield addTabWithToolbarRunTests(normWin);
  normWin.close();

  info("RUN TEST: private window");
  let pbWin = yield addWindow({ private: true });
  yield addTabWithToolbarRunTests(pbWin);
  pbWin.close();
}

function* addTabWithToolbarRunTests(win) {
  let options = yield helpers.openTab(TEST_URI, { chromeWindow: win });
  let browser = options.browser;
  yield helpers.openToolbar(options);

  // Test input status
  yield helpers.audit(options, [
    {
      setup: "screenshot",
      check: {
        input:  "screenshot",
        markup: "VVVVVVVVVV",
        status: "VALID",
        args: {
        }
      },
    },
    {
      setup: "screenshot abc.png",
      check: {
        input:  "screenshot abc.png",
        markup: "VVVVVVVVVVVVVVVVVV",
        status: "VALID",
        args: {
          filename: { value: "abc.png"},
        }
      },
    },
    {
      setup: "screenshot --fullpage",
      check: {
        input:  "screenshot --fullpage",
        markup: "VVVVVVVVVVVVVVVVVVVVV",
        status: "VALID",
        args: {
          fullpage: { value: true},
        }
      },
    },
    {
      setup: "screenshot abc --delay 5",
      check: {
        input:  "screenshot abc --delay 5",
        markup: "VVVVVVVVVVVVVVVVVVVVVVVV",
        status: "VALID",
        args: {
          filename: { value: "abc"},
          delay: { value: 5 },
        }
      },
    },
    {
      setup: "screenshot --selector img#testImage",
      check: {
        input:  "screenshot --selector img#testImage",
        markup: "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV",
        status: "VALID",
      },
    },
  ]);

  // Test capture to file
  let file = FileUtils.getFile("TmpD", [ "TestScreenshotFile.png" ]);

  yield helpers.audit(options, [
    {
      setup: "screenshot " + file.path,
      check: {
        args: {
          filename: { value: "" + file.path },
          fullpage: { value: false },
          clipboard: { value: false },
          chrome: { value: false },
        },
      },
      exec: {
        output: new RegExp("^Saved to "),
      },
      post: function () {
        // Bug 849168: screenshot command tests fail in try but not locally
        // ok(file.exists(), "Screenshot file exists");

        if (file.exists()) {
          file.remove(false);
        }
      }
    },
  ]);

  // Test capture to clipboard
  yield helpers.audit(options, [
    {
      setup: "screenshot --clipboard",
      check: {
        args: {
          clipboard: { value: true },
          chrome: { value: false },
        },
      },
      exec: {
        output: new RegExp("^Copied to clipboard.$"),
      },
      post: Task.async(function* () {
        let imgSize = yield getImageSizeFromClipboard();
        yield ContentTask.spawn(browser, imgSize, function* (imgSize) {
          Assert.equal(imgSize.width, content.innerWidth, "Image width matches window size");
          Assert.equal(imgSize.height, content.innerHeight, "Image height matches window size");
        });
      })
    },
    {
      setup: "screenshot --fullpage --clipboard",
      check: {
        args: {
          fullpage: { value: true },
          clipboard: { value: true },
          chrome: { value: false },
        },
      },
      exec: {
        output: new RegExp("^Copied to clipboard.$"),
      },
      post: Task.async(function* () {
        let imgSize = yield getImageSizeFromClipboard();
        yield ContentTask.spawn(browser, imgSize, function* (imgSize) {
          Assert.equal(imgSize.width,
            content.innerWidth + content.scrollMaxX - content.scrollMinX,
            "Image width matches page size");
          Assert.equal(imgSize.height,
            content.innerHeight + content.scrollMaxY - content.scrollMinY,
            "Image height matches page size");
        });
      })
    },
    {
      setup: "screenshot --selector img#testImage --clipboard",
      check: {
        args: {
          clipboard: { value: true },
          chrome: { value: false },
        },
      },
      exec: {
        output: new RegExp("^Copied to clipboard.$"),
      },
      post: Task.async(function* () {
        let imgSize = yield getImageSizeFromClipboard();
        yield ContentTask.spawn(browser, imgSize, function* (imgSize) {
          let img = content.document.querySelector("img#testImage");
          Assert.equal(imgSize.width, img.clientWidth,
             "Image width matches element size");
          Assert.equal(imgSize.height, img.clientHeight,
             "Image height matches element size");
        });
      })
    },
  ]);

  // Trigger scrollbars by forcing document to overflow
  // This only affects results on OSes with scrollbars that reduce document size
  // (non-floating scrollbars).  With default OS settings, this means Windows
  // and Linux are affected, but Mac is not.  For Mac to exhibit this behavior,
  // change System Preferences -> General -> Show scroll bars to Always.
  yield ContentTask.spawn(browser, {}, function* () {
    content.document.body.classList.add("overflow");
  });

  let scrollbarSize = yield ContentTask.spawn(browser, {}, function* () {
    const winUtils = content.QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIDOMWindowUtils);
    let scrollbarHeight = {};
    let scrollbarWidth = {};
    winUtils.getScrollbarSize(true, scrollbarWidth, scrollbarHeight);
    return {
      width: scrollbarWidth.value,
      height: scrollbarHeight.value,
    };
  });

  info(`Scrollbar size: ${scrollbarSize.width}x${scrollbarSize.height}`);

  // Test capture to clipboard in presence of scrollbars
  yield helpers.audit(options, [
    {
      setup: "screenshot --clipboard",
      check: {
        args: {
          clipboard: { value: true },
          chrome: { value: false },
        },
      },
      exec: {
        output: new RegExp("^Copied to clipboard.$"),
      },
      post: Task.async(function* () {
        let imgSize = yield getImageSizeFromClipboard();
        imgSize.scrollbarWidth = scrollbarSize.width;
        imgSize.scrollbarHeight = scrollbarSize.height;
        yield ContentTask.spawn(browser, imgSize, function* (imgSize) {
          Assert.equal(imgSize.width, content.innerWidth - imgSize.scrollbarWidth,
             "Image width matches window size minus scrollbar size");
          Assert.equal(imgSize.height, content.innerHeight - imgSize.scrollbarHeight,
             "Image height matches window size minus scrollbar size");
        });
      })
    },
    {
      setup: "screenshot --fullpage --clipboard",
      check: {
        args: {
          fullpage: { value: true },
          clipboard: { value: true },
          chrome: { value: false },
        },
      },
      exec: {
        output: new RegExp("^Copied to clipboard.$"),
      },
      post: Task.async(function* () {
        let imgSize = yield getImageSizeFromClipboard();
        imgSize.scrollbarWidth = scrollbarSize.width;
        imgSize.scrollbarHeight = scrollbarSize.height;
        yield ContentTask.spawn(browser, imgSize, function* (imgSize) {
          Assert.equal(imgSize.width,
            (content.innerWidth + content.scrollMaxX - content.scrollMinX) - imgSize.scrollbarWidth,
            "Image width matches page size minus scrollbar size");
          Assert.equal(imgSize.height,
            (content.innerHeight + content.scrollMaxY - content.scrollMinY) - imgSize.scrollbarHeight,
            "Image height matches page size minus scrollbar size");
        });
      })
    },
    {
      setup: "screenshot --selector img#testImage --clipboard",
      check: {
        args: {
          clipboard: { value: true },
          chrome: { value: false },
        },
      },
      exec: {
        output: new RegExp("^Copied to clipboard.$"),
      },
      post: Task.async(function* () {
        let imgSize = yield getImageSizeFromClipboard();
        yield ContentTask.spawn(browser, imgSize, function* (imgSize) {
          let img = content.document.querySelector("img#testImage");
          Assert.equal(imgSize.width, img.clientWidth,
             "Image width matches element size");
          Assert.equal(imgSize.height, img.clientHeight,
             "Image height matches element size");
        });
      })
    },
  ]);

  yield helpers.closeToolbar(options);
  yield helpers.closeTab(options);
}

function addWindow(windowOptions) {
  return new Promise(resolve => {
    let win = OpenBrowserWindow(windowOptions);

    // This feels hacky, we should refactor it
    whenDelayedStartupFinished(win, () => {
      // Would like to get rid of this executeSoon, but without it the url
      // (TEST_URI) provided in addTabWithToolbarRunTests hasn't loaded
      executeSoon(() => {
        resolve(win);
      });
    });
  });
}

let getImageSizeFromClipboard = Task.async(function* () {
  let clipid = Ci.nsIClipboard;
  let clip = Cc["@mozilla.org/widget/clipboard;1"].getService(clipid);
  let trans = Cc["@mozilla.org/widget/transferable;1"]
                .createInstance(Ci.nsITransferable);
  let flavor = "image/png";
  trans.init(null);
  trans.addDataFlavor(flavor);

  clip.getData(trans, clipid.kGlobalClipboard);
  let data = new Object();
  let dataLength = new Object();
  trans.getTransferData(flavor, data, dataLength);

  ok(data.value, "screenshot exists");
  ok(dataLength.value > 0, "screenshot has length");

  let image = data.value;
  let dataURI = `data:${flavor};base64,`;

  // Due to the differences in how images could be stored in the clipboard the
  // checks below are needed. The clipboard could already provide the image as
  // byte streams, but also as pointer, or as image container. If it's not
  // possible obtain a byte stream, the function returns `null`.
  if (image instanceof Ci.nsISupportsInterfacePointer) {
    image = image.data;
  }

  if (image instanceof Ci.imgIContainer) {
    image = Cc["@mozilla.org/image/tools;1"]
              .getService(Ci.imgITools)
              .encodeImage(image, flavor);
  }

  if (image instanceof Ci.nsIInputStream) {
    let binaryStream = Cc["@mozilla.org/binaryinputstream;1"]
                         .createInstance(Ci.nsIBinaryInputStream);
    binaryStream.setInputStream(image);
    let rawData = binaryStream.readBytes(binaryStream.available());
    let charCodes = Array.from(rawData, c => c.charCodeAt(0) & 0xff);
    let encodedData = String.fromCharCode(...charCodes);
    encodedData = btoa(encodedData);
    dataURI = dataURI + encodedData;
  } else {
    throw new Error("Unable to read image data");
  }

  let img = document.createElementNS("http://www.w3.org/1999/xhtml", "img");

  let loaded = new Promise(resolve => {
    img.addEventListener("load", function onLoad() {
      img.removeEventListener("load", onLoad);
      resolve();
    });
  });

  img.src = dataURI;
  document.documentElement.appendChild(img);
  yield loaded;
  img.remove();

  return {
    width: img.width,
    height: img.height,
  };
});
