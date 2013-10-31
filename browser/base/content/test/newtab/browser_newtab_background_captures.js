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
  let siteName = "newtab_background_captures";
  let url = "http://example.com/#" + siteName;
  let path = imports.PageThumbsStorage.getFilePathForURL(url);
  let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  file.initWithPath(path);
  try {
    file.remove(false);
  }
  catch (err) {}

  // Add a top site.
  yield setLinks(siteName);

  // We need a handle to a hidden, pre-loaded newtab so we can verify that it
  // doesn't allow background captures.  Add a newtab, which triggers creation
  // of a hidden newtab, and then keep calling BrowserNewTabPreloader.newTab
  // until it returns true, meaning that it swapped the passed-in tab's docshell
  // for the hidden newtab docshell.
  let tab = gWindow.gBrowser.addTab("about:blank");
  yield addNewTabPageTab();
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
  isnot(doc.documentElement.getAttribute("allow-background-captures"), "true",
        "Pre-loaded docshell just synchronously swapped, so background " +
        "captures should not be allowed yet");

  // Enable captures.
  Services.prefs.setBoolPref(CAPTURE_PREF, false);

  // Now that the newtab is visible, its allow-background-captures attribute
  // should be set eventually.
  let allowBackgroundCaptures = false;
  let mutationObserver = new MutationObserver(() => {
    mutationObserver.disconnect();
    allowBackgroundCaptures = true;
    is(doc.documentElement.getAttribute("allow-background-captures"), "true",
       "allow-background-captures should now be true");
    info("Waiting for thumbnail to be created after observing " +
         "allow-background-captures change");
  });
  mutationObserver.observe(doc.documentElement, {
    attributes: true,
    attributeFilter: ["allow-background-captures"],
  });

  // And the allow-background-captures change should trigger the thumbnail
  // capture.
  Services.obs.addObserver(function onCreate(subj, topic, data) {
    if (data != url)
      return;
    ok(allowBackgroundCaptures,
       "page-thumbnail:create should be observed after " +
       "allow-background-captures was set");
    Services.obs.removeObserver(onCreate, "page-thumbnail:create");
    // Test finished!
    Services.prefs.setBoolPref(CAPTURE_PREF, originalDisabledState);
    file.remove(false);
    TestRunner.next();
  }, "page-thumbnail:create", false);

  info("Waiting for allow-background-captures change");
  yield true;
}

function wait(ms) {
  setTimeout(TestRunner.next, ms);
}
