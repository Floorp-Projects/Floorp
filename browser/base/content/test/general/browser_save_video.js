/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);

/**
 * TestCase for bug 564387
 * <https://bugzilla.mozilla.org/show_bug.cgi?id=564387>
 */
function test() {
  waitForExplicitFinish();
  var fileName;

  gBrowser.loadURI("http://mochi.test:8888/browser/browser/base/content/test/general/bug564387.html");

  registerCleanupFunction(function () {
    gBrowser.addTab();
    gBrowser.removeCurrentTab();
  });

  gBrowser.addEventListener("pageshow", function pageShown(event) {
    if (event.target.location == "about:blank")
      return;
    gBrowser.removeEventListener("pageshow", pageShown);

    executeSoon(function () {
      document.addEventListener("popupshown", contextMenuOpened);

      var video1 = gBrowser.contentDocument.getElementById("video1");
      EventUtils.synthesizeMouseAtCenter(video1,
                                         { type: "contextmenu", button: 2 },
                                         gBrowser.contentWindow);
    });
  });

  function contextMenuOpened(event) {
    event.currentTarget.removeEventListener("popupshown", contextMenuOpened);

    // Create the folder the video will be saved into.
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

    // Select "Save Video As" option from context menu
    var saveVideoCommand = document.getElementById("context-savevideo");
    saveVideoCommand.doCommand();

    event.target.hidePopup();
  }

  function onTransferComplete(downloadSuccess) {
    ok(downloadSuccess, "Video file should have been downloaded successfully");

    is(fileName, "Bug564387-expectedName.ogv",
       "Video file name is correctly retrieved from Content-Disposition http header");

    finish();
  }
}

Cc["@mozilla.org/moz/jssubscript-loader;1"]
  .getService(Ci.mozIJSSubScriptLoader)
  .loadSubScript("chrome://mochitests/content/browser/toolkit/content/tests/browser/common/mockTransfer.js",
                 this);

function createTemporarySaveDirectory() {
  var saveDir = Cc["@mozilla.org/file/directory_service;1"]
                  .getService(Ci.nsIProperties)
                  .get("TmpD", Ci.nsIFile);
  saveDir.append("testsavedir");
  if (!saveDir.exists())
    saveDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0755);
  return saveDir;
}
