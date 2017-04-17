/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* testExecuteScript() {
  const BASE = "http://mochi.test:8888/browser/browser/components/extensions/test/browser/";
  const URL = BASE + "file_dummy.html";
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, URL, true);

  async function background() {
    try {
      await browser.tabs.executeScript({code: "this.foo = 'bar'"});
      await browser.tabs.executeScript({file: "script.js"});

      let [result1] = await browser.tabs.executeScript({code: "[this.foo, this.bar]"});
      let [result2] = await browser.tabs.executeScript({file: "script2.js"});

      browser.test.assertEq("bar,baz", String(result1), "executeScript({code}) result");
      browser.test.assertEq("bar,baz", String(result2), "executeScript({file}) result");

      browser.test.notifyPass("executeScript-multiple");
    } catch (e) {
      browser.test.fail(`Error: ${e} :: ${e.stack}`);
      browser.test.notifyFail("executeScript-multiple");
    }
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["http://mochi.test/"],
    },

    background,

    files: {
      "script.js": function() {
        this.bar = "baz";
      },

      "script2.js": "[this.foo, this.bar]",
    },
  });

  yield extension.startup();

  yield extension.awaitFinish("executeScript-multiple");

  yield extension.unload();
  yield BrowserTestUtils.removeTab(tab);
});
