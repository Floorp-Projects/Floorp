/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// Test that calling window.open from a content script running in a private
// window does not trigger a crash (regression test introduced in Bug 1653530
// to cover the issue introduced in Bug 1616353 and fixed by Bug 1638793).
add_task(async function test_contentscript_window_open_doesnot_crash() {
  const extension = ExtensionTestUtils.loadExtension({
    incognitoOverride: "spanning",
    manifest: {
      content_scripts: [
        {
          matches: ["https://example.com/*"],
          js: ["test_window_open.js"],
        },
      ],
    },
    files: {
      "test_window_open.js": function () {
        const newWin = window.open();
        browser.test.log("calling window.open did not triggered a crash");
        browser.test.sendMessage("window-open-called", !!newWin);
      },
    },
  });
  await extension.startup();

  const winPrivate = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  await BrowserTestUtils.openNewForegroundTab(
    winPrivate.gBrowser,
    "https://example.com"
  );
  const newWinOpened = await extension.awaitMessage("window-open-called");
  ok(newWinOpened, "Content script successfully open a new window");

  await extension.unload();
  await BrowserTestUtils.closeWindow(winPrivate);
});
