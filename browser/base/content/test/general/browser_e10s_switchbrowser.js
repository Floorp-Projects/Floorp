const DUMMY_PATH = "browser/browser/base/content/test/general/dummy_page.html";

const gExpectedHistory = {
  index: -1,
  entries: []
};

function check_history() {
  let webNav = gBrowser.webNavigation;
  let sessionHistory = webNav.sessionHistory;

  let count = sessionHistory.count;
  is(count, gExpectedHistory.entries.length, "Should have the right number of history entries");
  is(sessionHistory.index, gExpectedHistory.index, "Should have the right history index");

  for (let i = 0; i < count; i++) {
    let entry = sessionHistory.getEntryAtIndex(i, false);
    is(entry.URI.spec, gExpectedHistory.entries[i].uri, "Should have the right URI");
    is(entry.title, gExpectedHistory.entries[i].title, "Should have the right title");
  }
}

// Waits for a load and updates the known history
let waitForLoad = Task.async(function*(uri) {
  info("Loading " + uri);
  gBrowser.loadURI(uri);

  yield waitForDocLoadComplete();
  gExpectedHistory.index++;
  gExpectedHistory.entries.push({
    uri: gBrowser.currentURI.spec,
    title: gBrowser.contentTitle
  });
});

let back = Task.async(function*() {
  info("Going back");
  gBrowser.goBack();
  yield waitForDocLoadComplete();
  gExpectedHistory.index--;
});

let forward = Task.async(function*() {
  info("Going forward");
  gBrowser.goForward();
  yield waitForDocLoadComplete();
  gExpectedHistory.index++;
});

// Tests that navigating from a page that should be in the remote process and
// a page that should be in the main process works and retains history
add_task(function*() {
  SimpleTest.requestCompleteLog();

  let remoting = Services.prefs.getBoolPref("browser.tabs.remote.autostart");
  let expectedRemote = remoting ? "true" : "";

  info("1");
  // Create a tab and load a remote page in it
  gBrowser.selectedTab = gBrowser.addTab("about:blank", {skipAnimation: true});
  let {permanentKey} = gBrowser.selectedBrowser;
  yield waitForLoad("http://example.org/" + DUMMY_PATH);
  is(gBrowser.selectedTab.getAttribute("remote"), expectedRemote, "Remote attribute should be correct");
  is(gBrowser.selectedBrowser.permanentKey, permanentKey, "browser.permanentKey is still the same");

  info("2");
  // Load another page
  yield waitForLoad("http://example.com/" + DUMMY_PATH);
  is(gBrowser.selectedTab.getAttribute("remote"), expectedRemote, "Remote attribute should be correct");
  is(gBrowser.selectedBrowser.permanentKey, permanentKey, "browser.permanentKey is still the same");
  check_history();

  info("3");
  // Load a non-remote page
  yield waitForLoad("about:robots");
  is(gBrowser.selectedTab.getAttribute("remote"), "", "Remote attribute should be correct");
  is(gBrowser.selectedBrowser.permanentKey, permanentKey, "browser.permanentKey is still the same");
  check_history();

  info("4");
  // Load a remote page
  yield waitForLoad("http://example.org/" + DUMMY_PATH);
  is(gBrowser.selectedTab.getAttribute("remote"), expectedRemote, "Remote attribute should be correct");
  is(gBrowser.selectedBrowser.permanentKey, permanentKey, "browser.permanentKey is still the same");
  check_history();

  info("5");
  yield back();
  is(gBrowser.selectedTab.getAttribute("remote"), "", "Remote attribute should be correct");
  is(gBrowser.selectedBrowser.permanentKey, permanentKey, "browser.permanentKey is still the same");
  check_history();

  info("6");
  yield back();
  is(gBrowser.selectedTab.getAttribute("remote"), expectedRemote, "Remote attribute should be correct");
  is(gBrowser.selectedBrowser.permanentKey, permanentKey, "browser.permanentKey is still the same");
  check_history();

  info("7");
  yield forward();
  is(gBrowser.selectedTab.getAttribute("remote"), "", "Remote attribute should be correct");
  is(gBrowser.selectedBrowser.permanentKey, permanentKey, "browser.permanentKey is still the same");
  check_history();

  info("8");
  yield forward();
  is(gBrowser.selectedTab.getAttribute("remote"), expectedRemote, "Remote attribute should be correct");
  is(gBrowser.selectedBrowser.permanentKey, permanentKey, "browser.permanentKey is still the same");
  check_history();

  info("9");
  gBrowser.removeCurrentTab();
});
