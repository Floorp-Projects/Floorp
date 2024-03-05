/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

async function testReturnStatus(expectedStatus) {
  // Test that tabs.saveAsPDF() returns the correct status
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.net/"
  );

  let saveDir = FileUtils.getDir("TmpD", [`testSaveDir-${Math.random()}`]);
  saveDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

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
  MockFilePicker.init(window.browsingContext);

  if (expectedStatus == "replaced" || expectedStatus == "not_replaced") {
    MockFilePicker.returnValue = MockFilePicker.returnReplace;
  } else if (expectedStatus == "canceled") {
    MockFilePicker.returnValue = MockFilePicker.returnCancel;
  } else {
    MockFilePicker.returnValue = MockFilePicker.returnOK;
  }

  MockFilePicker.displayDirectory = saveDir;

  MockFilePicker.showCallback = () => {
    MockFilePicker.setFiles([saveFile]);
    MockFilePicker.filterIndex = 0; // *.* - all file extensions
  };

  let manifest = {
    description: expectedStatus,
  };

  let extension = ExtensionTestUtils.loadExtension({
    manifest: manifest,

    background: async function () {
      let pageSettings = {};

      let expected = chrome.runtime.getManifest().description;

      let status = await browser.tabs.saveAsPDF(pageSettings);

      browser.test.assertEq(expected, status, "Got expected status");

      browser.test.notifyPass("tabs.saveAsPDF");
    },
  });

  await extension.startup();
  await extension.awaitFinish("tabs.saveAsPDF");
  await extension.unload();

  if (expectedStatus == "saved" || expectedStatus == "replaced") {
    // Check that first four bytes of saved PDF file are "%PDF"
    let text = await IOUtils.read(saveFile.path, { maxBytes: 4 });
    text = new TextDecoder().decode(text);
    is(text, "%PDF", "Got correct magic number - %PDF");
  }

  MockFilePicker.cleanup();

  if (expectedStatus == "not_saved" || expectedStatus == "not_replaced") {
    saveFile.permissions = 0o666;
  }

  saveDir.remove(true);

  BrowserTestUtils.removeTab(tab);
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

async function testFileName(expectedFileName) {
  // Test that tabs.saveAsPDF() saves with the correct filename
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.net/"
  );

  let saveDir = FileUtils.getDir("TmpD", [`testSaveDir-${Math.random()}`]);
  saveDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  let saveFile = saveDir.clone();
  saveFile.append(expectedFileName);
  if (saveFile.exists()) {
    saveFile.remove(false);
  }

  let MockFilePicker = SpecialPowers.MockFilePicker;
  MockFilePicker.init(window.browsingContext);

  MockFilePicker.returnValue = MockFilePicker.returnOK;

  MockFilePicker.displayDirectory = saveDir;

  MockFilePicker.showCallback = fp => {
    is(
      fp.defaultString,
      expectedFileName,
      "Got expected FilePicker defaultString"
    );

    is(fp.defaultExtension, "pdf", "Got expected FilePicker defaultExtension");

    let file = saveDir.clone();
    file.append(fp.defaultString);
    MockFilePicker.setFiles([file]);
  };

  let manifest = {
    description: expectedFileName,
  };

  let extension = ExtensionTestUtils.loadExtension({
    manifest: manifest,

    background: async function () {
      let pageSettings = {};

      let expected = chrome.runtime.getManifest().description;

      if (expected == "definedFileName") {
        pageSettings.toFileName = expected;
      }

      let status = await browser.tabs.saveAsPDF(pageSettings);

      browser.test.assertEq("saved", status, "Got expected status");

      browser.test.notifyPass("tabs.saveAsPDF");
    },
  });

  await extension.startup();
  await extension.awaitFinish("tabs.saveAsPDF");
  await extension.unload();

  // Check that first four bytes of saved PDF file are "%PDF"
  let text = await IOUtils.read(saveFile.path, { maxBytes: 4 });
  text = new TextDecoder().decode(text);
  is(text, "%PDF", "Got correct magic number - %PDF");

  MockFilePicker.cleanup();

  saveDir.remove(true);

  BrowserTestUtils.removeTab(tab);
}

add_task(async function testSaveAsPDF_defined_filename() {
  await testFileName("definedFileName");
});

add_task(async function testSaveAsPDF_undefined_filename() {
  // If pageSettings.toFileName is undefined, the expected filename will be
  // the test page title "mochitest index /" with the "/" replaced by "_".
  await testFileName("mochitest index _");
});
