/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function createTemporarySaveDirectory() {
  var saveDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  saveDir.append("testsavedir");
  if (!saveDir.exists())
    saveDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  return saveDir;
}

function promiseNoCacheEntry(filename) {
  return new Promise((resolve, reject) => {
    Visitor.prototype = {
      onCacheStorageInfo(num, consumption) {
        info("disk storage contains " + num + " entries");
      },
      onCacheEntryInfo(uri) {
        let urispec = uri.asciiSpec;
        info(urispec);
        is(urispec.includes(filename), false, "web content present in disk cache");
      },
      onCacheEntryVisitCompleted() {
        resolve();
      }
    };
    function Visitor() {}

    let {LoadContextInfo} = Cu.import("resource://gre/modules/LoadContextInfo.jsm", null);
    let storage = Services.cache2.diskCacheStorage(LoadContextInfo.default, false);
    storage.asyncVisitStorage(new Visitor(), true /* Do walk entries */);
  });
}

function promiseImageDownloaded() {
  return new Promise((resolve, reject) => {
    let fileName;
    let MockFilePicker = SpecialPowers.MockFilePicker;
    MockFilePicker.init(window);

    function onTransferComplete(downloadSuccess) {
      ok(downloadSuccess, "Image file should have been downloaded successfully " + fileName);

      // Give the request a chance to finish and create a cache entry
      resolve(fileName);
    }

    // Create the folder the image will be saved into.
    var destDir = createTemporarySaveDirectory();
    var destFile = destDir.clone();

    MockFilePicker.displayDirectory = destDir;
    MockFilePicker.showCallback = function(fp) {
      fileName = fp.defaultString;
      destFile.append(fileName);
      MockFilePicker.setFiles([destFile]);
      MockFilePicker.filterIndex = 1; // kSaveAsType_URL
    };

    mockTransferCallback = onTransferComplete;
    mockTransferRegisterer.register();

    registerCleanupFunction(function() {
      mockTransferCallback = null;
      mockTransferRegisterer.unregister();
      MockFilePicker.cleanup();
      destDir.remove(true);
    });

  });
}

add_task(async function() {
  let testURI = "http://mochi.test:8888/browser/browser/base/content/test/general/bug792517.html";
  let privateWindow = await BrowserTestUtils.openNewBrowserWindow({private: true});
  let tab = await BrowserTestUtils.openNewForegroundTab(privateWindow.gBrowser, testURI);

  let contextMenu = privateWindow.document.getElementById("contentAreaContextMenu");
  let popupShown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  let popupHidden = BrowserTestUtils.waitForEvent(contextMenu, "popuphidden");
  await BrowserTestUtils.synthesizeMouseAtCenter("#img", {
    type: "contextmenu",
    button: 2
  }, tab.linkedBrowser);
  await popupShown;

  Services.cache2.clear();

  let imageDownloaded = promiseImageDownloaded();
  // Select "Save Image As" option from context menu
  privateWindow.document.getElementById("context-saveimage").doCommand();

  contextMenu.hidePopup();
  await popupHidden;

  // wait for image download
  let fileName = await imageDownloaded;
  await promiseNoCacheEntry(fileName);

  await BrowserTestUtils.closeWindow(privateWindow);
});

/* import-globals-from ../../../../../toolkit/content/tests/browser/common/mockTransfer.js */
Services.scriptloader.loadSubScript("chrome://mochitests/content/browser/toolkit/content/tests/browser/common/mockTransfer.js",
                 this);
