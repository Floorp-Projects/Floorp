/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);

/**
 * TestCase for bug 564387
 * <https://bugzilla.mozilla.org/show_bug.cgi?id=564387>
 */
add_task(async function() {
  var fileName;

  let loadPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  gBrowser.loadURI("http://mochi.test:8888/browser/browser/base/content/test/general/web_video.html");
  await loadPromise;

  let popupShownPromise = BrowserTestUtils.waitForEvent(document, "popupshown");

  await BrowserTestUtils.synthesizeMouseAtCenter("#video1",
                                                 { type: "contextmenu", button: 2 },
                                                 gBrowser.selectedBrowser);
  info("context menu click on video1");

  await popupShownPromise;

  info("context menu opened on video1");

  // Create the folder the video will be saved into.
  var destDir = createTemporarySaveDirectory();
  var destFile = destDir.clone();

  MockFilePicker.displayDirectory = destDir;
  MockFilePicker.showCallback = function(fp) {
    fileName = fp.defaultString;
    destFile.append(fileName);
    MockFilePicker.setFiles([destFile]);
    MockFilePicker.filterIndex = 1; // kSaveAsType_URL
  };

  let transferCompletePromise = new Promise((resolve) => {
    function onTransferComplete(downloadSuccess) {
      ok(downloadSuccess, "Video file should have been downloaded successfully");

      is(fileName, "web-video1-expectedName.ogv",
         "Video file name is correctly retrieved from Content-Disposition http header");
      resolve();
    }

    mockTransferCallback = onTransferComplete;
    mockTransferRegisterer.register();
  });

  registerCleanupFunction(function() {
    mockTransferRegisterer.unregister();
    MockFilePicker.cleanup();
    destDir.remove(true);
  });

  // Select "Save Video As" option from context menu
  var saveVideoCommand = document.getElementById("context-savevideo");
  saveVideoCommand.doCommand();
  info("context-savevideo command executed");

  let contextMenu = document.getElementById("contentAreaContextMenu");
  let popupHiddenPromise = BrowserTestUtils.waitForEvent(contextMenu, "popuphidden");
  contextMenu.hidePopup();
  await popupHiddenPromise;

  await transferCompletePromise;
});

/* import-globals-from ../../../../../toolkit/content/tests/browser/common/mockTransfer.js */
Services.scriptloader.loadSubScript("chrome://mochitests/content/browser/toolkit/content/tests/browser/common/mockTransfer.js",
                 this);

function createTemporarySaveDirectory() {
  var saveDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  saveDir.append("testsavedir");
  if (!saveDir.exists())
    saveDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  return saveDir;
}
