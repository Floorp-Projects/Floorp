/* eslint-disable no-unused-vars, no-undef */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const saveButton = "button.save";
const prettifyButton = "button.prettyprint";

const { MockFilePicker } = SpecialPowers;
MockFilePicker.init(window);
MockFilePicker.returnValue = MockFilePicker.returnOK;

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/content/tests/browser/common/mockTransfer.js",
  this
);

function click(selector) {
  return BrowserTestUtils.synthesizeMouseAtCenter(
    selector,
    {},
    gBrowser.selectedBrowser
  );
}

function rightClick(selector) {
  return BrowserTestUtils.synthesizeMouseAtCenter(
    selector,
    { type: "contextmenu", button: 2 },
    gBrowser.selectedBrowser
  );
}

function awaitFileSave(name, ext) {
  return new Promise(resolve => {
    MockFilePicker.showCallback = fp => {
      ok(true, "File picker was opened");
      const fileName = fp.defaultString;
      is(
        fileName,
        name,
        "File picker should provide the correct default filename."
      );
      is(
        fp.defaultExtension,
        ext,
        "File picker should provide the correct default file extension."
      );
      const destFile = destDir.clone();
      destFile.append(fileName);
      MockFilePicker.setFiles([destFile]);
      MockFilePicker.showCallback = null;
      mockTransferCallback = function(downloadSuccess) {
        ok(downloadSuccess, "JSON should have been downloaded successfully");
        ok(destFile.exists(), "The downloaded file should exist.");
        resolve(destFile);
      };
    };
  });
}

async function getFileContents(file) {
  await BrowserTestUtils.waitForCondition(() => OS.File.exists(file.path));
  const buffer = await OS.File.read(file.path);
  return new TextDecoder().decode(buffer);
}

function createTemporarySaveDirectory() {
  const saveDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  saveDir.append("jsonview-testsavedir");
  if (!saveDir.exists()) {
    info("Creating temporary save directory.");
    saveDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  }
  info("Temporary save directory: " + saveDir.path);
  return saveDir;
}

const destDir = createTemporarySaveDirectory();
mockTransferRegisterer.register();
MockFilePicker.displayDirectory = destDir;
registerCleanupFunction(function() {
  mockTransferRegisterer.unregister();
  MockFilePicker.cleanup();
  destDir.remove(true);
  ok(!destDir.exists(), "Destination dir should be removed");
});

add_task(async function() {
  info("Test 1 save JSON started");

  const JSON_FILE = "simple_json.json";
  const TEST_JSON_URL = URL_ROOT + JSON_FILE;
  await addJsonViewTab(TEST_JSON_URL);

  const response = await fetch(new Request(TEST_JSON_URL));

  info("Fetched JSON contents.");
  const rawJSON = await response.text();
  const prettyJSON = JSON.stringify(JSON.parse(rawJSON), null, "  ");

  // Attempt to save original JSON via "Save As" command
  let onFileSaved = awaitFileSave(JSON_FILE, "json");
  await new Promise(resolve => {
    info("Register to handle popupshown.");
    document.addEventListener(
      "popupshown",
      function(event) {
        info("Context menu opened.");
        const savePageCommand = document.getElementById("context-savepage");
        savePageCommand.doCommand();
        info("SavePage command done.");
        event.target.hidePopup();
        info("Context menu hidden.");
        resolve();
      },
      { once: true }
    );
    rightClick("body");
    info("Right clicked.");
  });
  let data = await onFileSaved.then(getFileContents);
  is(data, rawJSON, "Original JSON contents should have been saved.");

  // Attempt to save original JSON via "Save" button
  onFileSaved = awaitFileSave(JSON_FILE, "json");
  await click(saveButton);
  info("Clicked Save button.");
  data = await onFileSaved.then(getFileContents);
  is(data, rawJSON, "Original JSON contents should have been saved.");

  // Attempt to save prettified JSON via "Save" button
  await selectJsonViewContentTab("rawdata");
  info("Switched to Raw Data tab.");
  await click(prettifyButton);
  info("Clicked Pretty Print button.");
  onFileSaved = awaitFileSave(JSON_FILE, "json");
  await click(saveButton);
  info("Clicked Save button.");
  data = await onFileSaved.then(getFileContents);
  is(data, prettyJSON, "Prettified JSON contents should have been saved.");
});

add_task(async function() {
  info("Test 2 save JSON started");

  const TEST_JSON_URL = "data:application/json,2";
  await addJsonViewTab(TEST_JSON_URL);

  info("Checking that application/json adds .json extension by default.");
  const onFileSaved = awaitFileSave("index.json", "json");
  await click(saveButton);
  info("Clicked Save button.");
  await onFileSaved.then(getFileContents);
});

add_task(async function() {
  info("Test 3 save JSON started");

  const TEST_JSON_URL = "data:application/manifest+json,3";
  await addJsonViewTab(TEST_JSON_URL);

  info("Checking that application/manifest+json does not add .json extension.");
  const onFileSaved = awaitFileSave("index", null);
  await click(saveButton);
  info("Clicked Save button.");
  await onFileSaved.then(getFileContents);
});
