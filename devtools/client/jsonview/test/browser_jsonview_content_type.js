/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const mimeSvc = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
const handlerSvc = Cc["@mozilla.org/uriloader/handler-service;1"]
                     .getService(Ci.nsIHandlerService);

let contentTypes = {
  valid: [
    "application/json",
    "application/manifest+json",
    "application/vnd.api+json",
    "application/hal+json",
    "application/json+json",
    "application/whatever+json",
  ],
  invalid: [
    "text/json",
    "text/hal+json",
    "application/jsona",
    "application/whatever+jsona",
  ],
};

add_task(async function() {
  info("Test JSON content types started");

  // Prevent saving files to disk.
  let useDownloadDir = SpecialPowers.getBoolPref("browser.download.useDownloadDir");
  SpecialPowers.setBoolPref("browser.download.useDownloadDir", false);
  let { MockFilePicker } = SpecialPowers;
  MockFilePicker.init(window);
  MockFilePicker.returnValue = MockFilePicker.returnCancel;

  for (let kind of Object.keys(contentTypes)) {
    let isValid = kind === "valid";
    for (let type of contentTypes[kind]) {
      // Prevent "Open or Save" dialogs, which would make the test fail.
      let mimeInfo = mimeSvc.getFromTypeAndExtension(type, null);
      let exists = handlerSvc.exists(mimeInfo);
      let {alwaysAskBeforeHandling} = mimeInfo;
      mimeInfo.alwaysAskBeforeHandling = false;
      handlerSvc.store(mimeInfo);

      await testType(isValid, type);
      await testType(isValid, type, ";foo=bar+json");

      // Restore old nsIMIMEInfo
      if (exists) {
        Object.assign(mimeInfo, {alwaysAskBeforeHandling});
        handlerSvc.store(mimeInfo);
      } else {
        handlerSvc.remove(mimeInfo);
      }
    }
  }

  // Restore old pref
  registerCleanupFunction(function() {
    MockFilePicker.cleanup();
    SpecialPowers.setBoolPref("browser.download.useDownloadDir", useDownloadDir);
  });
});

function testType(isValid, type, params = "") {
  const TEST_JSON_URL = "data:" + type + params + ",[1,2,3]";
  return addJsonViewTab(TEST_JSON_URL).then(async function() {
    ok(isValid, "The JSON Viewer should only load for valid content types.");
    await ContentTask.spawn(gBrowser.selectedBrowser, type, function(contentType) {
      is(content.document.contentType, contentType, "Got the right content type");
    });

    let count = await getElementCount(".jsonPanelBox .treeTable .treeRow");
    is(count, 3, "There must be expected number of rows");
  }, function() {
    ok(!isValid, "The JSON Viewer should only not load for invalid content types.");
  });
}
