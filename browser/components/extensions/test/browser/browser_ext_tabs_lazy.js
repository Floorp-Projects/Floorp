"use strict";

const { E10SUtils } = ChromeUtils.import(
  "resource://gre/modules/E10SUtils.jsm"
);
const triggeringPrincipal_base64 = E10SUtils.SERIALIZED_SYSTEMPRINCIPAL;

const SESSION = {
  windows: [
    {
      tabs: [
        { entries: [{ url: "about:blank", triggeringPrincipal_base64 }] },
        {
          entries: [
            { url: "https://example.com/", triggeringPrincipal_base64 },
          ],
        },
      ],
    },
  ],
};

add_task(async function() {
  SessionStore.setBrowserState(JSON.stringify(SESSION));
  await promiseWindowRestored(window);
  const tab = gBrowser.tabs[1];

  is(tab.getAttribute("pending"), "true", "The tab is pending restore");
  is(tab.linkedBrowser.isConnected, false, "The tab is lazy");

  async function background() {
    const [tab] = await browser.tabs.query({ url: "https://example.com/" });
    browser.test.assertRejects(
      browser.tabs.sendMessage(tab.id, "void"),
      /Could not establish connection. Receiving end does not exist/,
      "No recievers in a tab pending restore."
    );
    browser.test.notifyPass("lazy");
  }

  const manifest = { permissions: ["tabs"] };
  const extension = ExtensionTestUtils.loadExtension({ manifest, background });

  await extension.startup();
  await extension.awaitFinish("lazy");
  await extension.unload();

  is(tab.getAttribute("pending"), "true", "The tab is still pending restore");
  is(tab.linkedBrowser.isConnected, false, "The tab is still lazy");

  BrowserTestUtils.removeTab(tab);
});
