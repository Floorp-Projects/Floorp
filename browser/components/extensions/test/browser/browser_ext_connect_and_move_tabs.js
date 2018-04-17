/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// Tests that the Port object created by browser.runtime.connect is not
// prematurely disconnected as the underlying message managers change when a
// tab is moved between windows.

function loadExtension() {
  return ExtensionTestUtils.loadExtension({
    manifest: {
      "content_scripts": [{
        "js": ["script.js"],
        "matches": ["http://mochi.test/?discoTest"],
      }],
    },
    background() {
      browser.runtime.onConnect.addListener((port) => {
        port.onDisconnect.addListener(() => {
          browser.test.fail("onDisconnect should not fire because the port is to be closed from this side");
          browser.test.sendMessage("port_disconnected");
        });
        port.onMessage.addListener(async msg => {
          browser.test.assertEq("connect_from_script", msg, "expected message");
          // Move a tab to a new window and back. Regression test for bugzil.la/1448674
          let {windowId, id: tabId, index} = port.sender.tab;
          await browser.windows.create({tabId});
          await browser.tabs.move(tabId, {index, windowId});
          await browser.windows.create({tabId});
          await browser.tabs.move(tabId, {index, windowId});
          try {
            // When the port is unexpectedly disconnected, postMessage will throw an error.
            port.postMessage("ping");
          } catch (e) {
            browser.test.fail(`Error: ${e} :: ${e.stack}`);
            browser.test.sendMessage("port_ping_ponged_before_disconnect");
          }
        });

        browser.runtime.onMessage.addListener(async (msg, sender) => {
          if (msg == "disconnect-me") {
            port.disconnect();
            // Now port.onDisconnect should fire in the content script.
          } else if (msg == "close-tab") {
            await browser.tabs.remove(sender.tab.id);
            browser.test.sendMessage("closed_tab");
          }
        });
      });

      browser.test.onMessage.addListener(msg => {
        browser.test.assertEq("open_extension_tab", msg, "expected message");
        browser.tabs.create({url: "tab.html"});
      });
    },

    files: {
      "tab.html": `
        <!DOCTYPE html><meta charset="utf-8">
        <script src="script.js"></script>
      `,
      "script.js": function() {
        let port = browser.runtime.connect();
        port.onMessage.addListener(msg => {
          browser.test.assertEq("ping", msg, "expected message");
          browser.test.sendMessage("port_ping_ponged_before_disconnect");
          port.onDisconnect.addListener(() => {
            browser.test.sendMessage("port_disconnected");
            browser.runtime.sendMessage("close-tab");
          });
          browser.runtime.sendMessage("disconnect-me");
        });
        port.postMessage("connect_from_script");
      },
    },
  });
}

add_task(async function contentscript_connect_and_move_tabs() {
  let extension = loadExtension();
  await extension.startup();
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/?discoTest");
  await extension.awaitMessage("port_ping_ponged_before_disconnect");
  await extension.awaitMessage("port_disconnected");
  await extension.awaitMessage("closed_tab");
  await extension.unload();
});

add_task(async function extension_tab_connect_and_move_tabs() {
  let extension = loadExtension();
  await extension.startup();
  extension.sendMessage("open_extension_tab");
  await extension.awaitMessage("port_ping_ponged_before_disconnect");
  await extension.awaitMessage("port_disconnected");
  await extension.awaitMessage("closed_tab");
  await extension.unload();
});
