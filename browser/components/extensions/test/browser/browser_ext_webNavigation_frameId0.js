/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function webNavigation_getFrameId_of_existing_main_frame() {
  const BASE = "http://mochi.test:8888/browser/browser/components/extensions/test/browser/";
  const DUMMY_URL = BASE + "file_dummy.html";
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, DUMMY_URL, true);

  async function background(DUMMY_URL) {
    let tabs = await browser.tabs.query({active: true, currentWindow: true});
    let frames = await browser.webNavigation.getAllFrames({tabId: tabs[0].id});
    browser.test.assertEq(1, frames.length, "The dummy page has one frame");
    browser.test.assertEq(0, frames[0].frameId, "Main frame's ID must be 0");
    browser.test.assertEq(DUMMY_URL, frames[0].url, "Main frame URL must match");
    browser.test.notifyPass("frameId checked");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["webNavigation"],
    },

    background: `(${background})(${JSON.stringify(DUMMY_URL)});`,
  });

  await extension.startup();
  await extension.awaitFinish("frameId checked");
  await extension.unload();

  await BrowserTestUtils.removeTab(tab);
});
