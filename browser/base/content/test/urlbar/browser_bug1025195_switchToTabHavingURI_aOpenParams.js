/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(function test_ignoreFragment() {
  let tabRefAboutHome = gBrowser.addTab("about:home#1");
  yield promiseTabLoaded(tabRefAboutHome);
  let tabRefAboutMozilla = gBrowser.addTab("about:mozilla");
  yield promiseTabLoaded(tabRefAboutMozilla);

  gBrowser.selectedTab = tabRefAboutMozilla;
  let numTabsAtStart = gBrowser.tabs.length;

  switchTab("about:home#1", true);
  switchTab("about:mozilla", true);

  let hashChangePromise = new Promise(resolve => {
    tabRefAboutHome.linkedBrowser.contentWindow.addEventListener("hashchange", resolve, false);
  });
  switchTab("about:home#2", true, { ignoreFragment: true });
  is(tabRefAboutHome, gBrowser.selectedTab, "The same about:home tab should be switched to");
  yield hashChangePromise;
  is(gBrowser.currentURI.ref, "2", "The ref should be updated to the new ref");
  switchTab("about:mozilla", true);
  switchTab("about:home#1", false);
  isnot(tabRefAboutHome, gBrowser.selectedTab, "Selected tab should not be initial about:blank tab");
  is(gBrowser.tabs.length, numTabsAtStart + 1, "Should have one new tab opened");
  switchTab("about:mozilla", true);
  switchTab("about:home", true, {ignoreFragment: true});
  yield promiseWaitForCondition(function() {
    return tabRefAboutHome.linkedBrowser.currentURI.spec == "about:home";
  });
  is(tabRefAboutHome.linkedBrowser.currentURI.spec, "about:home", "about:home shouldn't have hash");
  switchTab("about:about", false, { ignoreFragment: true });
  cleanupTestTabs();
});

add_task(function test_ignoreQueryString() {
  let tabRefAboutHome = gBrowser.addTab("about:home?hello=firefox");
  yield promiseTabLoaded(tabRefAboutHome);
  let tabRefAboutMozilla = gBrowser.addTab("about:mozilla");
  yield promiseTabLoaded(tabRefAboutMozilla);
  gBrowser.selectedTab = tabRefAboutMozilla;

  switchTab("about:home?hello=firefox", true);
  switchTab("about:home?hello=firefoxos", false);
  // Remove the last opened tab to test ignoreQueryString option.
  gBrowser.removeCurrentTab();
  switchTab("about:home?hello=firefoxos", true, { ignoreQueryString: true });
  is(tabRefAboutHome, gBrowser.selectedTab, "Selected tab should be the initial about:home tab");
  is(gBrowser.currentURI.spec, "about:home?hello=firefox", "The spec should NOT be updated to the new query string");
  cleanupTestTabs();
});

add_task(function test_replaceQueryString() {
  let tabRefAboutHome = gBrowser.addTab("about:home?hello=firefox");
  yield promiseTabLoaded(tabRefAboutHome);
  let tabRefAboutMozilla = gBrowser.addTab("about:mozilla");
  yield promiseTabLoaded(tabRefAboutMozilla);
  gBrowser.selectedTab = tabRefAboutMozilla;

  switchTab("about:home", false);
  switchTab("about:home?hello=firefox", true);
  switchTab("about:home?hello=firefoxos", false);
  // Remove the last opened tab to test replaceQueryString option.
  gBrowser.removeCurrentTab();
  switchTab("about:home?hello=firefoxos", true, { replaceQueryString: true });
  is(tabRefAboutHome, gBrowser.selectedTab, "Selected tab should be the initial about:home tab");
  // Wait for the tab to load the new URI spec.
  yield promiseTabLoaded(tabRefAboutHome);
  is(gBrowser.currentURI.spec, "about:home?hello=firefoxos", "The spec should be updated to the new spec");
  cleanupTestTabs();
});

// Begin helpers

function cleanupTestTabs() {
  while (gBrowser.tabs.length > 1)
    gBrowser.removeCurrentTab();
}

function switchTab(aURI, aShouldFindExistingTab, aOpenParams = {}) {
  // Build the description before switchToTabHavingURI deletes the object properties.
  let msg = "Should switch to existing " + aURI + " tab if one existed, " +
        (aOpenParams.ignoreFragment ? "ignoring" : "including") + " fragment portion, " +
        (aOpenParams.ignoreQueryString || aOpenParams.replaceQueryString ?
         (aOpenParams.replaceQueryString ? "replacing" : "ignoring") :
         "including"
        ) + " query string.";
  let tabFound = switchToTabHavingURI(aURI, true, aOpenParams);
  is(tabFound, aShouldFindExistingTab, msg);
}

registerCleanupFunction(cleanupTestTabs);
