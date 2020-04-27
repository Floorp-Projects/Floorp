/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.org"
);
const TEST_PATH2 = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

var MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);

registerCleanupFunction(async function() {
  info("Running the cleanup code");
  MockFilePicker.cleanup();
  if (gTestDir && gTestDir.exists()) {
    // On Windows, sometimes nsIFile.remove() throws, probably because we're
    // still writing to the directory we're trying to remove, despite
    // waiting for the download to complete. Just retry a bit later...
    let succeeded = false;
    while (!succeeded) {
      try {
        gTestDir.remove(true);
        succeeded = true;
      } catch (ex) {
        await new Promise(requestAnimationFrame);
      }
    }
  }
});

let gTestDir = null;

function createTemporarySaveDirectory() {
  var saveDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  saveDir.append("testsavedir");
  if (!saveDir.exists()) {
    saveDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  }
  return saveDir;
}

function canonicalizeExtension(str) {
  return str.replace(/\.htm$/, ".html");
}

function checkContents(dir, expected, str) {
  let stack = [dir];
  let files = [];
  while (stack.length) {
    for (let file of stack.pop().directoryEntries) {
      if (file.isDirectory()) {
        stack.push(file);
      }

      let path = canonicalizeExtension(file.getRelativePath(dir));
      files.push(path);
    }
  }

  SimpleTest.isDeeply(
    files.sort(),
    expected.sort(),
    str + "Should contain downloaded files in correct place."
  );
}

async function addFrame(browser, path, selector) {
  await SpecialPowers.spawn(browser, [path, selector], async function(
    path,
    selector
  ) {
    let document = content.document;
    let target = document.querySelector(selector);
    if (target instanceof content.HTMLIFrameElement) {
      document = target.contentDocument;
      target = document.body;
    }
    let element = document.createElement("iframe");
    element.src = path;
    await new Promise(resolve => {
      element.onload = resolve;
      target.appendChild(element);
    });
  });
}

async function handleResult(expected, str) {
  let dls = await Downloads.getList(Downloads.PUBLIC);
  return new Promise((resolve, reject) => {
    dls.addView({
      onDownloadChanged(download) {
        if (download.succeeded) {
          checkContents(gTestDir, expected, str);

          dls.removeView(this);
          dls.removeFinished();
          resolve();
        } else if (download.error) {
          reject("Download failed");
        }
      },
    });
  });
}

add_task(async function() {
  await BrowserTestUtils.withNewTab(TEST_PATH + "image.html", async function(
    browser
  ) {
    await addFrame(browser, TEST_PATH + "image.html", "body");
    await addFrame(browser, TEST_PATH2 + "image.html", "body>iframe");

    gTestDir = createTemporarySaveDirectory();

    MockFilePicker.displayDirectory = gTestDir;
    MockFilePicker.showCallback = function(fp) {
      let destFile = gTestDir.clone();
      destFile.append("first.html");
      MockFilePicker.setFiles([destFile]);
      MockFilePicker.filterIndex = 0; // kSaveAsType_Complete
    };

    let expected = [
      "first.html",
      "first_files",
      "first_files/image.html",
      "first_files/dummy.png",
      "first_files/image_data",
      "first_files/image_data/image.html",
      "first_files/image_data/image_data",
      "first_files/image_data/image_data/dummy.png",
    ];

    // This saves the top-level document contained in `browser`
    saveBrowser(browser);
    await handleResult(expected, "Check toplevel: ");

    // Instead of deleting previously saved files, we update our list
    // of expected files for the next part of the test. To not clash
    // we make sure to save to a different file name.
    expected = expected.concat([
      "second.html",
      "second_files",
      "second_files/dummy.png",
      "second_files/image.html",
      "second_files/image_data",
      "second_files/image_data/dummy.png",
    ]);

    MockFilePicker.showCallback = function(fp) {
      let destFile = gTestDir.clone();
      destFile.append("second.html");
      MockFilePicker.setFiles([destFile]);
      MockFilePicker.filterIndex = 0; // kSaveAsType_Complete
    };

    // This saves the sub-document of the iframe contained in the
    // top-level document, as indicated by passing a child browsing
    // context as target for the save.
    saveBrowser(browser, false, browser.browsingContext.children[0]);
    await handleResult(expected, "Check subframe: ");

    // Instead of deleting previously saved files, we update our list
    // of expected files for the next part of the test. To not clash
    // we make sure to save to a different file name.
    expected = expected.concat([
      "third.html",
      "third_files",
      "third_files/dummy.png",
    ]);

    MockFilePicker.showCallback = function(fp) {
      let destFile = gTestDir.clone();
      destFile.append("third.html");
      MockFilePicker.setFiles([destFile]);
      MockFilePicker.filterIndex = 0; // kSaveAsType_Complete
    };

    // This saves the sub-document of the iframe contained in the
    // first sub-document, as indicated by passing a child browsing
    // context as target for the save. That frame is special, because
    // it's cross-process.
    saveBrowser(
      browser,
      false,
      browser.browsingContext.children[0].children[0]
    );
    await handleResult(expected, "Check subframe: ");
  });
});
