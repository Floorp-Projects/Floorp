"use strict";

add_task(async function testLastAccessed() {
  let past = Date.now();

  for (let url of ["https://example.com/?1", "https://example.com/?2"]) {
    let tab = BrowserTestUtils.addTab(gBrowser, url, {skipAnimation: true});
    await BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, url);
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },
    async background() {
      browser.test.onMessage.addListener(async function(msg, past) {
        let [tab1] = await browser.tabs.query({url: "https://example.com/?1"});
        let [tab2] = await browser.tabs.query({url: "https://example.com/?2"});

        browser.test.assertTrue(tab1 && tab2, "Expected tabs were found");

        let now = Date.now();

        browser.test.assertTrue(past < tab1.lastAccessed,
                                "lastAccessed of tab 1 is later than the test start time.");
        browser.test.assertTrue(tab1.lastAccessed < tab2.lastAccessed,
                                "lastAccessed of tab 2 is later than lastAccessed of tab 1.");
        browser.test.assertTrue(tab2.lastAccessed <= now,
                                "lastAccessed of tab 2 is earlier than now.");

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
