/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function unsupportedSchemes() {
  const testcases = [
    {
      // Link to URL with query string parameters only.
      testUrl: "magnet:?xt=urn:btih:somesha1hash&dn=displayname.txt",
      matchingPatterns: [
        "magnet:*",
        "magnet:?xt=*",
        "magnet:?xt=*txt",
        "magnet:*?xt=*txt",
      ],
      nonmatchingPatterns: [
        // Although <all_urls> matches unsupported schemes in Chromium,
        // we have specified that <all_urls> only matches all supported
        // schemes. To match any scheme, an extension should not set the
        // targetUrlPatterns field - this is checked below in subtest
        // unsupportedSchemeWithoutTargetUrlPatterns.
        "<all_urls>",
        "agnet:*",
        "magne:*",
      ],
    },
    {
      // Link to bookmarklet.
      testUrl: "javascript:-URL",
      matchingPatterns: ["javascript:*", "javascript:*URL", "javascript:-URL"],
      nonmatchingPatterns: [
        "<all_urls>",
        "javascript://-URL",
        "javascript:javascript:-URL",
      ],
    },
    {
      // Link to bookmarklet with comment.
      testUrl: "javascript://-URL",
      matchingPatterns: [
        "javascript:*",
        "javascript://-URL",
        "javascript:*URL",
      ],
      nonmatchingPatterns: ["<all_urls>", "javascript:-URL"],
    },
    {
      // Link to data-URI.
      testUrl: "data:application/foo,bar",
      matchingPatterns: [
        "<all_urls>",
        "data:application/foo,bar",
        "data:*,*",
        "data:*",
      ],
      nonmatchingPatterns: ["data:,bar", "data:application/foo,"],
    },
    {
      // Extension page.
      testUrl: "moz-extension://uuid/manifest.json",
      matchingPatterns: ["moz-extension://*/*"],
      nonmatchingPatterns: [
        "<all_urls>",
        "moz-extension://uuid/not/manifest.json*",
      ],
    },
    {
      // While the scheme is supported, the URL is invalid.
      testUrl: "http://",
      matchingPatterns: [],
      nonmatchingPatterns: ["http://*/*", "<all_urls>"],
    },
  ];

  async function testScript(testcases) {
    let testcase;

    browser.contextMenus.onShown.addListener(({ menuIds, linkUrl }) => {
      browser.test.assertEq(testcase.testUrl, linkUrl, "Expected linkUrl");
      for (let pattern of testcase.matchingPatterns) {
        browser.test.assertTrue(
          menuIds.includes(pattern),
          `Menu item with targetUrlPattern="${pattern}" should be shown at ${testcase.testUrl}`
        );
      }
      for (let pattern of testcase.nonmatchingPatterns) {
        browser.test.assertFalse(
          menuIds.includes(pattern),
          `Menu item with targetUrlPattern="${pattern}" should not be shown at ${testcase.testUrl}`
        );
      }
      testcase = null;
      browser.test.sendMessage("onShown_checked");
    });

    browser.test.onMessage.addListener(async (msg, params) => {
      browser.test.assertEq("setupTest", msg, "Expected message");

      // Save test case in global variable for use in the onShown event.
      testcase = params;
      browser.test.log(`Running test for link with URL: ${testcase.testUrl}`);
      document.getElementById("test_link_element").href = testcase.testUrl;
      await browser.contextMenus.removeAll();
      for (let targetUrlPattern of [
        ...testcase.matchingPatterns,
        ...testcase.nonmatchingPatterns,
      ]) {
        await new Promise(resolve => {
          browser.test.log(`Creating menu with "${targetUrlPattern}"`);
          browser.contextMenus.create(
            {
              id: targetUrlPattern,
              contexts: ["link"],
              title: "Some menu item",
              targetUrlPatterns: [targetUrlPattern],
            },
            resolve
          );
        });
      }
      browser.test.sendMessage("setupTest_ready");
    });
    browser.test.sendMessage("ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["contextMenus"],
    },
    background() {
      browser.tabs.create({ url: "testrunner.html" });
    },
    files: {
      "testrunner.js": `(${testScript})()`,
      "testrunner.html": `
        <!DOCTYPE html><meta charset="utf-8">
        <body>
        <a id="test_link_element">Test link</a>
        <script src="testrunner.js"></script>
        </body>
      `,
    },
  });

  await extension.startup();
  await extension.awaitMessage("ready");
  for (let testcase of testcases) {
    extension.sendMessage("setupTest", testcase);
    await extension.awaitMessage("setupTest_ready");

    await openExtensionContextMenu("#test_link_element");
    await extension.awaitMessage("onShown_checked");
    await closeContextMenu();
  }
  await extension.unload();
});

