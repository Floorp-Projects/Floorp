/* eslint-env mozilla/frame-script */

requestLongerTimeout(2);

const DUMMY_PATH = "browser/browser/base/content/test/general/dummy_page.html";

const gExpectedHistory = {
  index: -1,
  entries: [],
};

function get_remote_history(browser) {
  return SpecialPowers.spawn(browser, [], () => {
    let webNav = content.docShell.QueryInterface(Ci.nsIWebNavigation);
    let sessionHistory = webNav.sessionHistory;
    let result = {
      index: sessionHistory.index,
      entries: [],
    };

    for (let i = 0; i < sessionHistory.count; i++) {
      let entry = sessionHistory.legacySHistory.getEntryAtIndex(i);
      result.entries.push({
        uri: entry.URI.spec,
        title: entry.title,
      });
    }

    return result;
  });
}

var check_history = async function() {
  let sessionHistory = await get_remote_history(gBrowser.selectedBrowser);

  let count = sessionHistory.entries.length;
  is(
    count,
    gExpectedHistory.entries.length,
    "Should have the right number of history entries"
  );
  is(
    sessionHistory.index,
    gExpectedHistory.index,
    "Should have the right history index"
  );

  for (let i = 0; i < count; i++) {
    let entry = sessionHistory.entries[i];
    info("Checking History Entry: " + entry.uri);
    is(entry.uri, gExpectedHistory.entries[i].uri, "Should have the right URI");
    is(
      entry.title,
      gExpectedHistory.entries[i].title,
      "Should have the right title"
    );
  }
};

function clear_history() {
  gExpectedHistory.index = -1;
  gExpectedHistory.entries = [];
}

// Waits for a load and updates the known history
var waitForLoad = async function(uri) {
  info("Loading " + uri);
  // Longwinded but this ensures we don't just shortcut to LoadInNewProcess
  let loadURIOptions = {
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  };
  gBrowser.selectedBrowser.webNavigation.loadURI(uri, loadURIOptions);

  await BrowserTestUtils.browserStopped(gBrowser);

  // Some of the documents we're using in this test use Fluent,
  // and they may finish localization later.
  // To prevent this test from being intermittent, we'll
  // wait for the `document.l10n.ready` promise to resolve.
  if (
    gBrowser.selectedBrowser.contentWindow &&
    gBrowser.selectedBrowser.contentWindow.document.l10n
  ) {
    await gBrowser.selectedBrowser.contentWindow.document.l10n.ready;
  }
  gExpectedHistory.index++;
  gExpectedHistory.entries.push({
    uri: gBrowser.currentURI.spec,
    title: gBrowser.contentTitle,
  });
};

// Waits for a load and updates the known history
var waitForLoadWithFlags = async function(
  uri,
  flags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE
) {
  info("Loading " + uri + " flags = " + flags);
  gBrowser.selectedBrowser.loadURI(uri, {
    flags,
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });

  await BrowserTestUtils.browserStopped(gBrowser);
  if (!(flags & Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_HISTORY)) {
    if (flags & Ci.nsIWebNavigation.LOAD_FLAGS_REPLACE_HISTORY) {
      gExpectedHistory.entries.pop();
    } else {
      gExpectedHistory.index++;
    }

    gExpectedHistory.entries.push({
      uri: gBrowser.currentURI.spec,
      title: gBrowser.contentTitle,
    });
  }
};

var back = async function() {
  info("Going back");
  gBrowser.goBack();
  await BrowserTestUtils.browserStopped(gBrowser);
  gExpectedHistory.index--;
};

var forward = async function() {
  info("Going forward");
  gBrowser.goForward();
  await BrowserTestUtils.browserStopped(gBrowser);
  gExpectedHistory.index++;
};

