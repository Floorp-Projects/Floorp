/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function createTemporarySaveDirectory() {
  var saveDir = Cc["@mozilla.org/file/directory_service;1"]
                  .getService(Ci.nsIProperties)
                  .get("TmpD", Ci.nsIFile);
  saveDir.append("testsavedir");
  if (!saveDir.exists())
    saveDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  return saveDir;
}

function promiseNoCacheEntry(filename) {
  return new Promise((resolve, reject) => {
    Visitor.prototype = {
      onCacheStorageInfo: function(num, consumption)
      {
        info("disk storage contains " + num + " entries");
      },
      onCacheEntryInfo: function(uri)
      {
        let urispec = uri.asciiSpec;
        info(urispec);
        is(urispec.includes(filename), false, "web content present in disk cache");
      },
      onCacheEntryVisitCompleted: function()
      {
        resolve();
      }
    };
    function Visitor() {}

    let cache = Cc["@mozilla.org/netwerk/cache-storage-service;1"]
                .getService(Ci.nsICacheStorageService);
    let {LoadContextInfo} = Cu.import("resource://gre/modules/LoadContextInfo.jsm", null);
    let storage = cache.diskCacheStorage(LoadContextInfo.default, false);
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
      MockFilePicker.returnFiles = [destFile];
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

add_task(function* () {
  let testURI = "http://mochi.test:8888/browser/browser/base/content/test/general/bug792517.html";
  let privateWindow = yield BrowserTestUtils.openNewBrowserWindow({private: true});
  let tab = yield BrowserTestUtils.openNewForegroundTab(privateWindow.gBrowser, testURI);

  let contextMenu = privateWindow.document.getElementById("contentAreaContextMenu");
  let popupShown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  let popupHidden = BrowserTestUtils.waitForEvent(contextMenu, "popuphidden");
  yield BrowserTestUtils.synthesizeMouseAtCenter("#img", {
    type: "contextmenu",
    button: 2
  }, tab.linkedBrowser);
  yield popupShown;

  let cache = Cc["@mozilla.org/netwerk/cache-storage-service;1"]
              .getService(Ci.nsICacheStorageService);
  cache.clear();

  let imageDownloaded = promiseImageDownloaded();
  // Select "Save Image As" option from context menu
  privateWindow.document.getElementById("context-saveimage").doCommand();

  contextMenu.hidePopup();
  yield popupHidden;

  // wait for image download
  let fileName = yield imageDownloaded;
  yield promiseNoCacheEntry(fileName);

  yield BrowserTestUtils.closeWindow(privateWindow);
});

Cc["@mozilla.org/moz/jssubscript-loader;1"]
  .getService(Ci.mozIJSSubScriptLoader)
  .loadSubScript("chrome://mochitests/content/browser/toolkit/content/tests/browser/common/mockTransfer.js",
                 this);
