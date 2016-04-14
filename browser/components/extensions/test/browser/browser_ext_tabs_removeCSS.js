/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* testExecuteScript() {
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/", true);

  function background() {
    let promises = [
      // Insert CSS file.
      {
        background: "transparent",
        foreground: "rgb(0, 113, 4)",
        promise: () => {
          return browser.tabs.insertCSS({
            file: "file2.css",
          });
        },
      },
      // Insert CSS code.
      {
        background: "rgb(42, 42, 42)",
        foreground: "rgb(0, 113, 4)",
        promise: () => {
          return browser.tabs.insertCSS({
            code: "* { background: rgb(42, 42, 42) }",
          });
        },
      },
      // Remove CSS code again.
      {
        background: "transparent",
        foreground: "rgb(0, 113, 4)",
        promise: () => {
          return browser.tabs.removeCSS({
            code: "* { background: rgb(42, 42, 42) }",
          });
        },
      },
      // Remove CSS file again.
      {
        background: "transparent",
        foreground: "rgb(0, 0, 0)",
        promise: () => {
          return browser.tabs.removeCSS({
            file: "file2.css",
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
      }).then(result => {
        browser.test.assertEq(background, result[0], "Expected background color");
        browser.test.assertEq(foreground, result[1], "Expected foreground color");
        return next();
      });
    }

    next().then(() => {
      browser.test.notifyPass("removeCSS");
    }).catch(e => {
      browser.test.fail(`Error: ${e} :: ${e.stack}`);
      browser.test.notifyFailure("removeCSS");
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

  yield extension.awaitFinish("removeCSS");

  yield extension.unload();

  yield BrowserTestUtils.removeTab(tab);
});
