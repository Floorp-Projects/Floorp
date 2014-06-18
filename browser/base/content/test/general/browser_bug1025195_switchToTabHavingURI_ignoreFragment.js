/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(function() {
  registerCleanupFunction(function() {
    while (gBrowser.tabs.length > 1)
      gBrowser.removeCurrentTab();
  });
  let tabRefAboutHome = gBrowser.addTab("about:home#1");
  yield promiseTabLoaded(tabRefAboutHome);
  let tabRefAboutMozilla = gBrowser.addTab("about:mozilla");
  yield promiseTabLoaded(tabRefAboutMozilla);

  gBrowser.selectedTab = tabRefAboutMozilla;
  let numTabsAtStart = gBrowser.tabs.length;

  switchTab("about:home#1", false, true);
  switchTab("about:mozilla", false, true);
  switchTab("about:home#2", true, true);
  is(tabRefAboutHome, gBrowser.selectedTab, "The same about:home tab should be switched to");
  is(gBrowser.currentURI.ref, "2", "The ref should be updated to the new ref");
  switchTab("about:mozilla", false, true);
  switchTab("about:home#1", false, false);
  isnot(tabRefAboutHome, gBrowser.selectedTab, "Selected tab should not be initial about:blank tab");
  is(gBrowser.tabs.length, numTabsAtStart + 1, "Should have one new tab opened");
  switchTab("about:about", true, false);
});

function switchTab(aURI, aIgnoreFragment, aShouldFindExistingTab) {
  let tabFound = switchToTabHavingURI(aURI, true, {ignoreFragment: aIgnoreFragment});
  is(tabFound, aShouldFindExistingTab,
     "Should switch to existing " + aURI + " tab if one existed, " +
     (aIgnoreFragment ? "ignoring" : "including") + " fragment portion");
}
