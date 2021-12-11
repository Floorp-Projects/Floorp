/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const VIDEO_URL =
  "http://mochi.test:8888/browser/browser/base/content/test/general/web_video.html";

/**
 * mockTransfer.js provides a utility that lets us mock out
 * the "Save File" dialog.
 */
/* import-globals-from ../../../../../toolkit/content/tests/browser/common/mockTransfer.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/content/tests/browser/common/mockTransfer.js",
  this
);

/**
 * Creates and returns an nsIFile for a new temporary save
 * directory.
 *
 * @return nsIFile
 */
function createTemporarySaveDirectory() {
  let saveDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  saveDir.append("testsavedir");
  if (!saveDir.exists()) {
    saveDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  }
  return saveDir;
}
/**
 * MockTransfer exposes a "mockTransferCallback" global which
 * allows us to define a callback to be called once the mock file
 * selector has selected where to save the file.
 */
function waitForTransferComplete() {
  return new Promise(resolve => {
    mockTransferCallback = () => {
      ok(true, "Transfer completed");
      mockTransferCallback = () => {};
      resolve();
    };
  });
}

/**
 * Loads a page with a <video> element, right-clicks it and chooses
 * to save a frame screenshot to the disk. Completes once we've
 * verified that the frame has been saved to disk.
 */
add_task(async function() {
  let MockFilePicker = SpecialPowers.MockFilePicker;
  MockFilePicker.init(window);

  // Create the folder the video will be saved into.
  let destDir = createTemporarySaveDirectory();
  let destFile = destDir.clone();

  MockFilePicker.displayDirectory = destDir;
  MockFilePicker.showCallback = function(fp) {
    destFile.append(fp.defaultString);
    MockFilePicker.setFiles([destFile]);
    MockFilePicker.filterIndex = 1; // kSaveAsType_URL
  };

  mockTransferRegisterer.register();

  // Make sure that we clean these things up when we're done.
  registerCleanupFunction(function() {
    mockTransferRegisterer.unregister();
    MockFilePicker.cleanup();
    destDir.remove(true);
  });

  let tab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedTab = tab;
  let browser = tab.linkedBrowser;
  info("Loading video tab");
  await promiseTabLoadEvent(tab, VIDEO_URL);
  info("Video tab loaded.");

  let context = document.getElementById("contentAreaContextMenu");
  let popupPromise = promisePopupShown(context);

  info("Synthesizing right-click on video element");
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#video1",
    { type: "contextmenu", button: 2 },
    browser
  );
  info("Waiting for popup to fire popupshown.");
  await popupPromise;
  info("Popup fired popupshown");

  let saveSnapshotCommand = document.getElementById("context-video-saveimage");
  let promiseTransfer = waitForTransferComplete();
  info("Firing save snapshot command");
  saveSnapshotCommand.doCommand();
  context.hidePopup();
  info("Waiting for transfer completion");
  await promiseTransfer;
  info("Transfer complete");
  gBrowser.removeTab(tab);
});
