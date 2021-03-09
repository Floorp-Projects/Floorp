/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// Like most of the mochitest-browser devtools test,
// on debug test machine, it takes about 50s to run the test.
requestLongerTimeout(4);

loadTestSubscript("head_devtools.js");

const MAIN_PROCESS_PAGE = "about:robots";
const CONTENT_PROCESS_PAGE =
  "data:text/html,<title>content process page</title>";
const CONTENT_PROCESS_PAGE2 = "http://example.com/";

async function assertInspectedWindow(extension, tab) {
  const onReloaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  extension.sendMessage("inspectedWindow-reload-request");
  await onReloaded;
  ok(true, "inspectedWindow works correctly");
}

async function getCurrentTabId(extension) {
  extension.sendMessage("inspectedWindow-tabId-request");
  return extension.awaitMessage("inspectedWindow-tabId-response");
}

async function navigateTo(uri, tab, toolbox, extension) {
  const originalTabId = await getCurrentTabId(extension);

  const onSwitched = toolbox.targetList.once("switched-target");
  BrowserTestUtils.loadURI(tab.linkedBrowser, uri);
  await onSwitched;

  const currentTabId = await getCurrentTabId(extension);
  is(
    originalTabId,
    currentTabId,
    "inspectWindow.tabId is not changed even when navigating to a page running on another process."
  );
}

/**
 * This test checks whether inspectedWindow works well even target-switching happens.
 */
add_task(async () => {
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    CONTENT_PROCESS_PAGE
  );

  async function devtools_page() {
    browser.test.onMessage.addListener(async message => {
      if (message === "inspectedWindow-reload-request") {
        browser.devtools.inspectedWindow.reload();
      } else if (message === "inspectedWindow-tabId-request") {
        browser.test.sendMessage(
          "inspectedWindow-tabId-response",
          browser.devtools.inspectedWindow.tabId
        );
      } else {
        browser.test.fail(`Unexpected test message received: ${message}`);
      }
    });

    browser.test.sendMessage("devtools-page-loaded");
  }

  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      devtools_page: "devtools_page.html",
    },
    files: {
      "devtools_page.html": `<!DOCTYPE html>
        <html>
          <head>
            <meta charset="utf-8">
          </head>
          <body>
            <script src="devtools_page.js"></script>
          </body>
        </html>`,
      "devtools_page.js": devtools_page,
    },
  });
  await extension.startup();

  info("Open the developer toolbox");
  const toolbox = await openToolboxForTab(tab);

  info("Wait the devtools page load");
  await extension.awaitMessage("devtools-page-loaded");

  info("Check whether inspectedWindow works on content process page");
  await assertInspectedWindow(extension, tab);

  info("Navigate to a page running on main process");
  await navigateTo(MAIN_PROCESS_PAGE, tab, toolbox, extension);

  info("Check whether inspectedWindow works on main process page");
  await assertInspectedWindow(extension, tab);

  info("Return to a page running on content process again");
  await navigateTo(CONTENT_PROCESS_PAGE, tab, toolbox, extension);

  info("Check whether inspectedWindow works again");
  await assertInspectedWindow(extension, tab);

  // Navigate to an url that should switch to another target
  // when fission is enabled.
  if (SpecialPowers.useRemoteSubframes) {
    info("Navigate to another page running on content process");
    await navigateTo(CONTENT_PROCESS_PAGE2, tab, toolbox, extension);

    info("Check whether inspectedWindow works again");
    await assertInspectedWindow(extension, tab);
  }

  await extension.unload();
  await closeToolboxForTab(tab);
  BrowserTestUtils.removeTab(tab);
});
