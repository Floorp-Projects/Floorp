/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// Like most of the mochitest-browser devtools test,
// on debug test machine, it takes about 50s to run the test.
requestLongerTimeout(4);

loadTestSubscript("head_devtools.js");

const MAIN_PROCESS_PAGE = "about:robots";
const CONTENT_PROCESS_PAGE = "http://example.com/";

async function testOnNavigatedEvent(uri, tab, toolbox, extension) {
  const onNavigated = extension.awaitMessage("network-onNavigated");
  const onSwitched = toolbox.targetList.once("switched-target");
  BrowserTestUtils.loadURI(tab.linkedBrowser, uri);
  await onSwitched;
  const result = await onNavigated;
  is(result, uri, "devtools.network.onNavigated works correctly");
}

/**
 * This test checks whether network works well even target-switching happens.
 */
add_task(async () => {
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    CONTENT_PROCESS_PAGE
  );

  async function devtools_page() {
    browser.devtools.network.onNavigated.addListener(url => {
      browser.test.sendMessage("network-onNavigated", url);
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

  info("Navigate to a page running on main process");
  await testOnNavigatedEvent(MAIN_PROCESS_PAGE, tab, toolbox, extension);

  info("Return to a page running on content process again");
  await testOnNavigatedEvent(CONTENT_PROCESS_PAGE, tab, toolbox, extension);

  await extension.unload();
  await closeToolboxForTab(tab);
  BrowserTestUtils.removeTab(tab);
});
