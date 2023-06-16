/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

AddonTestUtils.initMochitest(this);

add_task(async function testExecuteScript() {
  let { MessageChannel } = ChromeUtils.importESModule(
    "resource://testing-common/MessageChannel.sys.mjs"
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

  const appliedStyles = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
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

  const unloadedStyles = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
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

// Verify that no removeSheet/removeSheetUsingURIString errors are logged while
// cleaning up css injected using a manifest content script or tabs.insertCSS.
add_task(async function test_csscode_cleanup_on_closed_windows() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["http://example.com/*"],
      content_scripts: [
        {
          matches: ["http://example.com/*"],
          css: ["content.css"],
          run_at: "document_start",
        },
      ],
    },

    files: {
      "content.css": "body { min-width: 15px; }",
    },

    async background() {
      browser.runtime.onConnect.addListener(port => {
        port.onDisconnect.addListener(() => {
          browser.test.sendMessage("port-disconnected");
        });
        browser.test.sendMessage("port-connected");
      });

      await browser.tabs.create({
        url: "http://example.com/",
        active: true,
      });

      await browser.tabs.insertCSS({
        code: "body { max-width: 50px; }",
      });

      // Create a port, as a way to detect when the content script has been
      // destroyed and any removeSheet error already collected (if it has been
      // raised during the content scripts cleanup).
      await browser.tabs.executeScript({
        code: `(${function () {
          const { maxWidth, minWidth } = window.getComputedStyle(document.body);
          browser.test.sendMessage("body-styles", { maxWidth, minWidth });
          browser.runtime.connect();
        }})();`,
      });
    },
  });

  await extension.startup();

  let { messages } = await AddonTestUtils.promiseConsoleOutput(async () => {
    info("Waiting for content scripts to be injected");

    const { maxWidth, minWidth } = await extension.awaitMessage("body-styles");
    is(maxWidth, "50px", "tabs.insertCSS applied");
    is(minWidth, "15px", "manifest.content_scripts CSS applied");

    await extension.awaitMessage("port-connected");
    const tab = gBrowser.selectedTab;

    info("Close tab and wait for content script port to be disconnected");
    BrowserTestUtils.removeTab(tab);
    await extension.awaitMessage("port-disconnected");
  });

  // Look for nsIDOMWindowUtils.removeSheet and
  // nsIDOMWindowUtils.removeSheetUsingURIString errors.
  AddonTestUtils.checkMessages(
    messages,
    {
      forbidden: [{ errorMessage: /nsIDOMWindowUtils.removeSheet/ }],
    },
    "Expect no remoteSheet errors"
  );

  await extension.unload();
});
