/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

async function testReturnStatus(expectedStatus) {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.net/");

  let saveDir = FileUtils.getDir("TmpD", [`testSaveDir-${Math.random()}`], true);

  let saveFile = saveDir.clone();
  saveFile.append("testSaveFile.pdf");
  if (saveFile.exists()) {
    saveFile.remove(false);
  }

  if (expectedStatus == "replaced") {
    // Create file that can be replaced
    saveFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);
  } else if (expectedStatus == "not_saved") {
    // Create directory with same name as file - so that file cannot be saved
    saveFile.create(Ci.nsIFile.DIRECTORY_TYPE, 0o666);
  } else if (expectedStatus == "not_replaced") {
    // Create file that cannot be replaced
    saveFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o444);
  }

  let MockFilePicker = SpecialPowers.MockFilePicker;
  MockFilePicker.init(window);

  if (expectedStatus == "replaced" || expectedStatus == "not_replaced") {
    MockFilePicker.returnValue = MockFilePicker.returnReplace;
  } else if (expectedStatus == "canceled") {
    MockFilePicker.returnValue = MockFilePicker.returnCancel;
  } else {
    MockFilePicker.returnValue = MockFilePicker.returnOK;
  }

  MockFilePicker.displayDirectory = saveDir;
  MockFilePicker.showCallback = function(fp) {
    MockFilePicker.setFiles([saveFile]);
    MockFilePicker.filterIndex = 0; // *.* - all file extensions
  };

  let manifest = {
    "description": expectedStatus,
  };

  let extension = ExtensionTestUtils.loadExtension({
    manifest: manifest,

    background: async function() {
      let pageSettings = {};

      let status = await browser.tabs.saveAsPDF(pageSettings);

      let expected = chrome.runtime.getManifest().description;

      browser.test.assertEq(expected, status, "saveAsPDF " + expected);

      browser.test.notifyPass("tabs.saveAsPDF");
    },
  });

  await extension.startup();
  await extension.awaitFinish("tabs.saveAsPDF");
  await extension.unload();

  if (expectedStatus == "saved" || expectedStatus == "replaced") {
    // Check that first four bytes of saved PDF file are "%PDF"
    let text = await OS.File.read(saveFile.path, {encoding: "utf-8", bytes: 4});
    is(text, "%PDF", "Got correct magic number");
  }

  MockFilePicker.cleanup();

  if (expectedStatus == "not_saved" || expectedStatus == "not_replaced") {
    saveFile.permissions = 0o666;
  }

  saveDir.remove(true);

  await BrowserTestUtils.removeTab(tab);
}

add_task(async function testSaveAsPDF_saved() {
  await testReturnStatus("saved");
});

add_task(async function testSaveAsPDF_replaced() {
  await testReturnStatus("replaced");
});

add_task(async function testSaveAsPDF_canceled() {
  await testReturnStatus("canceled");
});

add_task(async function testSaveAsPDF_not_saved() {
  await testReturnStatus("not_saved");
});

add_task(async function testSaveAsPDF_not_replaced() {
  await testReturnStatus("not_replaced");
});
