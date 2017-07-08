"use strict";

add_task(async function testLastAccessed() {
  let past = Date.now();

  await BrowserTestUtils.openNewForegroundTab(gBrowser, "https://example.com/?1");
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "https://example.com/?2");

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },
    async background() {
      browser.test.onMessage.addListener(async function(msg, past) {
        if (msg !== "past") {
          return;
        }

        let [tab1] = await browser.tabs.query({url: "https://example.com/?1"});
        let [tab2] = await browser.tabs.query({url: "https://example.com/?2"});

        browser.test.assertTrue(tab1 && tab2, "Expected tabs were found");

        let now = Date.now();

        browser.test.assertTrue(past < tab1.lastAccessed &&
                                tab1.lastAccessed < tab2.lastAccessed &&
                                tab2.lastAccessed <= now,
                                "lastAccessed timestamps are recent and in the right order");

        await browser.tabs.remove([tab1.id, tab2.id]);

        browser.test.notifyPass("tabs.lastAccessed");
      });
    },
  });

  await extension.startup();
  await extension.sendMessage("past", past);
  await extension.awaitFinish("tabs.lastAccessed");
  await extension.unload();
});