// Tests that navigating from a page that should be in the remote process and
// a page that should be in the main process works and retains history
add_task(async function test_navigation() {
  let expectedRemote = gMultiProcessBrowser;

  info("1");
  // Create a tab and load a remote page in it
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
    skipAnimation: true,
  });
  let { permanentKey } = gBrowser.selectedBrowser;
  await waitForLoad("http://example.org/" + DUMMY_PATH);
  is(
    gBrowser.selectedBrowser.isRemoteBrowser,
    expectedRemote,
    "Remote attribute should be correct"
  );
  is(
    gBrowser.selectedBrowser.permanentKey,
    permanentKey,
    "browser.permanentKey is still the same"
  );

  info("2");
  // Load another page
  await waitForLoad("http://example.com/" + DUMMY_PATH);
  is(
    gBrowser.selectedBrowser.isRemoteBrowser,
    expectedRemote,
    "Remote attribute should be correct"
  );
  is(
    gBrowser.selectedBrowser.permanentKey,
    permanentKey,
    "browser.permanentKey is still the same"
  );
  await check_history();

  info("3");
  // Load a non-remote page
  await waitForLoad("about:robots");
  await TestUtils.waitForCondition(
    () => gBrowser.selectedBrowser.contentTitle != "about:robots",
    "Waiting for about:robots title to update"
  );
  is(
    gBrowser.selectedBrowser.isRemoteBrowser,
    false,
    "Remote attribute should be correct"
  );
  is(
    gBrowser.selectedBrowser.permanentKey,
    permanentKey,
    "browser.permanentKey is still the same"
  );
  await check_history();

  info("4");
  // Load a remote page
  await waitForLoad("http://example.org/" + DUMMY_PATH);
  is(
    gBrowser.selectedBrowser.isRemoteBrowser,
    expectedRemote,
    "Remote attribute should be correct"
  );
  is(
    gBrowser.selectedBrowser.permanentKey,
    permanentKey,
    "browser.permanentKey is still the same"
  );
  await check_history();

  info("5");
  await back();
  is(
    gBrowser.selectedBrowser.isRemoteBrowser,
    false,
    "Remote attribute should be correct"
  );
  is(
    gBrowser.selectedBrowser.permanentKey,
    permanentKey,
    "browser.permanentKey is still the same"
  );
  await check_history();

  info("6");
  await back();
  is(
    gBrowser.selectedBrowser.isRemoteBrowser,
    expectedRemote,
    "Remote attribute should be correct"
  );
  is(
    gBrowser.selectedBrowser.permanentKey,
    permanentKey,
    "browser.permanentKey is still the same"
  );
  await check_history();

  info("7");
  await forward();
  is(
    gBrowser.selectedBrowser.isRemoteBrowser,
    false,
    "Remote attribute should be correct"
  );
  is(
    gBrowser.selectedBrowser.permanentKey,
    permanentKey,
    "browser.permanentKey is still the same"
  );
  await check_history();

  info("8");
  await forward();
  is(
    gBrowser.selectedBrowser.isRemoteBrowser,
    expectedRemote,
    "Remote attribute should be correct"
  );
  is(
    gBrowser.selectedBrowser.permanentKey,
    permanentKey,
    "browser.permanentKey is still the same"
  );
  await check_history();

  info("9");
  await back();
  is(
    gBrowser.selectedBrowser.isRemoteBrowser,
    false,
    "Remote attribute should be correct"
  );
  is(
    gBrowser.selectedBrowser.permanentKey,
    permanentKey,
    "browser.permanentKey is still the same"
  );
  await check_history();

  info("10");
  // Load a new remote page, this should replace the last history entry
  gExpectedHistory.entries.splice(gExpectedHistory.entries.length - 1, 1);
  await waitForLoad("http://example.com/" + DUMMY_PATH);
  is(
    gBrowser.selectedBrowser.isRemoteBrowser,
    expectedRemote,
    "Remote attribute should be correct"
  );
  is(
    gBrowser.selectedBrowser.permanentKey,
    permanentKey,
    "browser.permanentKey is still the same"
  );
  await check_history();

  info("11");
  gBrowser.removeCurrentTab();
  clear_history();
});

