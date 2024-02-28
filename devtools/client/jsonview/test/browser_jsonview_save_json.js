/* eslint-disable no-unused-vars, no-undef */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const saveButton = "button.save";
const prettifyButton = "button.prettyprint";

const { MockFilePicker } = SpecialPowers;
MockFilePicker.init(window.browsingContext);
MockFilePicker.returnValue = MockFilePicker.returnOK;

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/content/tests/browser/common/mockTransfer.js",
  this
);

function awaitSavedFileContents(name, ext) {
  return new Promise((resolve, reject) => {
    MockFilePicker.showCallback = fp => {
      try {
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
        mockTransferCallback = async function (downloadSuccess) {
          try {
            ok(
              downloadSuccess,
              "JSON should have been downloaded successfully"
            );
            ok(destFile.exists(), "The downloaded file should exist.");
            const { path } = destFile;
            await BrowserTestUtils.waitForCondition(() => IOUtils.exists(path));
            await BrowserTestUtils.waitForCondition(async () => {
              const { size } = await IOUtils.stat(path);
              return size > 0;
            });
            const buffer = await IOUtils.read(path);
            resolve(new TextDecoder().decode(buffer));
          } catch (error) {
            reject(error);
          }
        };
      } catch (error) {
        reject(error);
      }
    };
  });
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
registerCleanupFunction(function () {
  mockTransferRegisterer.unregister();
  MockFilePicker.cleanup();
  destDir.remove(true);
  ok(!destDir.exists(), "Destination dir should be removed");
});

add_task(async function () {
  info("Test 1 save JSON started");

  const JSON_FILE = "simple_json.json";
  const TEST_JSON_URL = URL_ROOT + JSON_FILE;
  const tab = await addJsonViewTab(TEST_JSON_URL);

  const response = await fetch(new Request(TEST_JSON_URL));

  info("Fetched JSON contents.");
  const rawJSON = await response.text();
  const prettyJSON = JSON.stringify(JSON.parse(rawJSON), null, "  ");
  isnot(
    rawJSON,
    prettyJSON,
    "Original and prettified JSON should be different."
  );

  // Attempt to save original JSON via saveBrowser (ctrl/cmd+s or "Save Page As" command).
  let data = awaitSavedFileContents(JSON_FILE, "json");
  saveBrowser(tab.linkedBrowser);
  is(await data, rawJSON, "Original JSON contents should have been saved.");

  // Attempt to save original JSON via "Save" button
  data = awaitSavedFileContents(JSON_FILE, "json");
  await clickJsonNode(saveButton);
  info("Clicked Save button.");
  is(await data, rawJSON, "Original JSON contents should have been saved.");

  // Attempt to save prettified JSON via "Save" button
  await selectJsonViewContentTab("rawdata");
  info("Switched to Raw Data tab.");
  await clickJsonNode(prettifyButton);
  info("Clicked Pretty Print button.");
  data = awaitSavedFileContents(JSON_FILE, "json");
  await clickJsonNode(saveButton);
  info("Clicked Save button.");
  is(
    await data,
    prettyJSON,
    "Prettified JSON contents should have been saved."
  );

  // saveBrowser should still save original contents.
  data = awaitSavedFileContents(JSON_FILE, "json");
  saveBrowser(tab.linkedBrowser);
  is(await data, rawJSON, "Original JSON contents should have been saved.");
});

add_task(async function () {
  info("Test 2 save JSON started");

  const TEST_JSON_URL = "data:application/json,2";
  await addJsonViewTab(TEST_JSON_URL);

  info("Checking that application/json adds .json extension by default.");
  const data = awaitSavedFileContents("Untitled.json", "json");
  await clickJsonNode(saveButton);
  info("Clicked Save button.");
  is(await data, "2", "JSON contents should have been saved.");
});

add_task(async function () {
  info("Test 3 save JSON started");

  const TEST_JSON_URL = "data:application/manifest+json,3";
  await addJsonViewTab(TEST_JSON_URL);

  info("Checking that application/manifest+json does not add .json extension.");
  const data = awaitSavedFileContents("Untitled", null);
  await clickJsonNode(saveButton);
  info("Clicked Save button.");
  is(await data, "3", "JSON contents should have been saved.");
});
