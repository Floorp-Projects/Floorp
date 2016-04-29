/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_JSON_URL = URL_ROOT + "valid_json.json";

let { MockFilePicker } = SpecialPowers;

MockFilePicker.init(window);
MockFilePicker.returnValue = MockFilePicker.returnCancel;

registerCleanupFunction(function () {
  MockFilePicker.cleanup();
});

add_task(function* () {
  info("Test save JSON started");

  yield addJsonViewTab(TEST_JSON_URL);

  let promise = new Promise((resolve) => {
    MockFilePicker.showCallback = () => {
      MockFilePicker.showCallback = null;
      ok(true, "File picker was opened");
      resolve();
    };
  });

  let browser = gBrowser.selectedBrowser;
  yield BrowserTestUtils.synthesizeMouseAtCenter(
    ".jsonPanelBox button.save",
    {}, browser);

  yield promise;
});
