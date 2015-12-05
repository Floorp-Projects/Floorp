/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* () {
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/");

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background: function() {
      let messages_received = [];

      let tabId;

      browser.runtime.onConnect.addListener((port) => {
        browser.test.assertTrue(!!port, "tab to background port received");
        browser.test.assertEq("tab-connection-name", port.name, "port name should be defined and equal to connectInfo.name");
        browser.test.assertTrue(!!port.sender.tab, "port.sender.tab should be defined");
        browser.test.assertEq(tabId, port.sender.tab.id, "port.sender.tab.id should be equal to the expected tabId");

        port.onMessage.addListener((msg) => {
          messages_received.push(msg);

          if (messages_received.length == 1) {
            browser.test.assertEq("tab to background port message", msg, "'tab to background' port message received");
            port.postMessage("background to tab port message");
          }

          if (messages_received.length == 2) {
            browser.test.assertTrue(!!msg.tabReceived, "'background to tab' reply port message received");
            browser.test.assertEq("background to tab port message", msg.tabReceived, "reply port content contains the message received");

            browser.test.notifyPass("tabRuntimeConnect.pass");
          }
        });
      });

      browser.tabs.create({ url: "tab.html" },
                          (tab) => { tabId = tab.id; });
    },

    files: {
      "tab.js": function() {
        let port = browser.runtime.connect({ name: "tab-connection-name"});
        port.postMessage("tab to background port message");
        port.onMessage.addListener((msg) => {
          port.postMessage({ tabReceived: msg });
        });
      },
      "tab.html": `
        <!DOCTYPE html>
        <html>
          <head>
            <title>test tab extension page</title>
            <meta charset="utf-8">
            <script src="tab.js" async></script>
          </head>
          <body>
            <h1>test tab extension page</h1>
          </body>
        </html>
      `,
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("tabRuntimeConnect.pass");
  yield extension.unload();

  yield BrowserTestUtils.removeTab(tab);
});
