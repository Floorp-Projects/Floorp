"use strict";

const SESSION = {
  windows: [{
    tabs: [
      {entries: [{url: "about:blank"}]},
      {entries: [{url: "https://example.com/"}]},
    ],
  }],
};

add_task(async function() {
  SessionStore.setBrowserState(JSON.stringify(SESSION));
  const tab = gBrowser.tabs[1];

  is(tab.getAttribute("pending"), "true", "The tab is pending restore");
  is(tab.linkedBrowser.isConnected, false, "The tab is lazy");

  async function background() {
    const [tab] = await browser.tabs.query({url: "https://example.com/"});
    browser.test.assertRejects(
      browser.tabs.sendMessage(tab.id, "void"),
      /Could not establish connection. Receiving end does not exist/,
      "No recievers in a tab pending restore."
    );
    browser.test.notifyPass("lazy");
  }

  const manifest = {permissions: ["tabs"]};
  const extension = ExtensionTestUtils.loadExtension({manifest, background});

  await extension.startup();
  await extension.awaitFinish("lazy");
  await extension.unload();

  is(tab.getAttribute("pending"), "true", "The tab is still pending restore");
  is(tab.linkedBrowser.isConnected, false, "The tab is still lazy");

  await BrowserTestUtils.removeTab(tab);
});
