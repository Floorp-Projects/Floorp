"use strict";

const {Utils} = Cu.import("resource://gre/modules/sessionstore/Utils.jsm", {});
const triggeringPrincipal_base64 = Utils.SERIALIZED_SYSTEMPRINCIPAL;

const SESSION = {
  windows: [{
    tabs: [
      {entries: [{url: "about:blank", triggeringPrincipal_base64}]},
      {entries: [{url: "https://example.com/", triggeringPrincipal_base64}]},
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
