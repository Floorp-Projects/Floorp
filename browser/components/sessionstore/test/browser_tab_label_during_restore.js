/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that we don't do unnecessary tab label changes while restoring a tab.
 */

add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    "set": [
      ["browser.sessionstore.restore_on_demand", true],
      ["browser.sessionstore.restore_tabs_lazily", true],
    ]
  });

  const BACKUP_STATE = SessionStore.getBrowserState();
  const TEST_URL = "http://www.example.com/";
  const ABOUT_ROBOTS_URI = "about:robots";
  const ABOUT_ROBOTS_TITLE = "Gort! Klaatu barada nikto!";

  function observeLabelChanges(tab, expectedLabels) {
    let seenLabels = [tab.label];
    function TabAttrModifiedListener(event) {
      if (event.detail.changed.some(attr => attr == "label")) {
        seenLabels.push(tab.label);
      }
    }
    tab.addEventListener("TabAttrModified", TabAttrModifiedListener);
    return () => {
      tab.removeEventListener("TabAttrModified", TabAttrModifiedListener);
      is(JSON.stringify(seenLabels), JSON.stringify(expectedLabels || []), "observed tab label changes");
    }
  }

  info("setting test browser state");
  let browserLoadedPromise = BrowserTestUtils.firstBrowserLoaded(window, false);
  await promiseBrowserState({
    windows: [{
      tabs: [
        { entries: [{ url: TEST_URL }] },
        { entries: [{ url: ABOUT_ROBOTS_URI }] },
        { entries: [{ url: TEST_URL }] },
      ]
    }]
  });
  let [firstTab, secondTab, thirdTab] = gBrowser.tabs;
  is(gBrowser.selectedTab, firstTab, "first tab is selected");

  await browserLoadedPromise;
  const CONTENT_TITLE = firstTab.linkedBrowser.contentTitle;
  is(firstTab.linkedBrowser.currentURI.spec, TEST_URL, "correct URL loaded in first tab");
  is(typeof CONTENT_TITLE, "string", "content title is a string");
  isnot(CONTENT_TITLE.length, 0, "content title isn't empty");
  isnot(CONTENT_TITLE, TEST_URL, "content title is different from the URL");
  is(firstTab.label, CONTENT_TITLE, "first tab displays content title");
  ok(document.title.startsWith(CONTENT_TITLE), "title bar displays content title");
  ok(secondTab.hasAttribute("pending"), "second tab is pending");
  ok(thirdTab.hasAttribute("pending"), "third tab is pending");

  info("selecting the second tab");
  // The fix for bug 1364127 caused about: pages' initial tab titles to show
  // their about: URIs until their actual page titles are known, e.g.
  // "about:addons" -> "Add-ons Manager". This is bug 1371896. Previously,
  // about: pages' initial tab titles were blank until the page title was known.
  let finishObservingLabelChanges = observeLabelChanges(secondTab, [ABOUT_ROBOTS_URI, ABOUT_ROBOTS_TITLE]);
  browserLoadedPromise = BrowserTestUtils.browserLoaded(secondTab.linkedBrowser, false, ABOUT_ROBOTS_URI);
  gBrowser.selectedTab = secondTab;
  await browserLoadedPromise;
  ok(!secondTab.hasAttribute("pending"), "second tab isn't pending anymore");
  ok(document.title.startsWith(ABOUT_ROBOTS_TITLE), "title bar displays content title");
  finishObservingLabelChanges();

  info("selecting the third tab");
  finishObservingLabelChanges = observeLabelChanges(thirdTab, ["example.com", CONTENT_TITLE]);
  browserLoadedPromise = BrowserTestUtils.browserLoaded(thirdTab.linkedBrowser, false, TEST_URL);
  gBrowser.selectedTab = thirdTab;
  await browserLoadedPromise;
  ok(!thirdTab.hasAttribute("pending"), "third tab isn't pending anymore");
  ok(document.title.startsWith(CONTENT_TITLE), "title bar displays content title");
  finishObservingLabelChanges();

  info("restoring the modified browser state");
  await TabStateFlusher.flushWindow(window);
  await promiseBrowserState(SessionStore.getBrowserState());
  [firstTab, secondTab, thirdTab] = gBrowser.tabs;
  is(thirdTab, gBrowser.selectedTab, "third tab is selected after restoring");
  ok(document.title.startsWith(CONTENT_TITLE), "title bar displays content title");
  ok(firstTab.hasAttribute("pending"), "first tab is pending after restoring");
  ok(secondTab.hasAttribute("pending"), "second tab is pending after restoring");
  is(secondTab.label, ABOUT_ROBOTS_TITLE, "second tab displays content title");
  ok(!thirdTab.hasAttribute("pending"), "third tab is not pending after restoring");
  is(thirdTab.label, CONTENT_TITLE, "third tab displays content title in pending state");

  info("selecting the first tab");
  finishObservingLabelChanges = observeLabelChanges(firstTab, [CONTENT_TITLE]);
  let tabContentRestored = TestUtils.topicObserved("sessionstore-debug-tab-restored");
  gBrowser.selectedTab = firstTab;
  ok(document.title.startsWith(CONTENT_TITLE), "title bar displays content title");
  await tabContentRestored;
  ok(!firstTab.hasAttribute("pending"), "first tab isn't pending anymore");
  finishObservingLabelChanges();

  await promiseBrowserState(BACKUP_STATE);
});
