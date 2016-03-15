/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* () {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background: function() {
      browser.tabs.getAllInWindow(browser.windows.WINDOW_ID_CURRENT, tabs => {
        browser.test.assertEq(1, tabs.length, "There should be only 1 tab in the window");
        browser.test.notifyPass("tabs.getAllInWindow");
      });
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("tabs.getAllInWindow");
  yield extension.unload();
});
