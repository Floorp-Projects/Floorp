/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Verifies that hidden, pre-loaded newtabs don't allow background captures, and
 * when unhidden, do allow background captures.
 */

const CAPTURE_PREF = "browser.pagethumbnails.capturing_disabled";

function runTests() {
  let imports = {};
  Cu.import("resource://gre/modules/PageThumbs.jsm", imports);
  Cu.import("resource:///modules/BrowserNewTabPreloader.jsm", imports);

  // Disable captures.
  let originalDisabledState = Services.prefs.getBoolPref(CAPTURE_PREF);
  Services.prefs.setBoolPref(CAPTURE_PREF, true);

  // Make sure the thumbnail doesn't exist yet.
  let url = "http://example.com/";
  let path = imports.PageThumbsStorage.getFilePathForURL(url);
  let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  file.initWithPath(path);
  try {
    file.remove(false);
  }
  catch (err) {}

  // Add a top site.
  yield setLinks("-1");

  // We need a handle to a hidden, pre-loaded newtab so we can verify that it
  // doesn't allow background captures.  Add a newtab, which triggers creation
  // of a hidden newtab, and then keep calling BrowserNewTabPreloader.newTab
  // until it returns true, meaning that it swapped the passed-in tab's docshell
  // for the hidden newtab docshell.
  let tab = gWindow.gBrowser.addTab("about:blank");
  yield addNewTabPageTab();

  // When newtab is loaded very quickly (which is what happens in 99% of cases)
  // there is no need to wait so no tests are run. Because each test requires
  // either a pass, fail or todo we run a dummy test here.
  ok(true, "Each test requires at least one pass, fail or todo so here is a pass.");

  let swapWaitCount = 0;
  let swapped = imports.BrowserNewTabPreloader.newTab(tab);
  while (!swapped) {
    if (++swapWaitCount == 10) {
      ok(false, "Timed out waiting for newtab docshell swap.");
      return;
    }
    // Give the hidden newtab some time to finish loading.
    yield wait(2000);
    info("Checking newtab swap " + swapWaitCount);
    swapped = imports.BrowserNewTabPreloader.newTab(tab);
  }

  // The tab's docshell is now the previously hidden newtab docshell.
  let doc = tab.linkedBrowser.contentDocument;

  // Enable captures.
  Services.prefs.setBoolPref(CAPTURE_PREF, false);

  // Showing the preloaded tab should trigger thumbnail capture.
  Services.obs.addObserver(function onCreate(subj, topic, data) {
    if (data != url)
      return;
    Services.obs.removeObserver(onCreate, "page-thumbnail:create");
    // Test finished!
    Services.prefs.setBoolPref(CAPTURE_PREF, originalDisabledState);
    file.remove(false);
    TestRunner.next();
  }, "page-thumbnail:create", false);

  info("Waiting for thumbnail capture");
  yield true;
}

function wait(ms) {
  setTimeout(TestRunner.next, ms);
}
