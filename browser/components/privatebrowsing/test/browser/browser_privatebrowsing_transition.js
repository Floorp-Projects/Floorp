/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
// Tests the order of events and notifications when entering/exiting private
// browsing mode. This ensures that all private data is removed on exit, e.g.
// a cookie set in on unload handler, see bug 476463.
let cookieManager = Cc["@mozilla.org/cookiemanager;1"].
                    getService(Ci.nsICookieManager2);
let pb = Cc["@mozilla.org/privatebrowsing;1"].
         getService(Ci.nsIPrivateBrowsingService);
let observerNotified = 0, firstUnloadFired = 0, secondUnloadFired = 0;

function pbObserver(aSubject, aTopic, aData) {
  if (aTopic != "private-browsing")
    return;
  switch (aData) {
    case "enter":
      observerNotified++;
      is(observerNotified, 1, "This should be the first notification");
      is(firstUnloadFired, 1, "The first unload event should have been processed by now");
      break;
    case "exit":
      Services.obs.removeObserver(pbObserver, "private-browsing");
      observerNotified++;
      is(observerNotified, 2, "This should be the second notification");
      is(secondUnloadFired, 1, "The second unload event should have been processed by now");
      break;
  }
}

function test() {
  waitForExplicitFinish();
  Services.obs.addObserver(pbObserver, "private-browsing", false);
  is(gBrowser.tabs.length, 1, "There should only be one tab");
  let testTab = gBrowser.addTab();
  gBrowser.selectedTab = testTab;
  testTab.linkedBrowser.addEventListener("unload", function () {
    testTab.linkedBrowser.removeEventListener("unload", arguments.callee, true);
    firstUnloadFired++;
    is(observerNotified, 0, "The notification shouldn't have been sent yet");
  }, true);

  pb.privateBrowsingEnabled = true;
  let testTab = gBrowser.addTab();
  gBrowser.selectedTab = testTab;
  testTab.linkedBrowser.addEventListener("unload", function () {
    testTab.linkedBrowser.removeEventListener("unload", arguments.callee, true);
    secondUnloadFired++;
    is(observerNotified, 1, "The notification shouldn't have been sent yet");
    cookieManager.add("example.com", "test/", "PB", "1", false, false, false, 1000000000000);
  }, true);

  pb.privateBrowsingEnabled = false;
  gBrowser.tabContainer.lastChild.linkedBrowser.addEventListener("unload", function () {
    gBrowser.tabContainer.lastChild.linkedBrowser.removeEventListener("unload", arguments.callee, true);
    let count = cookieManager.countCookiesFromHost("example.com");
    is(count, 0, "There shouldn't be any cookies once pb mode has exited");
    cookieManager.QueryInterface(Ci.nsICookieManager);
    cookieManager.remove("example.com", "PB", "test/", false);
  }, true);
  gBrowser.removeCurrentTab();
  finish();
}
