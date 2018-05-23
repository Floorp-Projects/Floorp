/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* eslint-disable no-unused-vars, no-undef */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const saveButton = "button.save";
const prettifyButton = "button.prettyprint";

let { MockFilePicker } = SpecialPowers;
MockFilePicker.init(window);
MockFilePicker.returnValue = MockFilePicker.returnOK;

Services.scriptloader
        .loadSubScript("chrome://mochitests/content/browser/toolkit/content/tests/browser/common/mockTransfer.js",
                       this);

function click(selector) {
  return BrowserTestUtils.synthesizeMouseAtCenter(selector, {}, gBrowser.selectedBrowser);
}

function rightClick(selector) {
  return BrowserTestUtils.synthesizeMouseAtCenter(
    selector,
    {type: "contextmenu", button: 2},
    gBrowser.selectedBrowser
  );
}

function awaitFileSave(name, ext) {
  return new Promise((resolve) => {
    MockFilePicker.showCallback = (fp) => {
      ok(true, "File picker was opened");
      let fileName = fp.defaultString;
      is(fileName, name, "File picker should provide the correct default filename.");
      is(fp.defaultExtension, ext,
         "File picker should provide the correct default file extension.");
      let destFile = destDir.clone();
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

function getFileContents(file) {
  return new Promise((resolve, reject) => {
    let channel = NetUtil.newChannel({
      uri: NetUtil.newURI(file),
      loadUsingSystemPrincipal: true,
    });
    NetUtil.asyncFetch(channel, function(inputStream, status) {
      if (Components.isSuccessCode(status)) {
        info("Fetched downloaded contents.");
        resolve(NetUtil.readInputStreamToString(inputStream, inputStream.available()));
      } else {
        reject();
      }
    });
  });
}

function createTemporarySaveDirectory() {
  let saveDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  saveDir.append("jsonview-testsavedir");
  if (!saveDir.exists()) {
    info("Creating temporary save directory.");
    saveDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  }
  info("Temporary save directory: " + saveDir.path);
  return saveDir;
}

let destDir = createTemporarySaveDirectory();
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

  let promise, rawJSON, prettyJSON;
  await fetch(new Request(TEST_JSON_URL))
    .then(response => response.text())
    .then(function(data) {
      info("Fetched JSON contents.");
      rawJSON = data;
      prettyJSON = JSON.stringify(JSON.parse(data), null, "  ");
    });

  // Attempt to save original JSON via "Save As" command
  promise = awaitFileSave(JSON_FILE, "json");
  await new Promise((resolve) => {
    info("Register to handle popupshown.");
    document.addEventListener("popupshown", function(event) {
      info("Context menu opened.");
      let savePageCommand = document.getElementById("context-savepage");
      savePageCommand.doCommand();
      info("SavePage command done.");
      event.target.hidePopup();
      info("Context menu hidden.");
      resolve();
    }, {once: true});
    rightClick("body");
    info("Right clicked.");
  });
  await promise.then(getFileContents).then(function(data) {
    is(data, rawJSON, "Original JSON contents should have been saved.");
  });

  // Attempt to save original JSON via "Save" button
  promise = awaitFileSave(JSON_FILE, "json");
  await click(saveButton);
  info("Clicked Save button.");
  await promise.then(getFileContents).then(function(data) {
    is(data, rawJSON, "Original JSON contents should have been saved.");
  });

  // Attempt to save prettified JSON via "Save" button
  await selectJsonViewContentTab("rawdata");
  info("Switched to Raw Data tab.");
  await click(prettifyButton);
  info("Clicked Pretty Print button.");
  promise = awaitFileSave(JSON_FILE, "json");
  await click(saveButton);
  info("Clicked Save button.");
  await promise.then(getFileContents).then(function(data) {
    is(data, prettyJSON, "Prettified JSON contents should have been saved.");
  });
});

add_task(async function() {
  info("Test 2 save JSON started");

  const TEST_JSON_URL = "data:application/json,2";
  await addJsonViewTab(TEST_JSON_URL);

  info("Checking that application/json adds .json extension by default.");
  let promise = awaitFileSave("index.json", "json");
  await click(saveButton);
  info("Clicked Save button.");
  await promise.then(getFileContents);
});

add_task(async function() {
  info("Test 3 save JSON started");

  const TEST_JSON_URL = "data:application/manifest+json,3";
  await addJsonViewTab(TEST_JSON_URL);

  info("Checking that application/manifest+json does not add .json extension.");
  let promise = awaitFileSave("index", null);
  await click(saveButton);
  info("Clicked Save button.");
  await promise.then(getFileContents);
});
