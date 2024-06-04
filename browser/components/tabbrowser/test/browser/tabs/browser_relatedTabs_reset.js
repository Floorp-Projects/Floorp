/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function () {
  is(gBrowser.tabs.length, 1, "one tab is open initially");

  const TestPage =
    "http://mochi.test:8888/browser/browser/base/content/test/general/dummy_page.html";

  // Add several new tabs in sequence
  let tabs = [];
  let ReferrerInfo = Components.Constructor(
    "@mozilla.org/referrer-info;1",
    "nsIReferrerInfo",
    "init"
  );

  function getPrincipal(url, attrs) {
    let uri = Services.io.newURI(url);
    if (!attrs) {
      attrs = {};
    }
    return Services.scriptSecurityManager.createContentPrincipal(uri, attrs);
  }

  function addTab(aURL, aReferrer) {
    let referrerInfo = new ReferrerInfo(
      Ci.nsIReferrerInfo.EMPTY,
      true,
      aReferrer
    );
    let triggeringPrincipal = getPrincipal(aURL);
    let tab = BrowserTestUtils.addTab(gBrowser, aURL, {
      referrerInfo,
      triggeringPrincipal,
    });
    tabs.push(tab);
    return BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  }

  function loadTab(tab, url) {
    let loaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    info("Loading page: " + url);
    BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, url);
    return loaded;
  }

  function testPosition(tabNum, expectedPosition, msg) {
    is(
      Array.prototype.indexOf.call(gBrowser.tabs, tabs[tabNum]),
      expectedPosition,
      msg
    );
  }

  // Initial selected tab
  await addTab("http://mochi.test:8888/#0");
  testPosition(0, 1, "Initial tab opened in position 1");
  gBrowser.selectedTab = tabs[0];

  // Related tabs
  await addTab("http://mochi.test:8888/#1", gBrowser.currentURI);
  testPosition(1, 2, "Related tab was opened to the far right");

  await addTab("http://mochi.test:8888/#2", gBrowser.currentURI);
  testPosition(2, 3, "Related tab was opened to the far right");

  // Load a new page
  await loadTab(tabs[0], TestPage);

  // Add a new related tab after the page load
  await addTab("http://mochi.test:8888/#3", gBrowser.currentURI);
  testPosition(
    3,
    2,
    "Tab opened to the right of initial tab after system navigation"
  );

  tabs.forEach(gBrowser.removeTab, gBrowser);
});
