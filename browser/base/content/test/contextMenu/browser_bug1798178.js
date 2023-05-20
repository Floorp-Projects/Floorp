/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/content/tests/browser/common/mockTransfer.js",
  this
);

const TEST_URL =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "file_bug1798178.html";

let MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);

function createTemporarySaveDirectory() {
  let saveDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  saveDir.append("testsavedir");
  if (!saveDir.exists()) {
    info("create testsavedir!");
    saveDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  }
  info("return from createTempSaveDir: " + saveDir.path);
  return saveDir;
}

add_task(async function test_save_link_cross_origin() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.opaqueResponseBlocking", true]],
  });
  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    let menu = document.getElementById("contentAreaContextMenu");
    let popupShown = BrowserTestUtils.waitForEvent(menu, "popupshown");
    BrowserTestUtils.synthesizeMouseAtCenter(
      "a[href]",
      { type: "contextmenu", button: 2 },
      browser
    );
    await popupShown;

    let filePickerShow = new Promise(r => {
      MockFilePicker.showCallback = function (fp) {
        ok(true, "filepicker should be shown");
        info("MockFilePicker showCallback");

        let fileName = fp.defaultString;
        destFile = tempDir.clone();
        destFile.append(fileName);

        MockFilePicker.setFiles([destFile]);
        MockFilePicker.filterIndex = 1; // kSaveAsType_URL

        info("MockFilePicker showCallback done");
        r();
      };
    });

    info("Let's create a temporary dir");
    let tempDir = createTemporarySaveDirectory();
    let destFile;

    MockFilePicker.displayDirectory = tempDir;

    let transferCompletePromise = new Promise(resolve => {
      function onTransferComplete(downloadSuccess) {
        ok(downloadSuccess, "File should have been downloaded successfully");
        resolve();
      }
      mockTransferCallback = onTransferComplete;
      mockTransferRegisterer.register();
    });

    let saveLinkCommand = document.getElementById("context-savelink");
    info("saveLinkCommand: " + saveLinkCommand);
    saveLinkCommand.doCommand();

    await filePickerShow;

    let popupHiddenPromise = BrowserTestUtils.waitForEvent(menu, "popuphidden");
    menu.hidePopup();
    await popupHiddenPromise;

    await transferCompletePromise;
  });
});
