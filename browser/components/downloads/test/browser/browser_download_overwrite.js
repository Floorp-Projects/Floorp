/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

var MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/content/tests/browser/common/mockTransfer.js",
  this
);

add_setup(async function () {
  // head.js has helpers that write to a nice unique file we can use.
  await createDownloadedFile(gTestTargetFile.path, "Hello.\n");
  ok(gTestTargetFile.exists(), "We created a test file.");

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.always_ask_before_handling_new_types", false],
      ["browser.download.useDownloadDir", false],
    ],
  });
  // Set up the file picker.
  let destDir = gTestTargetFile.parent;

  MockFilePicker.displayDirectory = destDir;
  MockFilePicker.showCallback = function (fp) {
    MockFilePicker.setFiles([gTestTargetFile]);
    return MockFilePicker.returnOK;
  };
  registerCleanupFunction(function () {
    MockFilePicker.cleanup();
    if (gTestTargetFile.exists()) {
      gTestTargetFile.remove(false);
    }
  });
});

// If we download a file and the user accepts overwriting an existing one,
// we shouldn't first delete that file before moving the .part file into
// place.
add_task(async function test_overwrite_does_not_delete_first() {
  let unregisteredTransfer = false;
  let transferCompletePromise = new Promise(resolve => {
    mockTransferCallback = resolve;
  });
  mockTransferRegisterer.register();

  registerCleanupFunction(function () {
    if (!unregisteredTransfer) {
      mockTransferRegisterer.unregister();
    }
  });

  // Now try and download a thing to the file:
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      opening: TEST_ROOT + "foo.txt",
      waitForLoad: false,
      waitForStateStop: true,
    },
    async function () {
      ok(await transferCompletePromise, "download should succeed");
      ok(
        gTestTargetFile.exists(),
        "File should still exist and not have been deleted."
      );
      // Note: the download transfer is fake so data won't have been written to
      // the file, so we can't verify that the download actually overwrites data
      // like this.
      mockTransferRegisterer.unregister();
      unregisteredTransfer = true;
    }
  );
});

// If we download a file and the user accepts overwriting an existing one,
// we should successfully overwrite its contents.
add_task(async function test_overwrite_works() {
  let publicDownloads = await Downloads.getList(Downloads.PUBLIC);
  // First ensure we catch the download finishing.
  let downloadFinishedPromise = new Promise(resolve => {
    publicDownloads.addView({
      onDownloadChanged(download) {
        info("Download changed!");
        if (download.succeeded || download.error) {
          info("Download succeeded or errored");
          publicDownloads.removeView(this);
          publicDownloads.removeFinished();
          resolve(download);
        }
      },
    });
  });
  // Now try and download a thing to the file:
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      opening: TEST_ROOT + "foo.txt",
      waitForLoad: false,
      waitForStateStop: true,
    },
    async function () {
      info("wait for download to finish");
      let download = await downloadFinishedPromise;
      ok(download.succeeded, "Download should succeed");
      ok(
        gTestTargetFile.exists(),
        "File should still exist and not have been deleted."
      );
      let contents = new TextDecoder().decode(
        await IOUtils.read(gTestTargetFile.path)
      );
      info("Got: " + contents);
      ok(contents.startsWith("Dummy"), "The file was overwritten.");
    }
  );
});
