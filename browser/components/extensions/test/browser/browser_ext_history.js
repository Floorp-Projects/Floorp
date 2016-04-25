/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* test_history_schema() {
  function background() {
    browser.test.assertTrue(browser.history, "browser.history API exists");
    browser.test.notifyPass("history-schema");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["history"],
    },
    background: `(${background})()`,
  });
  yield extension.startup();
  yield extension.awaitFinish("history-schema");
  yield extension.unload();
});
