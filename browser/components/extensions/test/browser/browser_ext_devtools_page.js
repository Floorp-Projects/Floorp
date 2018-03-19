/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const {require} = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
const {gDevTools} = require("devtools/client/framework/devtools");

/**
 * This test file ensures that:
 *
 * - the devtools_page property creates a new WebExtensions context
 * - the devtools_page can exchange messages with the background page
 */
add_task(async function test_devtools_page_runtime_api_messaging() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/");

  function background() {
    browser.runtime.onMessage.addListener((msg, sender) => {
      if (sender.tab) {
        browser.test.sendMessage("content_script_message_received");
      }
    });

    browser.runtime.onConnect.addListener((port) => {
      if (port.sender.tab) {
        browser.test.sendMessage("content_script_port_received");
        return;
      }

      let portMessageReceived = false;

      port.onDisconnect.addListener(() => {
        browser.test.assertTrue(portMessageReceived,
                                "Got a port message before the port disconnect event");
        browser.test.sendMessage("devtools_page_connect.done");
      });

      port.onMessage.addListener((msg) => {
        portMessageReceived = true;
        browser.test.assertEq("devtools -> background port message", msg,
                              "Got the expected message from the devtools page");
        port.postMessage("background -> devtools port message");
      });
    });
  }

  function devtools_page() {
    browser.runtime.onConnect.addListener(port => {
      // Fail if a content script port has been received by the devtools page (Bug 1383310).
      if (port.sender.tab) {
        browser.test.fail(`A DevTools page should not receive ports from content scripts`);
      }
    });

    browser.runtime.onMessage.addListener((msg, sender) => {
      // Fail if a content script message has been received by the devtools page (Bug 1383310).
      if (sender.tab) {
        browser.test.fail(`A DevTools page should not receive messages from content scripts`);
      }
    });

    const port = browser.runtime.connect();
    port.onMessage.addListener((msg) => {
      browser.test.assertEq("background -> devtools port message", msg,
                            "Got the expected message from the background page");
      port.disconnect();
    });
    port.postMessage("devtools -> background port message");

    browser.test.sendMessage("devtools_page_loaded");
  }

  function content_script() {
    browser.test.onMessage.addListener(msg => {
      switch (msg) {
        case "content_script.send_message":
          browser.runtime.sendMessage("content_script_message");
          break;
        case "content_script.connect_port":
          const port = browser.runtime.connect();
          port.disconnect();
          break;
        default:
          browser.test.fail(`Unexpected message ${msg} received by content script`);
      }
    });

    browser.test.sendMessage("content_script_loaded");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      devtools_page: "devtools_page.html",
      content_scripts: [
        {
          js: ["content_script.js"],
          matches: ["http://mochi.test/*"],
        },
      ],
    },
    files: {
      "content_script.js": content_script,
      "devtools_page.html": `<!DOCTYPE html>
        <html>
          <head>
            <meta charset="utf-8">
          </head>
          <body>
            <script src="devtools_page.js"></script>
          </body>
        </html>`,
      "devtools_page.js": devtools_page,
    },
  });

  await extension.startup();

  info("Wait the content script load");
  await extension.awaitMessage("content_script_loaded");

  let target = gDevTools.getTargetForTab(tab);

  info("Open the developer toolbox");
  await gDevTools.showToolbox(target, "webconsole");

  info("Wait the devtools page load");
  await extension.awaitMessage("devtools_page_loaded");

  info("Wait the connection 'devtools_page -> background' to complete");
  await extension.awaitMessage("devtools_page_connect.done");

  // Send a message from the content script and expect it to be received from
  // the background page (repeated twice to be sure that the devtools_page had
  // the chance to receive the message and fail as expected).
  info("Wait for 2 content script messages to be received from the background page");
  extension.sendMessage("content_script.send_message");
  await extension.awaitMessage("content_script_message_received");
  extension.sendMessage("content_script.send_message");
  await extension.awaitMessage("content_script_message_received");

  // Create a port from the content script and expect a port to be received from
  // the background page (repeated twice to be sure that the devtools_page had
  // the chance to receive the message and fail as expected).
  info("Wait for 2 content script ports to be received from the background page");
  extension.sendMessage("content_script.connect_port");
  await extension.awaitMessage("content_script_port_received");
  extension.sendMessage("content_script.connect_port");
  await extension.awaitMessage("content_script_port_received");

  await gDevTools.closeToolbox(target);

  await target.destroy();

  await extension.unload();

  BrowserTestUtils.removeTab(tab);
});

/**
 * This test file ensures that:
 *
 * - the devtools_page can exchange messages with an extension tab page
 */

add_task(async function test_devtools_page_and_extension_tab_messaging() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/");

  function background() {
    browser.runtime.onMessage.addListener((msg, sender) => {
      if (sender.tab) {
        browser.test.sendMessage("extension_tab_message_received");
      }
    });

    browser.runtime.onConnect.addListener((port) => {
      if (port.sender.tab) {
        browser.test.sendMessage("extension_tab_port_received");
      }
    });

    browser.tabs.create({url: browser.runtime.getURL("extension_tab.html")});
  }

  function devtools_page() {
    browser.runtime.onConnect.addListener(port => {
      browser.test.sendMessage("devtools_page_onconnect");
    });

    browser.runtime.onMessage.addListener((msg, sender) => {
      browser.test.sendMessage("devtools_page_onmessage");
    });

    browser.test.sendMessage("devtools_page_loaded");
  }

  function extension_tab() {
    browser.test.onMessage.addListener(msg => {
      switch (msg) {
        case "extension_tab.send_message":
          browser.runtime.sendMessage("content_script_message");
          break;
        case "extension_tab.connect_port":
          const port = browser.runtime.connect();
          port.disconnect();
          break;
        default:
          browser.test.fail(`Unexpected message ${msg} received by content script`);
      }
    });

    browser.test.sendMessage("extension_tab_loaded");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      devtools_page: "devtools_page.html",
    },
    files: {
      "extension_tab.html": `<!DOCTYPE html>
        <html>
          <head>
            <meta charset="utf-8">
          </head>
          <body>
            <h1>Extension Tab Page</h1>
            <script src="extension_tab.js"></script>
          </body>
        </html>`,
      "extension_tab.js": extension_tab,
      "devtools_page.html": `<!DOCTYPE html>
        <html>
          <head>
            <meta charset="utf-8">
          </head>
          <body>
            <script src="devtools_page.js"></script>
          </body>
        </html>`,
      "devtools_page.js": devtools_page,
    },
  });

  await extension.startup();

  info("Wait the extension tab page load");
  await extension.awaitMessage("extension_tab_loaded");

  let target = gDevTools.getTargetForTab(tab);

  info("Open the developer toolbox");
  await gDevTools.showToolbox(target, "webconsole");

  info("Wait the devtools page load");
  await extension.awaitMessage("devtools_page_loaded");

  extension.sendMessage("extension_tab.send_message");

  info("Wait for an extension tab message to be received from the devtools page");
  await extension.awaitMessage("devtools_page_onmessage");

  info("Wait for an extension tab message to be received from the background page");
  await extension.awaitMessage("extension_tab_message_received");

  extension.sendMessage("extension_tab.connect_port");

  info("Wait for an extension tab port to be received from the devtools page");
  await extension.awaitMessage("devtools_page_onconnect");

  info("Wait for an extension tab port to be received from the background page");
  await extension.awaitMessage("extension_tab_port_received");

  await gDevTools.closeToolbox(target);

  await target.destroy();

  await extension.unload();

  BrowserTestUtils.removeTab(tab);
});
