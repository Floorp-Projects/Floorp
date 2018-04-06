/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function connect_from_tab_to_bg_and_crash_tab() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "content_scripts": [{
        "js": ["contentscript.js"],
        "matches": ["http://example.com/?crashme"],
      }],
    },

    background() {
      browser.runtime.onConnect.addListener((port) => {
        browser.test.assertEq("tab_to_bg", port.name, "expected port");
        port.onDisconnect.addListener(() => {
          browser.test.assertEq(null, port.error, "port should be disconnected without errors");
          browser.test.sendMessage("port_disconnected");
        });
        browser.test.sendMessage("bg_runtime_onConnect");
      });
    },

    files: {
      "contentscript.js": function() {
        let port = browser.runtime.connect({name: "tab_to_bg"});
        port.onDisconnect.addListener(() => {
          browser.test.fail("Unexpected onDisconnect event in content script");
        });
      },
    },
  });

  await extension.startup();

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/?crashme");
  await extension.awaitMessage("bg_runtime_onConnect");
  // Force the message manager to disconnect without giving the content a
  // chance to send an "Extension:Port:Disconnect" message.
  await BrowserTestUtils.crashBrowser(tab.linkedBrowser);
  await extension.awaitMessage("port_disconnected");
  BrowserTestUtils.removeTab(tab);
  await extension.unload();
});

add_task(async function connect_from_bg_to_tab_and_crash_tab() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "content_scripts": [{
        "js": ["contentscript.js"],
        "matches": ["http://example.com/?crashme"],
      }],
    },

    background() {
      browser.runtime.onMessage.addListener((msg, sender) => {
        browser.test.assertEq("contentscript_ready", msg, "expected message");
        let port = browser.tabs.connect(sender.tab.id, {name: "bg_to_tab"});
        port.onDisconnect.addListener(() => {
          browser.test.assertEq(null, port.error, "port should be disconnected without errors");
          browser.test.sendMessage("port_disconnected");
        });
      });
    },

    files: {
      "contentscript.js": function() {
        browser.runtime.onConnect.addListener((port) => {
          browser.test.assertEq("bg_to_tab", port.name, "expected port");
          port.onDisconnect.addListener(() => {
            browser.test.fail("Unexpected onDisconnect event in content script");
          });
          browser.test.sendMessage("tab_runtime_onConnect");
        });
        browser.runtime.sendMessage("contentscript_ready");
      },
    },
  });

  await extension.startup();

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/?crashme");
  await extension.awaitMessage("tab_runtime_onConnect");
  // Force the message manager to disconnect without giving the content a
  // chance to send an "Extension:Port:Disconnect" message.
  await BrowserTestUtils.crashBrowser(tab.linkedBrowser);
  await extension.awaitMessage("port_disconnected");
  BrowserTestUtils.removeTab(tab);
  await extension.unload();
});