// Tests that calling gBrowser.loadURI or browser.loadURI to load a page in a
// different process updates the browser synchronously
add_task(async function test_synchronous() {
  let expectedRemote = gMultiProcessBrowser;

  info("1");
  // Create a tab and load a remote page in it
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
    skipAnimation: true,
  });
  let { permanentKey } = gBrowser.selectedBrowser;
  await waitForLoad("http://example.org/" + DUMMY_PATH);
  is(
    gBrowser.selectedBrowser.isRemoteBrowser,
    expectedRemote,
    "Remote attribute should be correct"
  );
  is(
    gBrowser.selectedBrowser.permanentKey,
    permanentKey,
    "browser.permanentKey is still the same"
  );

  info("2");
  // Load another page
  info("Loading about:robots");
  await BrowserTestUtils.loadURI(gBrowser.selectedBrowser, "about:robots");
  is(
    gBrowser.selectedBrowser.isRemoteBrowser,
    false,
    "Remote attribute should be correct"
  );
  is(
    gBrowser.selectedBrowser.permanentKey,
    permanentKey,
    "browser.permanentKey is still the same"
  );

  await BrowserTestUtils.browserStopped(gBrowser);
  is(
    gBrowser.selectedBrowser.isRemoteBrowser,
    false,
    "Remote attribute should be correct"
  );
  is(
    gBrowser.selectedBrowser.permanentKey,
    permanentKey,
    "browser.permanentKey is still the same"
  );

  info("3");
  // Load the remote page again
  info("Loading http://example.org/" + DUMMY_PATH);
  await BrowserTestUtils.loadURI(
    gBrowser.selectedBrowser,
    "http://example.org/" + DUMMY_PATH
  );
  is(
    gBrowser.selectedBrowser.isRemoteBrowser,
    expectedRemote,
    "Remote attribute should be correct"
  );
  is(
    gBrowser.selectedBrowser.permanentKey,
    permanentKey,
    "browser.permanentKey is still the same"
  );

  await BrowserTestUtils.browserStopped(gBrowser);
  is(
    gBrowser.selectedBrowser.isRemoteBrowser,
    expectedRemote,
    "Remote attribute should be correct"
  );
  is(
    gBrowser.selectedBrowser.permanentKey,
    permanentKey,
    "browser.permanentKey is still the same"
  );

  info("4");
  gBrowser.removeCurrentTab();
  clear_history();
});

// Tests that load flags are correctly passed through to the child process with
// normal loads
add_task(async function test_loadflags() {
  let expectedRemote = gMultiProcessBrowser;

  info("1");
  // Create a tab and load a remote page in it
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
    skipAnimation: true,
  });
  await waitForLoadWithFlags("about:robots");
  await TestUtils.waitForCondition(
    () => gBrowser.selectedBrowser.contentTitle != "about:robots",
    "Waiting for about:robots title to update"
  );
  is(
    gBrowser.selectedBrowser.isRemoteBrowser,
    false,
    "Remote attribute should be correct"
  );
  await check_history();

  info("2");
  // Load a page in the remote process with some custom flags
  await waitForLoadWithFlags(
    "http://example.com/" + DUMMY_PATH,
    Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_HISTORY
  );
  is(
    gBrowser.selectedBrowser.isRemoteBrowser,
    expectedRemote,
    "Remote attribute should be correct"
  );
  await check_history();

  info("3");
  // Load a non-remote page
  await waitForLoadWithFlags("about:robots");
  await TestUtils.waitForCondition(
    () => gBrowser.selectedBrowser.contentTitle != "about:robots",
    "Waiting for about:robots title to update"
  );
  is(
    gBrowser.selectedBrowser.isRemoteBrowser,
    false,
    "Remote attribute should be correct"
  );
  await check_history();

  info("4");
  // Load another remote page
  await waitForLoadWithFlags(
    "http://example.org/" + DUMMY_PATH,
    Ci.nsIWebNavigation.LOAD_FLAGS_REPLACE_HISTORY
  );
  is(
    gBrowser.selectedBrowser.isRemoteBrowser,
    expectedRemote,
    "Remote attribute should be correct"
  );
  await check_history();

  info("5");
  // Load another remote page from a different origin
  await waitForLoadWithFlags(
    "http://example.com/" + DUMMY_PATH,
    Ci.nsIWebNavigation.LOAD_FLAGS_REPLACE_HISTORY
  );
  is(
    gBrowser.selectedBrowser.isRemoteBrowser,
    expectedRemote,
    "Remote attribute should be correct"
  );
  await check_history();

  is(
    gExpectedHistory.entries.length,
    2,
    "Should end with the right number of history entries"
  );

  info("6");
  gBrowser.removeCurrentTab();
  clear_history();
});