async function testLinkMenuWithoutTargetUrlPatterns(linkUrl) {
  function background(expectedLinkUrl) {
    let menuId;
    browser.contextMenus.onShown.addListener(({ menuIds, linkUrl }) => {
      browser.test.assertEq(1, menuIds.length, "Expected number of menus");
      browser.test.assertEq(menuId, menuIds[0], "Expected menu ID");
      browser.test.assertEq(expectedLinkUrl, linkUrl, "Expected linkUrl");
      browser.test.sendMessage("done");
    });
    menuId = browser.contextMenus.create(
      {
        contexts: ["link"],
        title: "Test menu item without targetUrlPattern",
      },
      () => {
        browser.tabs.create({ url: "testpage.html" });
      }
    );
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["contextMenus"],
    },
    background: `(${background})("${linkUrl}")`,
    files: {
      "testpage.js": `browser.test.sendMessage("ready")`,
      "testpage.html": `
        <!DOCTYPE html><meta charset="utf-8">
        <a id="test_link_element" href="${linkUrl}">Test link</a>
        <script src="testpage.js"></script>
      `,
    },
  });

  await extension.startup();
  await extension.awaitMessage("ready");
  // Wait for the browser window chrome document to be flushed before
  // trying to trigger the context menu in the newly created tab,
  // to prevent intermittent failures (e.g. Bug 1775558).
  await gBrowser.ownerGlobal.promiseDocumentFlushed(() => {});
  await openExtensionContextMenu("#test_link_element");
  await extension.awaitMessage("done");
  await closeContextMenu();

  await extension.unload();
}

// Tests that a menu item is shown on links with an unsupported scheme if
// targetUrlPatterns is not set.
add_task(async function unsupportedSchemeWithoutPattern() {
  await testLinkMenuWithoutTargetUrlPatterns("unsupported-scheme:data");
});

// Tests that a menu item is shown on links with an invalid http:-URL if
// targetUrlPatterns is not set.
add_task(async function invalidHttpUrlWithoutPattern() {
  await testLinkMenuWithoutTargetUrlPatterns("http://");
});

add_task(async function privileged_are_allowed_to_use_restrictedSchemes() {
  let privilegedExtension = ExtensionTestUtils.loadExtension({
    isPrivileged: true,
    manifest: {
      permissions: ["tabs", "contextMenus", "mozillaAddons"],
    },
    async background() {
      browser.contextMenus.create({
        id: "privileged-extension",
        title: "Privileged Extension",
        contexts: ["page"],
        documentUrlPatterns: ["about:reader*"],
      });

      browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {
        if (
          changeInfo.status === "complete" &&
          tab.url.startsWith("about:reader")
        ) {
          browser.test.sendMessage("readerModeEntered");
        }
      });

      browser.test.onMessage.addListener(async msg => {
        if (msg !== "enterReaderMode") {
          browser.test.fail(`Received unexpected test message: ${msg}`);
          return;
        }

        browser.tabs.toggleReaderMode();
      });
    },
  });

  let nonPrivilegedExtension = ExtensionTestUtils.loadExtension({
    isPrivileged: false,
    manifest: {
      permissions: ["contextMenus", "mozillaAddons"],
    },
    async background() {
      browser.contextMenus.create({
        id: "non-privileged-extension",
        title: "Non Privileged Extension",
        contexts: ["page"],
        documentUrlPatterns: ["about:reader*"],
      });
    },
  });

  const baseUrl = getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "http://example.com"
  );
  const url = `${baseUrl}/readerModeArticle.html`;
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    url,
    true,
    true
  );

  await Promise.all([
    privilegedExtension.startup(),
    nonPrivilegedExtension.startup(),
  ]);

  privilegedExtension.sendMessage("enterReaderMode");
  await privilegedExtension.awaitMessage("readerModeEntered");

  const contextMenu = await openContextMenu("body > h1");

  let item = contextMenu.getElementsByAttribute(
    "label",
    "Privileged Extension"
  );
  is(
    item.length,
    1,
    "Privileged extension's contextMenu item found as expected"
  );

  item = contextMenu.getElementsByAttribute(
    "label",
    "Non Privileged Extension"
  );
  is(
    item.length,
    0,
    "Non privileged extension's contextMenu not found as expected"
  );

  await closeContextMenu();

  BrowserTestUtils.removeTab(tab);

  await Promise.all([
    privilegedExtension.unload(),
    nonPrivilegedExtension.unload(),
  ]);
});
