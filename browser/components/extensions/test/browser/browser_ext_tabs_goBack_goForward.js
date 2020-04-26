/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_tabs_goBack_goForward() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },

    files: {
      "tab1.html": `<head>
          <meta charset="utf-8">
          <title>tab1</title>
        </head>`,
      "tab2.html": `<head>
          <meta charset="utf-8">
          <title>tab2</title>
        </head>`,
    },

    async background() {
      let tabUpdatedCount = 0;
      let tab = {};

      browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tabInfo) => {
        if (changeInfo.status !== "complete" || tabId !== tab.id) {
          return;
        }

        tabUpdatedCount++;
        switch (tabUpdatedCount) {
          case 1:
            browser.test.assertEq(
              "tab1",
              tabInfo.title,
              "tab1 is found as expected"
            );
            browser.tabs.update(tabId, { url: "tab2.html" });
            break;

          case 2:
            browser.test.assertEq(
              "tab2",
              tabInfo.title,
              "tab2 is found as expected"
            );
            browser.tabs.update(tabId, { url: "tab1.html" });
            break;

          case 3:
            browser.test.assertEq(
              "tab1",
              tabInfo.title,
              "tab1 is found as expected"
            );
            browser.tabs.goBack();
            break;

          case 4:
            browser.test.assertEq(
              "tab2",
              tabInfo.title,
              "tab2 is found after navigating backward with empty parameter"
            );
            browser.tabs.goBack(tabId);
            break;

          case 5:
            browser.test.assertEq(
              "tab1",
              tabInfo.title,
              "tab1 is found after navigating backward with tabId as parameter"
            );
            browser.tabs.goForward();
            break;

          case 6:
            browser.test.assertEq(
              "tab2",
              tabInfo.title,
              "tab2 is found after navigating forward with empty parameter"
            );
            browser.tabs.goForward(tabId);
            break;

          case 7:
            browser.test.assertEq(
              "tab1",
              tabInfo.title,
              "tab1 is found after navigating forward with tabId as parameter"
            );
            await browser.tabs.remove(tabId);
            browser.test.notifyPass("tabs.goBack.goForward");
            break;

          default:
            break;
        }
      });

      tab = await browser.tabs.create({ url: "tab1.html", active: true });
    },
  });

  await extension.startup();
  await extension.awaitFinish("tabs.goBack.goForward");
  await extension.unload();
});
