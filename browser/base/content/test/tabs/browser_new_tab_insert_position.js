/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

async function doTest(aInsertRelatedAfterCurrent, aInsertAfterCurrent) {
  const kDescription = "(aInsertRelatedAfterCurrent=" + aInsertRelatedAfterCurrent +
                       ", aInsertAfterCurrent=" + aInsertAfterCurrent + "): ";
  is(gBrowser.tabs.length, 1, kDescription + "one tab is open initially");

  await SpecialPowers.pushPrefEnv({set: [
    ["browser.tabs.opentabfor.middleclick", true],
    ["browser.tabs.loadBookmarksInBackground", false],
    ["browser.tabs.insertRelatedAfterCurrent", aInsertRelatedAfterCurrent],
    ["browser.tabs.insertAfterCurrent", aInsertAfterCurrent]
  ]});

  // Add a few tabs.
  let tabs = [];
  function addTab(aURL, aReferrer) {
    let tab = BrowserTestUtils.addTab(gBrowser, aURL, {referrerURI: aReferrer});
    tabs.push(tab);
    return BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  }

  await addTab("http://mochi.test:8888/#0");
  await addTab("http://mochi.test:8888/#1");
  await addTab("http://mochi.test:8888/#2");
  await addTab("http://mochi.test:8888/#3");

  // Create a new tab page which has a link to "example.com".
  let pageURL = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "http://example.com");
  pageURL = `${pageURL}file_new_tab_page.html`;
  let newTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, pageURL);
  let newTabURISpec = newTab.linkedBrowser.currentURI.spec;
  const kNewTabIndex = 1;
  gBrowser.moveTabTo(newTab, kNewTabIndex);

  let openTabIndex = aInsertRelatedAfterCurrent || aInsertAfterCurrent ?
    kNewTabIndex + 1 : gBrowser.tabs.length;
  let openTabDescription = aInsertRelatedAfterCurrent || aInsertAfterCurrent ?
    "immediately to the right" : "at rightmost";

  // Middle click on the cell should open example.com in a new tab.
  let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, "http://example.com/", true);
  await BrowserTestUtils.synthesizeMouseAtCenter("#link_to_example_com",
                                                 {button: 1}, gBrowser.selectedBrowser);
  let openTab = await newTabPromise;
  is(openTab.linkedBrowser.currentURI.spec, "http://example.com/",
     "Middle click should open site to correct url.");
  is(openTab._tPos, openTabIndex,
     kDescription + "Middle click should open site in a new tab " + openTabDescription);

  // Remove the new opened tab which loaded example.com.
  gBrowser.removeTab(gBrowser.tabs[openTabIndex]);

  // Go back to the new tab.
  gBrowser.selectedTab = newTab;
  is(gBrowser.selectedBrowser.currentURI.spec, newTabURISpec,
     kDescription + "New tab URI shouldn't be changed");

  openTabIndex = aInsertAfterCurrent ? kNewTabIndex + 1 : gBrowser.tabs.length;
  openTabDescription = aInsertAfterCurrent ? "immediately to the right" : "at rightmost";

  // Open about:mozilla in new tab from the URL bar.
  gURLBar.focus();
  gURLBar.select();
  newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, "about:mozilla");
  EventUtils.sendString("about:mozilla");
  EventUtils.synthesizeKey("KEY_Alt", { altKey: true, code: "AltLeft", type: "keydown" });
  EventUtils.synthesizeKey("KEY_Enter", { altKey: true, code: "Enter" });
  EventUtils.synthesizeKey("KEY_Alt", { altKey: false, code: "AltLeft", type: "keyup" });
  openTab = await newTabPromise;

  is(newTab.linkedBrowser.currentURI.spec, newTabURISpec,
     kDescription + "example.com should be loaded in current tab.");
  is(openTab.linkedBrowser.currentURI.spec, "about:mozilla",
     kDescription + "about:mozilla should be loaded in the new tab.");
  is(openTab._tPos, openTabIndex,
     kDescription + "Alt+Enter in the URL bar should open page in a new tab " + openTabDescription);

  // Remove all tabs opened by this test.
  while (gBrowser.tabs[1]) {
    gBrowser.removeTab(gBrowser.tabs[1]);
  }
}

add_task(async function() {
  // Current default settings.
  await doTest(true, false);
  // Perhaps, some users would love this settings.
  await doTest(true, true);
  // Maybe, unrealistic cases, but we should test these cases too.
  await doTest(false, true);
  await doTest(false, false);
});
