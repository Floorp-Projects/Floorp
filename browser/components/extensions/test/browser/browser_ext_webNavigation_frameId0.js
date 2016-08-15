/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* webNavigation_getFrameId_of_existing_main_frame() {
  // Whether the frame ID in the extension API is 0 is determined by a map that
  // is maintained by |Frames| in ExtensionManagement.jsm. This map is filled
  // using data from content processes. But if ExtensionManagement.jsm is not
  // imported, then the "Extension:TopWindowID" message gets lost.
  // As a result, if the state is not synchronized again, the webNavigation API
  // will mistakenly report a non-zero frame ID for top-level frames.
  //
  // If you want to be absolutely sure that the frame ID is correct, don't open
  // tabs before starting an extension, or explicitly load the module in the
  // main process:
  // Cu.import("resource://gre/modules/ExtensionManagement.jsm", {});
  //
  // Or simply run the test again.
  const BASE = "http://mochi.test:8888/browser/browser/components/extensions/test/browser/";
  const DUMMY_URL = BASE + "file_dummy.html";
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, DUMMY_URL, true);

  function background(DUMMY_URL) {
    browser.tabs.query({active: true, currentWindow: true}).then(tabs => {
      return browser.webNavigation.getAllFrames({tabId: tabs[0].id});
    }).then(frames => {
      browser.test.assertEq(1, frames.length, "The dummy page has one frame");
      browser.test.assertEq(0, frames[0].frameId, "Main frame's ID must be 0");
      browser.test.assertEq(DUMMY_URL, frames[0].url, "Main frame URL must match");
      browser.test.notifyPass("frameId checked");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["webNavigation"],
    },

    background: `(${background})(${JSON.stringify(DUMMY_URL)});`,
  });

  yield extension.startup();
  yield extension.awaitFinish("frameId checked");
  yield extension.unload();

  yield BrowserTestUtils.removeTab(tab);
});
