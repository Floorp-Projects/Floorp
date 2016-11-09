"use strict";

const {
  LegacyExtensionContext,
} = Cu.import("resource://gre/modules/LegacyExtensionsUtils.jsm", {});

function promiseAddonStartup(extension) {
  const {Management} = Cu.import("resource://gre/modules/Extension.jsm", {});

  return new Promise((resolve) => {
    let listener = (evt, extensionInstance) => {
      Management.off("startup", listener);
      resolve(extensionInstance);
    };
    Management.on("startup", listener);
  });
}

/**
 * This test case ensures that the LegacyExtensionContext can receive a connection
 * from a content script and that the received port contains the expected sender
 * tab info.
 */
add_task(function* test_legacy_extension_context_contentscript_connection() {
  function backgroundScript() {
    // Extract the assigned uuid from the background page url and send it
    // in a test message.
    let uuid = window.location.hostname;

    browser.test.onMessage.addListener(async msg => {
      if (msg == "open-test-tab") {
        let tab = await browser.tabs.create({url: "http://example.com/"});
        browser.test.sendMessage("get-expected-sender-info",
                                 {uuid, tab});
      } else if (msg == "close-current-tab") {
        try {
          let [tab] = await browser.tabs.query({active: true});
          await browser.tabs.remove(tab.id);
          browser.test.sendMessage("current-tab-closed", true);
        } catch (e) {
          browser.test.sendMessage("current-tab-closed", false);
        }
      }
    });

    browser.test.sendMessage("ready");
  }

  function contentScript() {
    browser.runtime.sendMessage("webextension -> legacy_extension message", (reply) => {
      browser.test.assertEq("legacy_extension -> webextension reply", reply,
                           "Got the expected reply from the LegacyExtensionContext");
      browser.test.sendMessage("got-reply-message");
    });

    let port = browser.runtime.connect();

    port.onMessage.addListener(msg => {
      browser.test.assertEq("legacy_extension -> webextension port message", msg,
                            "Got the expected message from the LegacyExtensionContext");
      port.postMessage("webextension -> legacy_extension port message");
    });
  }

  let extensionData = {
    background: `new ${backgroundScript}`,
    manifest: {
      content_scripts: [
        {
          matches: ["http://example.com/*"],
          js: ["content-script.js"],
        },
      ],
    },
    files: {
      "content-script.js": `new ${contentScript}`,
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);

  let waitForExtensionReady = extension.awaitMessage("ready");

  let waitForExtensionInstance = promiseAddonStartup(extension);

  extension.startup();

  let extensionInstance = yield waitForExtensionInstance;

  // Connect to the target extension.id as an external context
  // using the given custom sender info.
  let legacyContext = new LegacyExtensionContext(extensionInstance);

  let waitConnectPort = new Promise(resolve => {
    let {browser} = legacyContext.api;
    browser.runtime.onConnect.addListener(port => {
      resolve(port);
    });
  });

  let waitMessage = new Promise(resolve => {
    let {browser} = legacyContext.api;
    browser.runtime.onMessage.addListener((singleMsg, msgSender, sendReply) => {
      sendReply("legacy_extension -> webextension reply");
      resolve({singleMsg, msgSender});
    });
  });

  is(legacyContext.envType, "legacy_extension",
     "LegacyExtensionContext instance has the expected type");

  ok(legacyContext.api, "Got the API object");

  yield waitForExtensionReady;

  extension.sendMessage("open-test-tab");

  let {uuid, tab} = yield extension.awaitMessage("get-expected-sender-info");

  let {singleMsg, msgSender} = yield waitMessage;
  is(singleMsg, "webextension -> legacy_extension message",
     "Got the expected message");
  ok(msgSender, "Got a message sender object");

  is(msgSender.id, uuid, "The sender has the expected id property");
  is(msgSender.url, "http://example.com/", "The sender has the expected url property");
  ok(msgSender.tab, "The sender has a tab property");
  is(msgSender.tab.id, tab.id, "The port sender has the expected tab.id");

  // Wait confirmation that the reply has been received.
  yield extension.awaitMessage("got-reply-message");

  let port = yield waitConnectPort;

  ok(port, "Got the Port API object");
  ok(port.sender, "The port has a sender property");

  is(port.sender.id, uuid, "The port sender has an id property");
  is(port.sender.url, "http://example.com/", "The port sender has the expected url property");
  ok(port.sender.tab, "The port sender has a tab property");
  is(port.sender.tab.id, tab.id, "The port sender has the expected tab.id");

  let waitPortMessage = new Promise(resolve => {
    port.onMessage.addListener((msg) => {
      resolve(msg);
    });
  });

  port.postMessage("legacy_extension -> webextension port message");

  let msg = yield waitPortMessage;

  is(msg, "webextension -> legacy_extension port message",
     "LegacyExtensionContext received the expected message from the webextension");

  let waitForDisconnect = new Promise(resolve => {
    port.onDisconnect.addListener(resolve);
  });

  let waitForTestDone = extension.awaitMessage("current-tab-closed");

  extension.sendMessage("close-current-tab");

  yield waitForDisconnect;

  info("Got the disconnect event on tab closed");

  let success = yield waitForTestDone;

  ok(success, "Test completed successfully");

  yield extension.unload();
});
