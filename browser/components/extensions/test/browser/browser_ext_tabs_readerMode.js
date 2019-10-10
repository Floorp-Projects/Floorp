/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_reader_mode() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },

    async background() {
      let tab;
      let tabId;
      let expected = { isInReaderMode: false };
      let testState = {};
      browser.test.onMessage.addListener(async (msg, ...args) => {
        switch (msg) {
          case "updateUrl":
            expected.isArticle = args[0];
            expected.url = args[1];
            tab = await browser.tabs.update({ url: expected.url });
            tabId = tab.id;
            break;
          case "enterReaderMode":
            expected.isArticle = !args[0];
            expected.isInReaderMode = true;
            tab = await browser.tabs.get(tabId);
            browser.test.assertEq(
              false,
              tab.isInReaderMode,
              "The tab is not in reader mode."
            );
            if (args[0]) {
              browser.tabs.toggleReaderMode(tabId);
            } else {
              await browser.test.assertRejects(
                browser.tabs.toggleReaderMode(tabId),
                /The specified tab cannot be placed into reader mode/,
                "Toggle fails with an unreaderable document."
              );
              browser.test.assertEq(
                false,
                tab.isInReaderMode,
                "The tab is still not in reader mode."
              );
              browser.test.sendMessage("enterFailed");
            }
            break;
          case "leaveReaderMode":
            expected.isInReaderMode = false;
            tab = await browser.tabs.get(tabId);
            browser.test.assertTrue(
              tab.isInReaderMode,
              "The tab is in reader mode."
            );
            browser.tabs.toggleReaderMode(tabId);
            break;
        }
      });

      browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {
        if (changeInfo.status === "complete") {
          testState.url = tab.url;
          let urlOk = expected.isInReaderMode
            ? testState.url.startsWith("about:reader")
            : expected.url == testState.url;
          if (urlOk && expected.isArticle == testState.isArticle) {
            browser.test.sendMessage("tabUpdated", tab);
          }
          return;
        }
        if (
          changeInfo.isArticle == expected.isArticle &&
          changeInfo.isArticle != testState.isArticle
        ) {
          testState.isArticle = changeInfo.isArticle;
          let urlOk = expected.isInReaderMode
            ? testState.url.startsWith("about:reader")
            : expected.url == testState.url;
          if (urlOk && expected.isArticle == testState.isArticle) {
            browser.test.sendMessage("isArticle", tab);
          }
        }
      });
    },
  });

  const TEST_PATH = getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "http://example.com"
  );
  const READER_MODE_PREFIX = "about:reader";

  await extension.startup();
  extension.sendMessage(
    "updateUrl",
    true,
    `${TEST_PATH}readerModeArticle.html`
  );
  let tab = await extension.awaitMessage("isArticle");

  ok(
    !tab.url.startsWith(READER_MODE_PREFIX),
    "Tab url does not indicate reader mode."
  );
  ok(tab.isArticle, "Tab is readerable.");

  extension.sendMessage("enterReaderMode", true);
  tab = await extension.awaitMessage("tabUpdated");
  ok(tab.url.startsWith(READER_MODE_PREFIX), "Tab url indicates reader mode.");
  ok(tab.isInReaderMode, "tab.isInReaderMode indicates reader mode.");

  extension.sendMessage("leaveReaderMode");
  tab = await extension.awaitMessage("tabUpdated");
  ok(
    !tab.url.startsWith(READER_MODE_PREFIX),
    "Tab url does not indicate reader mode."
  );
  ok(!tab.isInReaderMode, "tab.isInReaderMode does not indicate reader mode.");

  extension.sendMessage(
    "updateUrl",
    false,
    `${TEST_PATH}readerModeNonArticle.html`
  );
  tab = await extension.awaitMessage("tabUpdated");
  ok(
    !tab.url.startsWith(READER_MODE_PREFIX),
    "Tab url does not indicate reader mode."
  );
  ok(!tab.isArticle, "Tab is not readerable.");
  ok(!tab.isInReaderMode, "tab.isInReaderMode does not indicate reader mode.");

  extension.sendMessage("enterReaderMode", false);
  await extension.awaitMessage("enterFailed");

  await extension.unload();
});
