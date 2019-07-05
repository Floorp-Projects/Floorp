/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function testExecuteScript() {
  let { MessageChannel } = ChromeUtils.import(
    "resource://gre/modules/MessageChannel.jsm"
  );

  // When the first extension is started, ProxyMessenger.init adds MessageChannel
  // listeners for Services.mm and Services.ppmm, and they are never unsubscribed.
  // We have to exclude them after the extension has been unloaded to get an accurate
  // test.
  function getMessageManagersSize(messageManagers) {
    return Array.from(messageManagers).filter(([mm]) => {
      return ![Services.mm, Services.ppmm].includes(mm);
    }).length;
  }

  let messageManagersSize = getMessageManagersSize(
    MessageChannel.messageManagers
  );
  let responseManagersSize = MessageChannel.responseManagers.size;

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://mochi.test:8888/",
    true
  );

  async function background() {
    let tasks = [
      {
        background: "rgba(0, 0, 0, 0)",
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
      {
        background: "rgb(43, 43, 43)",
        foreground: "rgb(0, 113, 4)",
        promise: () => {
          return browser.tabs
            .insertCSS({
              code: "* { background: rgb(100, 100, 100) !important }",
              cssOrigin: "author",
            })
            .then(r =>
              browser.tabs.insertCSS({
                code: "* { background: rgb(43, 43, 43) !important }",
                cssOrigin: "author",
              })
            );
        },
      },
      {
        background: "rgb(100, 100, 100)",
        foreground: "rgb(0, 113, 4)",
        promise: () => {
          // User has higher importance
          return browser.tabs
            .insertCSS({
              code: "* { background: rgb(100, 100, 100) !important }",
              cssOrigin: "user",
            })
            .then(r =>
              browser.tabs.insertCSS({
                code: "* { background: rgb(44, 44, 44) !important }",
                cssOrigin: "author",
              })
            );
        },
      },
    ];

    function checkCSS() {
      let computedStyle = window.getComputedStyle(document.body);
      return [computedStyle.backgroundColor, computedStyle.color];
    }

    try {
      for (let { promise, background, foreground } of tasks) {
        let result = await promise();

        browser.test.assertEq(undefined, result, "Expected callback result");

        [result] = await browser.tabs.executeScript({
          code: `(${checkCSS})()`,
        });

        browser.test.assertEq(
          background,
          result[0],
          "Expected background color"
        );
        browser.test.assertEq(
          foreground,
          result[1],
          "Expected foreground color"
        );
      }

      browser.test.notifyPass("insertCSS");
    } catch (e) {
      browser.test.fail(`Error: ${e} :: ${e.stack}`);
      browser.test.notifyFail("insertCSS");
    }
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["http://mochi.test/"],
    },

    background,

    files: {
      "file2.css": "* { color: rgb(0, 113, 4) }",
    },
  });

  await extension.startup();

  await extension.awaitFinish("insertCSS");

  await extension.unload();

  BrowserTestUtils.removeTab(tab);

  // Make sure that we're not holding on to references to closed message
  // managers.
  is(
    getMessageManagersSize(MessageChannel.messageManagers),
    messageManagersSize,
    "Message manager count"
  );
  is(
    MessageChannel.responseManagers.size,
    responseManagersSize,
    "Response manager count"
  );
  is(MessageChannel.pendingResponses.size, 0, "Pending response count");
});

add_task(async function testInsertCSS_cleanup() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://mochi.test:8888/",
    true
  );

  async function background() {
    await browser.tabs.insertCSS({ code: "* { background: rgb(42, 42, 42) }" });
    await browser.tabs.insertCSS({ file: "customize_fg_color.css" });

    browser.test.notifyPass("insertCSS");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["http://mochi.test/"],
    },
    background,
    files: {
      "customize_fg_color.css": `* { color: rgb(255, 0, 0) }`,
    },
  });

  await extension.startup();

  await extension.awaitFinish("insertCSS");

  const getTabContentComputedStyle = async () => {
    let computedStyle = content.getComputedStyle(content.document.body);
    return [computedStyle.backgroundColor, computedStyle.color];
  };

  const appliedStyles = await ContentTask.spawn(
    tab.linkedBrowser,
    null,
    getTabContentComputedStyle
  );

  is(
    appliedStyles[0],
    "rgb(42, 42, 42)",
    "The injected CSS code has been applied as expected"
  );
  is(
    appliedStyles[1],
    "rgb(255, 0, 0)",
    "The injected CSS file has been applied as expected"
  );

  await extension.unload();

  const unloadedStyles = await ContentTask.spawn(
    tab.linkedBrowser,
    null,
    getTabContentComputedStyle
  );

  is(
    unloadedStyles[0],
    "rgba(0, 0, 0, 0)",
    "The injected CSS code has been removed as expected"
  );
  is(
    unloadedStyles[1],
    "rgb(0, 0, 0)",
    "The injected CSS file has been removed as expected"
  );

  BrowserTestUtils.removeTab(tab);
});
