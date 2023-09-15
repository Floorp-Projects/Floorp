/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests for waiting for beforeunload before replacing a session.
 */

const { PromptTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromptTestUtils.sys.mjs"
);

// The first two urls are intentionally different domains to force pages
// to load in different tabs.
const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);
const TEST_URL = "https://example.com/";

const BUILDER_URL = "https://example.com/document-builder.sjs?html=";
const PAGE_MARKUP = `
<html>
<head>
  <script>
    window.onbeforeunload = function() {
      return true;
    };
  </script>
</head>
<body>TEST PAGE</body>
</html>
`;
const TEST_URL2 = BUILDER_URL + encodeURI(PAGE_MARKUP);

let win;
let nonBeforeUnloadTab;
let beforeUnloadTab;

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.require_user_interaction_for_beforeunload", false]],
  });

  // Run tests in a new window to avoid affecting the main test window.
  win = await BrowserTestUtils.openNewBrowserWindow();

  BrowserTestUtils.startLoadingURIString(
    win.gBrowser.selectedBrowser,
    TEST_URL
  );
  await BrowserTestUtils.browserLoaded(
    win.gBrowser.selectedBrowser,
    false,
    TEST_URL
  );
  nonBeforeUnloadTab = win.gBrowser.selectedTab;
  beforeUnloadTab = await BrowserTestUtils.openNewForegroundTab(
    win.gBrowser,
    TEST_URL2
  );

  registerCleanupFunction(async () => {
    await BrowserTestUtils.closeWindow(win);
  });
});

add_task(async function test_runBeforeUnloadForTabs() {
  let unloadDialogPromise = PromptTestUtils.handleNextPrompt(
    win,
    {
      modalType: Ci.nsIPrompt.MODAL_TYPE_CONTENT,
      promptType: "confirmEx",
    },
    // Click the cancel button.
    { buttonNumClick: 1 }
  );

  let unloadBlocked = await win.gBrowser.runBeforeUnloadForTabs(
    win.gBrowser.tabs
  );

  await unloadDialogPromise;

  Assert.ok(unloadBlocked, "Should have reported the unload was blocked");
  Assert.equal(win.gBrowser.tabs.length, 2, "Should have left all tabs open");

  unloadDialogPromise = PromptTestUtils.handleNextPrompt(
    win,
    {
      modalType: Ci.nsIPrompt.MODAL_TYPE_CONTENT,
      promptType: "confirmEx",
    },
    // Click the ok button.
    { buttonNumClick: 0 }
  );

  unloadBlocked = await win.gBrowser.runBeforeUnloadForTabs(win.gBrowser.tabs);

  await unloadDialogPromise;

  Assert.ok(!unloadBlocked, "Should have reported the unload was not blocked");
  Assert.equal(win.gBrowser.tabs.length, 2, "Should have left all tabs open");
});

add_task(async function test_skipPermitUnload() {
  let closePromise = BrowserTestUtils.waitForTabClosing(beforeUnloadTab);

  await win.gBrowser.removeAllTabsBut(nonBeforeUnloadTab, {
    animate: false,
    skipPermitUnload: true,
  });

  await closePromise;

  Assert.equal(win.gBrowser.tabs.length, 1, "Should have left one tab open");
});
