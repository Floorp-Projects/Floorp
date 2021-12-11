/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs", "<all_urls>"],
    },

    async background() {
      const BASE =
        "http://mochi.test:8888/browser/browser/components/extensions/test/browser/";
      const URL = BASE + "file_bypass_cache.sjs";

      let tabId = null;
      let loadPromise, resolveLoad;
      function resetLoad() {
        loadPromise = new Promise(resolve => {
          resolveLoad = resolve;
        });
      }
      function awaitLoad() {
        return loadPromise.then(() => {
          resetLoad();
        });
      }
      resetLoad();

      browser.tabs.onUpdated.addListener(function listener(
        tabId_,
        changed,
        tab
      ) {
        if (tabId == tabId_ && changed.status == "complete" && tab.url == URL) {
          resolveLoad();
        }
      });

      try {
        let tab = await browser.tabs.create({ url: URL });
        tabId = tab.id;
        await awaitLoad();

        await browser.tabs.reload(tab.id, { bypassCache: false });
        await awaitLoad();

        let [textContent] = await browser.tabs.executeScript(tab.id, {
          code: "document.body.textContent",
        });
        browser.test.assertEq(
          "",
          textContent,
          "`textContent` should be empty when bypassCache=false"
        );

        await browser.tabs.reload(tab.id, { bypassCache: true });
        await awaitLoad();

        [textContent] = await browser.tabs.executeScript(tab.id, {
          code: "document.body.textContent",
        });

        let [pragma, cacheControl] = textContent.split(":");
        browser.test.assertEq(
          "no-cache",
          pragma,
          "`pragma` should be set to `no-cache` when bypassCache is true"
        );
        browser.test.assertEq(
          "no-cache",
          cacheControl,
          "`cacheControl` should be set to `no-cache` when bypassCache is true"
        );

        await browser.tabs.remove(tab.id);

        browser.test.notifyPass("tabs.reload_bypass_cache");
      } catch (error) {
        browser.test.fail(`${error} :: ${error.stack}`);
        browser.test.notifyFail("tabs.reload_bypass_cache");
      }
    },
  });

  await extension.startup();
  await extension.awaitFinish("tabs.reload_bypass_cache");
  await extension.unload();
});
