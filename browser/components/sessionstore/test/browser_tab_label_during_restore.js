/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

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
  const TEST_URL = "http://example.com/";

  function observeLabelChanges(tab) {
    info("observing tab label changes. initial label: " + tab.label);
    let labelChangeCount = 0;
    function TabAttrModifiedListener(event) {
      if (event.detail.changed.some(attr => { return attr == "label" })) {
        info("tab label change: " + tab.label);
        labelChangeCount++;
      }
    }
    tab.addEventListener("TabAttrModified", TabAttrModifiedListener);
    return (expectedCount) => {
      tab.removeEventListener("TabAttrModified", TabAttrModifiedListener);
      is(labelChangeCount, expectedCount, "observed tab label changes");
    }
  }

  info("setting test browser state");
  let browserLoadedPromise = BrowserTestUtils.firstBrowserLoaded(window, false);
  await promiseBrowserState({
    windows: [{
      tabs: [
        { entries: [{ url: TEST_URL }] },
        { entries: [{ url: TEST_URL }] },
      ]
    }]
  });
  let [firstTab, secondTab] = gBrowser.tabs;
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
  is(secondTab.label, TEST_URL, "second tab displays URL as its title");

  info("selecting the second tab");
  let checkLabelChangeCount = observeLabelChanges(secondTab);
  browserLoadedPromise = BrowserTestUtils.browserLoaded(secondTab.linkedBrowser, false, TEST_URL);
  gBrowser.selectedTab = secondTab;
  await browserLoadedPromise;
  ok(!secondTab.hasAttribute("pending"), "second tab isn't pending anymore");
  is(secondTab.label, CONTENT_TITLE, "second tab displays content title");
  ok(document.title.startsWith(CONTENT_TITLE), "title bar displays content title");
  checkLabelChangeCount(1);

  info("restoring the modified browser state");
  await TabStateFlusher.flushWindow(window);
  await promiseBrowserState(SessionStore.getBrowserState());
  [firstTab, secondTab] = gBrowser.tabs;
  is(secondTab, gBrowser.selectedTab, "second tab is selected after restoring");
  ok(document.title.startsWith(CONTENT_TITLE), "title bar displays content title");
  ok(firstTab.hasAttribute("pending"), "first tab is pending after restoring");
  is(firstTab.label, CONTENT_TITLE, "first tab displays content title in pending state");

  info("selecting the first tab");
  checkLabelChangeCount = observeLabelChanges(firstTab);
  let tabContentRestored = TestUtils.topicObserved("sessionstore-debug-tab-restored");
  gBrowser.selectedTab = firstTab;
  ok(document.title.startsWith(CONTENT_TITLE), "title bar displays content title");
  await tabContentRestored;
  ok(!firstTab.hasAttribute("pending"), "first tab isn't pending anymore");
  checkLabelChangeCount(0);
  is(firstTab.label, CONTENT_TITLE, "first tab displays content title after restoring content");

  await promiseBrowserState(BACKUP_STATE);
});

