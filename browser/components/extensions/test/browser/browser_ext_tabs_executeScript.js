/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* testExecuteScript() {
  let {MessageChannel} = Cu.import("resource://gre/modules/MessageChannel.jsm", {});

  let messageManagersSize = MessageChannel.messageManagers.size;
  let responseManagersSize = MessageChannel.responseManagers.size;

  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/", true);

  function background() {
    browser.tabs.executeScript({
      file: "script.js",
      code: "42",
    }, result => {
      browser.test.assertEq(42, result, "Expected callback result");
      browser.test.sendMessage("got result", result);
    });

    browser.tabs.executeScript({
      file: "script2.js",
    }, result => {
      browser.test.assertEq(27, result, "Expected callback result");
      browser.test.sendMessage("got callback", result);
    });

    browser.runtime.onMessage.addListener(message => {
      browser.test.assertEq("script ran", message, "Expected runtime message");
      browser.test.sendMessage("got message", message);
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["http://mochi.test/"],
    },

    background,

    files: {
      "script.js": function() {
        browser.runtime.sendMessage("script ran");
      },

      "script2.js": "27",
    },
  });

  yield extension.startup();

  yield extension.awaitMessage("got result");
  yield extension.awaitMessage("got callback");
  yield extension.awaitMessage("got message");

  yield extension.unload();

  yield BrowserTestUtils.removeTab(tab);

  // Make sure that we're not holding on to references to closed message
  // managers.
  is(MessageChannel.messageManagers.size, messageManagersSize, "Message manager count");
  is(MessageChannel.responseManagers.size, responseManagersSize, "Response manager count");
  is(MessageChannel.pendingResponses.size, 0, "Pending response count");
});
