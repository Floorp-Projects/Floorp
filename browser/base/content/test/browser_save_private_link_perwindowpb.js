/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
function test() {
  // initialization
  waitForExplicitFinish();
  let windowsToClose = [];
  let testURI = "http://mochi.test:8888/browser/browser/base/content/test/bug792517.html";
  let fileName;
  let MockFilePicker = SpecialPowers.MockFilePicker;
  let cache = Cc["@mozilla.org/network/cache-service;1"]
              .getService(Ci.nsICacheService);

  function checkDiskCacheFor(filename) {
    let visitor = {
      visitDevice: function(deviceID, deviceInfo) {
        if (deviceID == "disk")
          info(deviceID + " device contains " + deviceInfo.entryCount + " entries");
        return deviceID == "disk";
      },

      visitEntry: function(deviceID, entryInfo) {
        info(entryInfo.key);
        is(entryInfo.key.contains(filename), false, "web content present in disk cache");
      }
    };
    cache.visitEntries(visitor);
  }

  function contextMenuOpened(aWindow, event) {
    cache.evictEntries(Ci.nsICache.STORE_ANYWHERE);

    event.currentTarget.removeEventListener("popupshown", contextMenuOpened);

    // Create the folder the image will be saved into.
    var destDir = createTemporarySaveDirectory();
    var destFile = destDir.clone();

    MockFilePicker.displayDirectory = destDir;
    MockFilePicker.showCallback = function(fp) {
      fileName = fp.defaultString;
      destFile.append (fileName);
      MockFilePicker.returnFiles = [destFile];
      MockFilePicker.filterIndex = 1; // kSaveAsType_URL
    };

    mockTransferCallback = onTransferComplete;
    mockTransferRegisterer.register();

    registerCleanupFunction(function () {
      mockTransferRegisterer.unregister();
      MockFilePicker.cleanup();
      destDir.remove(true);
    });

    // Select "Save Image As" option from context menu
    var saveVideoCommand = aWindow.document.getElementById("context-saveimage");
    saveVideoCommand.doCommand();

    event.target.hidePopup();
  }

  function onTransferComplete(downloadSuccess) {
    ok(downloadSuccess, "Image file should have been downloaded successfully");

    // Give the request a chance to finish and create a cache entry
    executeSoon(function() {
      checkDiskCacheFor(fileName);
      finish();
    });
  }

  function createTemporarySaveDirectory() {
    var saveDir = Cc["@mozilla.org/file/directory_service;1"]
                    .getService(Ci.nsIProperties)
                    .get("TmpD", Ci.nsIFile);
    saveDir.append("testsavedir");
    if (!saveDir.exists())
      saveDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0755);
    return saveDir;
  }

  function doTest(aIsPrivateMode, aWindow, aCallback) {
    aWindow.gBrowser.addEventListener("pageshow", function pageShown(event) {
      // If data: -url PAC file isn't loaded soon enough, we may get about:privatebrowsing loaded
      if (event.target.location == "about:blank" ||
          event.target.location == "about:privatebrowsing") {
        aWindow.gBrowser.selectedBrowser.loadURI(testURI);
        return;
      }
      aWindow.gBrowser.removeEventListener("pageshow", pageShown);

      executeSoon(function () {
        aWindow.document.addEventListener("popupshown",
                                          function(e) contextMenuOpened(aWindow, e), false);
        var img = aWindow.gBrowser.selectedBrowser.contentDocument.getElementById("img");
        EventUtils.synthesizeMouseAtCenter(img,
                                           { type: "contextmenu", button: 2 },
                                           aWindow.gBrowser.contentWindow);
      });
    });
  }

  function testOnWindow(aOptions, aCallback) {
    whenNewWindowLoaded(aOptions, function(aWin) {
      windowsToClose.push(aWin);
      // execute should only be called when need, like when you are opening
      // web pages on the test. If calling executeSoon() is not necesary, then
      // call whenNewWindowLoaded() instead of testOnWindow() on your test.
      executeSoon(function() aCallback(aWin));
    });
  };

   // this function is called after calling finish() on the test.
  registerCleanupFunction(function() {
    windowsToClose.forEach(function(aWin) {
      aWin.close();
    });
  });

  MockFilePicker.init(window);
  // then test when on private mode
  testOnWindow({private: true}, function(aWin) {
    doTest(true, aWin, finish);
  });
}

Cc["@mozilla.org/moz/jssubscript-loader;1"]
  .getService(Ci.mozIJSSubScriptLoader)
  .loadSubScript("chrome://mochitests/content/browser/toolkit/content/tests/browser/common/mockTransfer.js",
                 this);
