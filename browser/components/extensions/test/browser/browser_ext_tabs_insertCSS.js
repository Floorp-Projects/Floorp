/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* testExecuteScript() {
  let {MessageChannel} = Cu.import("resource://gre/modules/MessageChannel.jsm", {});

  let messageManagersSize = MessageChannel.messageManagers.size;
  let responseManagersSize = MessageChannel.responseManagers.size;

  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/", true);

  function background() {
    let promises = [
      {
        background: "transparent",
        foreground: "rgb(0, 113, 4)",
        promise: () => {
          return browser.tabs.insertCSS({
            file: "file2.css",
          });
        },
      },
      {
        background: "rgb(42, 42, 42)",
        foreground: "rgb(0, 113, 4)",
        promise: () => {
          return browser.tabs.insertCSS({
            code: "* { background: rgb(42, 42, 42) }",
          });
        },
      },
    ];

    function checkCSS() {
      let computedStyle = window.getComputedStyle(document.body);
      return [computedStyle.backgroundColor, computedStyle.color];
    }

    function next() {
      if (!promises.length) {
        return;
      }

      let {promise, background, foreground} = promises.shift();
      return promise().then(result => {
        browser.test.assertEq(undefined, result, "Expected callback result");

        return browser.tabs.executeScript({
          code: `(${checkCSS})()`,
        });
      }).then(([result]) => {
        browser.test.assertEq(background, result[0], "Expected background color");
        browser.test.assertEq(foreground, result[1], "Expected foreground color");
        return next();
      });
    }

    next().then(() => {
      browser.test.notifyPass("insertCSS");
    }).catch(e => {
      browser.test.fail(`Error: ${e} :: ${e.stack}`);
      browser.test.notifyFailure("insertCSS");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["http://mochi.test/"],
    },

    background,

    files: {
      "file2.css": "* { color: rgb(0, 113, 4) }",
    },
  });

  yield extension.startup();

  yield extension.awaitFinish("insertCSS");

  yield extension.unload();

  yield BrowserTestUtils.removeTab(tab);

  // Make sure that we're not holding on to references to closed message
  // managers.
  is(MessageChannel.messageManagers.size, messageManagersSize, "Message manager count");
  is(MessageChannel.responseManagers.size, responseManagersSize, "Response manager count");
  is(MessageChannel.pendingResponses.size, 0, "Pending response count");
});
