/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env webextensions */

"use strict";

const TEST_URL = "http://example.com/";

// These allowed rejections are copied from
// browser/components/extensions/test/browser/head.js.
const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);
PromiseTestUtils.whitelistRejectionsGlobally(/Message manager disconnected/);
PromiseTestUtils.whitelistRejectionsGlobally(/Receiving end does not exist/);

add_task(async function test_port_kept_connected_on_switch_to_RDB() {
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: [TEST_URL],
          js: ["content-script.js"],
          run_at: "document_start",
        },
      ],
    },
    background() {
      let currentPort;

      browser.runtime.onConnect.addListener(port => {
        currentPort = port;
        port.onDisconnect.addListener(() =>
          browser.test.sendMessage("port-disconnected")
        );
        port.onMessage.addListener(msg =>
          browser.test.sendMessage("port-message-received", msg)
        );
        browser.test.sendMessage("port-connected");
      });

      browser.test.onMessage.addListener(async msg => {
        if (msg !== "test:port-message-send") {
          browser.test.fail(`Unexpected test message received: ${msg}`);
        }

        currentPort.postMessage("ping");
      });

      browser.test.sendMessage("background:ready");
    },
    files: {
      "content-script.js": function contentScript() {
        const port = browser.runtime.connect();
        port.onMessage.addListener(msg => port.postMessage(`${msg}-pong`));
      },
    },
  });

  await extension.startup();

  await extension.awaitMessage("background:ready");

  const tab = await addTab(TEST_URL);

  await extension.awaitMessage("port-connected");

  await openRDM(tab);

  extension.sendMessage("test:port-message-send");

  is(
    await extension.awaitMessage("port-message-received"),
    "ping-pong",
    "Got the expected message back from the content script"
  );

  await closeRDM(tab);

  extension.sendMessage("test:port-message-send");

  is(
    await extension.awaitMessage("port-message-received"),
    "ping-pong",
    "Got the expected message back from the content script"
  );

  await removeTab(tab);

  await extension.awaitMessage("port-disconnected");

  await extension.unload();
});

add_task(async function test_tab_sender() {
  const tab = await addTab(TEST_URL);
  await openRDM(tab);

  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],

      content_scripts: [
        {
          matches: [TEST_URL],
          js: ["content-script.js"],
          run_at: "document_start",
        },
      ],
    },

    async background() {
      const TEST_URL = "http://example.com/"; // eslint-disable-line no-shadow

      browser.test.log("Background script init");

      let extTab;
      const contentMessage = new Promise(resolve => {
        browser.test.log("Listen to content");
        const listener = async (msg, sender, respond) => {
          browser.test.assertEq(
            msg,
            "hello-from-content",
            "Background script got hello-from-content message"
          );

          const tabs = await browser.tabs.query({
            currentWindow: true,
            active: true,
          });
          browser.test.assertEq(
            tabs.length,
            1,
            "One tab is active in the current window"
          );
          extTab = tabs[0];
          browser.test.log(`Tab: id ${extTab.id}, url ${extTab.url}`);
          browser.test.assertEq(extTab.url, TEST_URL, "Tab has the test URL");

          browser.test.assertTrue(!!sender, "Message has a sender");
          browser.test.assertTrue(!!sender.tab, "Message has a sender.tab");
          browser.test.assertEq(
            sender.tab.id,
            extTab.id,
            "Sender's tab ID matches the RDM tab ID"
          );
          browser.test.assertEq(
            sender.tab.url,
            extTab.url,
            "Sender's tab URL matches the RDM tab URL"
          );

          browser.runtime.onMessage.removeListener(listener);
          resolve();
        };
        browser.runtime.onMessage.addListener(listener);
      });

      // Wait for "resume" message so we know the content script is also ready.
      await new Promise(resolve => {
        browser.test.onMessage.addListener(resolve);
        browser.test.sendMessage("background-script-ready");
      });

      await contentMessage;

      browser.test.log("Send message from background to content");
      const contentSender = await browser.tabs.sendMessage(
        extTab.id,
        "hello-from-background"
      );
      browser.test.assertEq(
        contentSender.id,
        browser.runtime.id,
        "The sender ID in content matches this extension"
      );

      browser.test.notifyPass("rdm-messaging");
    },

    files: {
      "content-script.js": async function() {
        browser.test.log("Content script init");

        browser.test.log("Listen to background");
        browser.runtime.onMessage.addListener((msg, sender, respond) => {
          browser.test.assertEq(
            msg,
            "hello-from-background",
            "Content script got hello-from-background message"
          );

          browser.test.assertTrue(!!sender, "Message has a sender");
          browser.test.assertTrue(!!sender.id, "Message has a sender.id");

          const { id } = sender;
          respond({ id });
        });

        // Wait for "resume" message so we know the background script is also ready.
        await new Promise(resolve => {
          browser.test.onMessage.addListener(resolve);
          browser.test.sendMessage("content-script-ready");
        });

        browser.test.log("Send message from content to background");
        browser.runtime.sendMessage("hello-from-content");
      },
    },
  });

  const contentScriptReady = extension.awaitMessage("content-script-ready");
  const backgroundScriptReady = extension.awaitMessage(
    "background-script-ready"
  );
  const finish = extension.awaitFinish("rdm-messaging");

  await extension.startup();

  // It appears the background script and content script can loaded in either order, so
  // we'll wait for the both to listen before proceeding.
  await backgroundScriptReady;
  await contentScriptReady;
  extension.sendMessage("resume");

  await finish;
  await extension.unload();

  await closeRDM(tab);
  await removeTab(tab);
});
