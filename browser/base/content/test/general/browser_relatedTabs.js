/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function() {
  is(gBrowser.tabs.length, 1, "one tab is open initially");

  // Add several new tabs in sequence, interrupted by selecting a
  // different tab, moving a tab around and closing a tab,
  // returning a list of opened tabs for verifying the expected order.
  // The new tab behaviour is documented in bug 465673
  let tabs = [];
  let ReferrerInfo = Components.Constructor(
    "@mozilla.org/referrer-info;1",
    "nsIReferrerInfo",
    "init"
  );

  function addTab(aURL, aReferrer) {
    let referrerInfo = new ReferrerInfo(
      Ci.nsIReferrerInfo.EMPTY,
      true,
      aReferrer
    );
    let tab = BrowserTestUtils.addTab(gBrowser, aURL, { referrerInfo });
    tabs.push(tab);
    return BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  }

  await addTab("http://mochi.test:8888/#0");
  gBrowser.selectedTab = tabs[0];
  await addTab("http://mochi.test:8888/#1");
  await addTab("http://mochi.test:8888/#2", gBrowser.currentURI);
  await addTab("http://mochi.test:8888/#3", gBrowser.currentURI);
  gBrowser.selectedTab = tabs[tabs.length - 1];
  gBrowser.selectedTab = tabs[0];
  await addTab("http://mochi.test:8888/#4", gBrowser.currentURI);
  gBrowser.selectedTab = tabs[3];
  await addTab("http://mochi.test:8888/#5", gBrowser.currentURI);
  gBrowser.removeTab(tabs.pop());
  await addTab("about:blank", gBrowser.currentURI);
  gBrowser.moveTabTo(gBrowser.selectedTab, 1);
  await addTab("http://mochi.test:8888/#6", gBrowser.currentURI);
  await addTab();
  await addTab("http://mochi.test:8888/#7");

  function testPosition(tabNum, expectedPosition, msg) {
    is(
      Array.prototype.indexOf.call(gBrowser.tabs, tabs[tabNum]),
      expectedPosition,
      msg
    );
  }

  testPosition(0, 3, "tab without referrer was opened to the far right");
  testPosition(1, 7, "tab without referrer was opened to the far right");
  testPosition(2, 5, "tab with referrer opened immediately to the right");
  testPosition(3, 1, "next tab with referrer opened further to the right");
  testPosition(
    4,
    4,
    "tab selection changed, tab opens immediately to the right"
  );
  testPosition(
    5,
    6,
    "blank tab with referrer opens to the right of 3rd original tab where removed tab was"
  );
  testPosition(6, 2, "tab has moved, new tab opens immediately to the right");
  testPosition(7, 8, "blank tab without referrer opens at the end");
  testPosition(8, 9, "tab without referrer opens at the end");

  tabs.forEach(gBrowser.removeTab, gBrowser);
});
