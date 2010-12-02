/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  test_tab("about:blank");
  test_tab("about:license");
}

function test_tab(url) {
  let originalTab = gBrowser.selectedTab;
  let newTab = gBrowser.addTab(url, {skipAnimation: true});
  is(tabIsActive(newTab), false, "newly added " + url + " tab is not active");
  is(tabIsActive(originalTab), true, "original tab is active initially");

  gBrowser.selectedTab = newTab;
  is(tabIsActive(newTab), true, "newly added " + url + " tab is active after selection");
  is(tabIsActive(originalTab), false, "original tab is not active while unselected");

  gBrowser.selectedTab = originalTab;
  is(tabIsActive(newTab), false, "newly added " + url + " tab is not active after switch back");
  is(tabIsActive(originalTab), true, "original tab is active again after switch back");
  
  gBrowser.removeTab(newTab);
}

function tabIsActive(tab) {
  let browser = tab.linkedBrowser;
  return browser.docShell.isActive;
}
