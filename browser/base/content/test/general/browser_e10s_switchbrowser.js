requestLongerTimeout(2);

const DUMMY_PATH = "browser/browser/base/content/test/general/dummy_page.html";

const gExpectedHistory = {
  index: -1,
  entries: []
};

function get_remote_history(browser) {
  function frame_script() {
    let webNav = docShell.QueryInterface(Components.interfaces.nsIWebNavigation);
    let sessionHistory = webNav.sessionHistory;
    let result = {
      index: sessionHistory.index,
      entries: []
    };

    for (let i = 0; i < sessionHistory.count; i++) {
      let entry = sessionHistory.getEntryAtIndex(i, false);
      result.entries.push({
        uri: entry.URI.spec,
        title: entry.title
      });
    }

    sendAsyncMessage("Test:History", result);
  }

  return new Promise(resolve => {
    browser.messageManager.addMessageListener("Test:History", function listener({data}) {
      browser.messageManager.removeMessageListener("Test:History", listener);
      resolve(data);
    });

    browser.messageManager.loadFrameScript("data:,(" + frame_script.toString() + ")();", true);
  });
}

var check_history = Task.async(function*() {
  let sessionHistory = yield get_remote_history(gBrowser.selectedBrowser);

  let count = sessionHistory.entries.length;
  is(count, gExpectedHistory.entries.length, "Should have the right number of history entries");
  is(sessionHistory.index, gExpectedHistory.index, "Should have the right history index");

  for (let i = 0; i < count; i++) {
    let entry = sessionHistory.entries[i];
    is(entry.uri, gExpectedHistory.entries[i].uri, "Should have the right URI");
    is(entry.title, gExpectedHistory.entries[i].title, "Should have the right title");
  }
});

function clear_history() {
  gExpectedHistory.index = -1;
  gExpectedHistory.entries = [];
}

// Waits for a load and updates the known history
var waitForLoad = Task.async(function*(uri) {
  info("Loading " + uri);
  // Longwinded but this ensures we don't just shortcut to LoadInNewProcess
  gBrowser.selectedBrowser.webNavigation.loadURI(uri, Ci.nsIWebNavigation.LOAD_FLAGS_NONE, null, null, null);

  yield waitForDocLoadComplete();
  gExpectedHistory.index++;
  gExpectedHistory.entries.push({
    uri: gBrowser.currentURI.spec,
    title: gBrowser.contentTitle
  });
});

// Waits for a load and updates the known history
var waitForLoadWithFlags = Task.async(function*(uri, flags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE) {
  info("Loading " + uri + " flags = " + flags);
  gBrowser.selectedBrowser.loadURIWithFlags(uri, flags, null, null, null);

  yield waitForDocLoadComplete();
  if (!(flags & Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_HISTORY)) {

    if (flags & Ci.nsIWebNavigation.LOAD_FLAGS_REPLACE_HISTORY) {
      gExpectedHistory.entries.pop();
    } else {
      gExpectedHistory.index++;
    }

    gExpectedHistory.entries.push({
      uri: gBrowser.currentURI.spec,
      title: gBrowser.contentTitle
    });
  }
});

var back = Task.async(function*() {
  info("Going back");
  gBrowser.goBack();
  yield waitForDocLoadComplete();
  gExpectedHistory.index--;
});

var forward = Task.async(function*() {
  info("Going forward");
  gBrowser.goForward();
  yield waitForDocLoadComplete();
  gExpectedHistory.index++;
});

// Tests that navigating from a page that should be in the remote process and
// a page that should be in the main process works and retains history
add_task(function* test_navigation() {
  let expectedRemote = gMultiProcessBrowser;

  info("1");
  // Create a tab and load a remote page in it
  gBrowser.selectedTab = gBrowser.addTab("about:blank", {skipAnimation: true});
  let {permanentKey} = gBrowser.selectedBrowser;
  yield waitForLoad("http://example.org/" + DUMMY_PATH);
  is(gBrowser.selectedBrowser.isRemoteBrowser, expectedRemote, "Remote attribute should be correct");
  is(gBrowser.selectedBrowser.permanentKey, permanentKey, "browser.permanentKey is still the same");

  info("2");
  // Load another page
  yield waitForLoad("http://example.com/" + DUMMY_PATH);
  is(gBrowser.selectedBrowser.isRemoteBrowser, expectedRemote, "Remote attribute should be correct");
  is(gBrowser.selectedBrowser.permanentKey, permanentKey, "browser.permanentKey is still the same");
  yield check_history();

  info("3");
  // Load a non-remote page
  yield waitForLoad("about:robots");
  is(gBrowser.selectedBrowser.isRemoteBrowser, false, "Remote attribute should be correct");
  is(gBrowser.selectedBrowser.permanentKey, permanentKey, "browser.permanentKey is still the same");
  yield check_history();

  info("4");
  // Load a remote page
  yield waitForLoad("http://example.org/" + DUMMY_PATH);
  is(gBrowser.selectedBrowser.isRemoteBrowser, expectedRemote, "Remote attribute should be correct");
  is(gBrowser.selectedBrowser.permanentKey, permanentKey, "browser.permanentKey is still the same");
  yield check_history();

  info("5");
  yield back();
  is(gBrowser.selectedBrowser.isRemoteBrowser, false, "Remote attribute should be correct");
  is(gBrowser.selectedBrowser.permanentKey, permanentKey, "browser.permanentKey is still the same");
  yield check_history();

  info("6");
  yield back();
  is(gBrowser.selectedBrowser.isRemoteBrowser, expectedRemote, "Remote attribute should be correct");
  is(gBrowser.selectedBrowser.permanentKey, permanentKey, "browser.permanentKey is still the same");
  yield check_history();

  info("7");
  yield forward();
  is(gBrowser.selectedBrowser.isRemoteBrowser, false, "Remote attribute should be correct");
  is(gBrowser.selectedBrowser.permanentKey, permanentKey, "browser.permanentKey is still the same");
  yield check_history();

  info("8");
  yield forward();
  is(gBrowser.selectedBrowser.isRemoteBrowser, expectedRemote, "Remote attribute should be correct");
  is(gBrowser.selectedBrowser.permanentKey, permanentKey, "browser.permanentKey is still the same");
  yield check_history();

  info("9");
  yield back();
  is(gBrowser.selectedBrowser.isRemoteBrowser, false, "Remote attribute should be correct");
  is(gBrowser.selectedBrowser.permanentKey, permanentKey, "browser.permanentKey is still the same");
  yield check_history();

  info("10");
  // Load a new remote page, this should replace the last history entry
  gExpectedHistory.entries.splice(gExpectedHistory.entries.length - 1, 1);
  yield waitForLoad("http://example.com/" + DUMMY_PATH);
  is(gBrowser.selectedBrowser.isRemoteBrowser, expectedRemote, "Remote attribute should be correct");
  is(gBrowser.selectedBrowser.permanentKey, permanentKey, "browser.permanentKey is still the same");
  yield check_history();

  info("11");
  gBrowser.removeCurrentTab();
  clear_history();
});

