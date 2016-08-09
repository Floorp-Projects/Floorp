/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* () {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs", "<all_urls>"],
    },

    background: function() {
      const BASE = "http://mochi.test:8888/browser/browser/components/extensions/test/browser/";
      const URL = BASE + "file_bypass_cache.sjs";

      function awaitLoad(tabId) {
        return new Promise(resolve => {
          browser.tabs.onUpdated.addListener(function listener(tabId_, changed, tab) {
            if (tabId == tabId_ && changed.status == "complete" && tab.url == URL) {
              browser.tabs.onUpdated.removeListener(listener);
              resolve();
            }
          });
        });
      }

      let tabId;
      browser.tabs.create({url: URL}).then((tab) => {
        tabId = tab.id;
        return awaitLoad(tabId);
      }).then(() => {
        return browser.tabs.reload(tabId, {bypassCache: false});
      }).then(() => {
        return awaitLoad(tabId);
      }).then(() => {
        return browser.tabs.executeScript(tabId, {code: "document.body.textContent"});
      }).then(([textContent]) => {
        browser.test.assertEq("", textContent, "`textContent` should be empty when bypassCache=false");
        return browser.tabs.reload(tabId, {bypassCache: true});
      }).then(() => {
        return awaitLoad(tabId);
      }).then(() => {
        return browser.tabs.executeScript(tabId, {code: "document.body.textContent"});
      }).then(([textContent]) => {
        let [pragma, cacheControl] = textContent.split(":");
        browser.test.assertEq("no-cache", pragma, "`pragma` should be set to `no-cache` when bypassCache is true");
        browser.test.assertEq("no-cache", cacheControl, "`cacheControl` should be set to `no-cache` when bypassCache is true");
        browser.tabs.remove(tabId);
        browser.test.notifyPass("tabs.reload_bypass_cache");
      }).catch(error => {
        browser.test.fail(`${error} :: ${error.stack}`);
        browser.test.notifyFail("tabs.reload_bypass_cache");
      });
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("tabs.reload_bypass_cache");
  yield extension.unload();
});
