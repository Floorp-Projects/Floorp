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
        background: "rgb(0, 0, 0)",
        foreground: "rgb(255, 192, 203)",
        promise: resolve => {
          browser.tabs.insertCSS({
            file: "file1.css",
            code: "* { background: black }",
          }, result => {
            browser.test.assertEq(undefined, result, "Expected callback result");
            resolve();
          });
        },
      },
      {
        background: "rgb(0, 0, 0)",
        foreground: "rgb(0, 113, 4)",
        promise: resolve => {
          browser.tabs.insertCSS({
            file: "file2.css",
          }, result => {
            browser.test.assertEq(undefined, result, "Expected callback result");
            resolve();
          });
        },
      },
      {
        background: "rgb(42, 42, 42)",
        foreground: "rgb(0, 113, 4)",
        promise: resolve => {
          browser.tabs.insertCSS({
            code: "* { background: rgb(42, 42, 42) }",
          }, result => {
            browser.test.assertEq(undefined, result, "Expected callback result");
            resolve();
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
        browser.test.notifyPass("insertCSS");
        return;
      }

      let { promise, background, foreground } = promises.shift();
      new Promise(promise).then(() => {
        browser.tabs.executeScript({
          code: `(${checkCSS})()`,
        }, result => {
          browser.test.assertEq(background, result[0], "Expected background color");
          browser.test.assertEq(foreground, result[1], "Expected foreground color");
          next();
        });
      });
    }

    next();
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["http://mochi.test/"],
    },

    background,

    files: {
      "file1.css": "* { color: pink }",
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