// Tests that calling gBrowser.loadURI or browser.loadURI to load a page in a
// different process updates the browser synchronously
add_task(function* test_synchronous() {
  let expectedRemote = gMultiProcessBrowser;

  info("1");
  // Create a tab and load a remote page in it
  gBrowser.selectedTab = gBrowser.addTab("about:blank", {skipAnimation: true});
  let {permanentKey} = gBrowser.selectedBrowser;
  yield waitForLoad("http://example.org/" + DUMMY_PATH);
  is(gBrowser.selectedBrowser.isRemoteBrowser, expectedRemote, "Remote attribute should be correct");
  is(gBrowser.selectedBrowser.permanentKey, permanentKey, "browser.permanentKey is still the same");

  info("2");
  // Load another page
  info("Loading about:robots");
  yield BrowserTestUtils.loadURI(gBrowser.selectedBrowser, "about:robots");
  is(gBrowser.selectedBrowser.isRemoteBrowser, false, "Remote attribute should be correct");
  is(gBrowser.selectedBrowser.permanentKey, permanentKey, "browser.permanentKey is still the same");

  yield waitForDocLoadComplete();
  is(gBrowser.selectedBrowser.isRemoteBrowser, false, "Remote attribute should be correct");
  is(gBrowser.selectedBrowser.permanentKey, permanentKey, "browser.permanentKey is still the same");

  info("3");
  // Load the remote page again
  info("Loading http://example.org/" + DUMMY_PATH);
  yield BrowserTestUtils.loadURI(gBrowser.selectedBrowser, "http://example.org/" + DUMMY_PATH);
  is(gBrowser.selectedBrowser.isRemoteBrowser, expectedRemote, "Remote attribute should be correct");
  is(gBrowser.selectedBrowser.permanentKey, permanentKey, "browser.permanentKey is still the same");

  yield waitForDocLoadComplete();
  is(gBrowser.selectedBrowser.isRemoteBrowser, expectedRemote, "Remote attribute should be correct");
  is(gBrowser.selectedBrowser.permanentKey, permanentKey, "browser.permanentKey is still the same");

  info("4");
  gBrowser.removeCurrentTab();
  clear_history();
});

// Tests that load flags are correctly passed through to the child process with
// normal loads
add_task(function* test_loadflags() {
  let expectedRemote = gMultiProcessBrowser;

  info("1");
  // Create a tab and load a remote page in it
  gBrowser.selectedTab = gBrowser.addTab("about:blank", {skipAnimation: true});
  yield waitForLoadWithFlags("about:robots");
  is(gBrowser.selectedBrowser.isRemoteBrowser, false, "Remote attribute should be correct");
  yield check_history();

  info("2");
  // Load a page in the remote process with some custom flags
  yield waitForLoadWithFlags("http://example.com/" + DUMMY_PATH, Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_HISTORY);
  is(gBrowser.selectedBrowser.isRemoteBrowser, expectedRemote, "Remote attribute should be correct");
  yield check_history();

  info("3");
  // Load a non-remote page
  yield waitForLoadWithFlags("about:robots");
  is(gBrowser.selectedBrowser.isRemoteBrowser, false, "Remote attribute should be correct");
  yield check_history();

  info("4");
  // Load another remote page
  yield waitForLoadWithFlags("http://example.org/" + DUMMY_PATH, Ci.nsIWebNavigation.LOAD_FLAGS_REPLACE_HISTORY);
  is(gBrowser.selectedBrowser.isRemoteBrowser, expectedRemote, "Remote attribute should be correct");
  yield check_history();

  is(gExpectedHistory.entries.length, 2, "Should end with the right number of history entries");

  info("5");
  gBrowser.removeCurrentTab();
  clear_history();
});
