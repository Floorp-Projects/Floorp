/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "PlacesTestUtils",
                                  "resource://testing-common/PlacesTestUtils.jsm");

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

add_task(function* test_delete_url() {
  const TEST_URL = `http://example.com/${Math.random()}`;

  function background() {
    browser.test.onMessage.addListener((msg, url) => {
      browser.history.deleteUrl({url: url}).then(result => {
        browser.test.assertEq(undefined, result, "browser.history.deleteUrl returns nothing");
        browser.test.sendMessage("url-deleted");
      });
    });

    browser.test.sendMessage("ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["history"],
    },
    background: `(${background})()`,
  });

  yield extension.startup();
  yield PlacesTestUtils.clearHistory();
  yield extension.awaitMessage("ready");

  yield PlacesTestUtils.addVisits(TEST_URL);
  ok(yield PlacesTestUtils.isPageInDB(TEST_URL), `${TEST_URL} found in history database`);

  extension.sendMessage("delete-url", TEST_URL);
  yield extension.awaitMessage("url-deleted");
  ok(!(yield PlacesTestUtils.isPageInDB(TEST_URL)), `${TEST_URL} not found in history database`);

  yield extension.unload();
});
