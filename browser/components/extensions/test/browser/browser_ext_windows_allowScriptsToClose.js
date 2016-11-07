/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// Tests allowScriptsToClose option
add_task(function* test_allowScriptsToClose() {
  const files = {
    "dummy.html": "<meta charset=utf-8><script src=close.js></script>",
    "close.js": function() {
      window.close();
      if (!window.closed) {
        browser.test.sendMessage("close-failed");
      }
    },
  };

  function background() {
    browser.test.onMessage.addListener((msg, options) => {
      function listener(_, {status}, {url}) {
        if (status == "complete" && url == options.url) {
          browser.tabs.onUpdated.removeListener(listener);
          browser.tabs.executeScript({file: "close.js"});
        }
      }
      options.url = browser.runtime.getURL(options.url);
      browser.windows.create(options);
      if (msg === "create+execute") {
        browser.tabs.onUpdated.addListener(listener);
      }
    });
    browser.test.notifyPass();
  }

  const example = "http://example.com/";
  const manifest = {permissions: ["tabs", example]};

  const extension = ExtensionTestUtils.loadExtension({files, background, manifest});
  yield SpecialPowers.pushPrefEnv({set: [["dom.allow_scripts_to_close_windows", false]]});

  yield extension.startup();
  yield extension.awaitFinish();

  extension.sendMessage("create", {url: "dummy.html"});
  let win = yield BrowserTestUtils.waitForNewWindow();
  yield BrowserTestUtils.windowClosed(win);
  info("script allowed to close the window");

  extension.sendMessage("create+execute", {url: example});
  win = yield BrowserTestUtils.waitForNewWindow();
  yield extension.awaitMessage("close-failed");
  info("script prevented from closing the window");
  win.close();

  extension.sendMessage("create+execute", {url: example, allowScriptsToClose: true});
  win = yield BrowserTestUtils.waitForNewWindow();
  yield BrowserTestUtils.windowClosed(win);
  info("script allowed to close the window");

  yield SpecialPowers.popPrefEnv();
  yield extension.unload();
});
