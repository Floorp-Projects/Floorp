/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

loadTestSubscript("head_devtools.js");

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.allowPrivateBrowsingByDefault", false]],
  });
});

async function testIncognito(incognitoOverride) {
  let privateAllowed = incognitoOverride == "spanning";

  function devtools_page(privateAllowed) {
    if (!privateAllowed) {
      browser.test.fail(
        "Extension devtools_page should not be created on private tabs if not allowed"
      );
    }

    browser.test.sendMessage("devtools_page:loaded");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      devtools_page: "devtools_page.html",
    },
    incognitoOverride,
    files: {
      "devtools_page.html": `
        <!DOCTYPE html>
        <html>
          <head>
            <meta charset="utf-8">
            <script src="devtools_page.js"></script>
          </head>
        </html>
      `,
      "devtools_page.js": `(${devtools_page})(${privateAllowed})`,
    },
  });

  let existingPrivateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  await extension.startup();

  await openToolboxForTab(existingPrivateWindow.gBrowser.selectedTab);

  if (privateAllowed) {
    // Wait the devtools_page to be loaded if it is allowed.
    await extension.awaitMessage("devtools_page:loaded");
  }

  // If the devtools_page is created for a not allowed extension, the devtools page will
  // trigger a test failure, but let's make an explicit assertion otherwise mochitest will
  // complain because there was no assertion in the test.
  ok(
    true,
    `Opened toolbox on an existing private window (extension ${incognitoOverride})`
  );

  let newPrivateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  await openToolboxForTab(newPrivateWindow.gBrowser.selectedTab);

  if (privateAllowed) {
    await extension.awaitMessage("devtools_page:loaded");
  }

  // If the devtools_page is created for a not allowed extension, the devtools page will
  // trigger a test failure.
  ok(
    true,
    `Opened toolbox on a newprivate window (extension ${incognitoOverride})`
  );

  // Close opened toolboxes and private windows.
  await closeToolboxForTab(existingPrivateWindow.gBrowser.selectedTab);
  await closeToolboxForTab(newPrivateWindow.gBrowser.selectedTab);
  await BrowserTestUtils.closeWindow(existingPrivateWindow);
  await BrowserTestUtils.closeWindow(newPrivateWindow);

  await extension.unload();
}

add_task(async function test_devtools_page_not_allowed() {
  await testIncognito();
});

add_task(async function test_devtools_page_allowed() {
  await testIncognito("spanning");
});
