/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test opening two tabs that share a localStorage, but keep one in private mode.
// Ensure that values from one don't leak into the other, and that values from
// earlier private storage sessions aren't visible later.

// Step 1: create new tab, load a page that sets test=value in non-private storage
// Step 2: create a new tab, load a page that sets test2=value2 in private storage
// Step 3: load a page in the tab from step 1 that checks the value of test2 is value2 and the total count in non-private storage is 1
// Step 4: load a page in the tab from step 2 that checks the value of test is value and the total count in private storage is 1

add_task(function test() {
  let prefix = 'http://mochi.test:8888/browser/browser/components/privatebrowsing/test/browser/browser_privatebrowsing_concurrent_page.html';

  function setUsePrivateBrowsing(browser, val) {
    return ContentTask.spawn(browser, val, function* (val) {
      docShell.QueryInterface(Ci.nsILoadContext).usePrivateBrowsing = val;
    });
  };

  function getElts(browser) {
    return browser.contentTitle.split('|');
  };

  // Step 1
  gBrowser.selectedTab = gBrowser.addTab(prefix + '?action=set&name=test&value=value&initial=true');
  let non_private_browser = gBrowser.selectedBrowser;
  yield BrowserTestUtils.browserLoaded(non_private_browser);


  // Step 2
  gBrowser.selectedTab = gBrowser.addTab();
  let private_browser = gBrowser.selectedBrowser;
  yield BrowserTestUtils.browserLoaded(private_browser);
  yield setUsePrivateBrowsing(private_browser, true);
  private_browser.loadURI(prefix + '?action=set&name=test2&value=value2');
  yield BrowserTestUtils.browserLoaded(private_browser);


  // Step 3
  non_private_browser.loadURI(prefix + '?action=get&name=test2');
  yield BrowserTestUtils.browserLoaded(non_private_browser);
  let elts = yield getElts(non_private_browser);
  isnot(elts[0], 'value2', "public window shouldn't see private storage");
  is(elts[1], '1', "public window should only see public items");


  // Step 4
  private_browser.loadURI(prefix + '?action=get&name=test');
  yield BrowserTestUtils.browserLoaded(private_browser);
  elts = yield getElts(private_browser);
  isnot(elts[0], 'value', "private window shouldn't see public storage");
  is(elts[1], '1', "private window should only see private items");


  // Make the private tab public again, which should clear the
  // the private storage.
  yield setUsePrivateBrowsing(private_browser, false);
  yield new Promise(resolve => Cu.schedulePreciseGC(resolve));

  private_browser.loadURI(prefix + '?action=get&name=test2');
  yield BrowserTestUtils.browserLoaded(private_browser);
  elts = yield getElts(private_browser);
  isnot(elts[0], 'value2', "public window shouldn't see cleared private storage");
  is(elts[1], '1', "public window should only see public items");


  // Making it private again should clear the storage and it shouldn't
  // be able to see the old private storage as well.
  yield setUsePrivateBrowsing(private_browser, true);

  private_browser.loadURI(prefix + '?action=set&name=test3&value=value3');
  BrowserTestUtils.browserLoaded(private_browser);
  elts = yield getElts(private_browser);
  is(elts[1], '1', "private window should only see new private items");

  // Cleanup.
  non_private_browser.loadURI(prefix + '?final=true');
  yield BrowserTestUtils.browserLoaded(non_private_browser);
  gBrowser.removeCurrentTab();
  gBrowser.removeCurrentTab();
});
