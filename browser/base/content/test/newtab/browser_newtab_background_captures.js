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
  // doesn't allow background captures. Ensure we have a preloaded browser.
  gBrowser._createPreloadBrowser();

  // Wait for the preloaded browser to load.
  yield waitForBrowserLoad(gBrowser._preloadedBrowser);

  // We're now ready to use the preloaded browser.
  BrowserOpenTab();
  let tab = gBrowser.selectedTab;
  let doc = tab.linkedBrowser.contentDocument;

  // Enable captures.
  Services.prefs.setBoolPref(CAPTURE_PREF, false);

  // Showing the preloaded tab should trigger thumbnail capture.
  Services.obs.addObserver(function onCreate(subj, topic, data) {
    if (data != url)
      return;
    Services.obs.removeObserver(onCreate, "page-thumbnail:create");
    ok(true, "thumbnail created after preloaded tab was shown");

    // Test finished!
    Services.prefs.setBoolPref(CAPTURE_PREF, originalDisabledState);
    gBrowser.removeTab(tab);
    file.remove(false);
    TestRunner.next();
  }, "page-thumbnail:create", false);

  info("Waiting for thumbnail capture");
  yield true;
}
