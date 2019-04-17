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

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.ipc.processCount", 1]],
  });
});

add_task(async function test() {
  let prefix = "http://mochi.test:8888/browser/browser/components/privatebrowsing/test/browser/browser_privatebrowsing_concurrent_page.html";

  function getElts(browser) {
    return browser.contentTitle.split("|");
  }

  // Step 1
  let non_private_browser = gBrowser.selectedBrowser;
  let url = prefix + "?action=set&name=test&value=value&initial=true";
  BrowserTestUtils.loadURI(non_private_browser, url);
  await BrowserTestUtils.browserLoaded(non_private_browser, false, url);


  // Step 2
  let private_window = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  let private_browser = private_window.gBrowser.selectedBrowser;
  url = prefix + "?action=set&name=test2&value=value2";
  BrowserTestUtils.loadURI(private_browser, url);
  await BrowserTestUtils.browserLoaded(private_browser, false, url);


  // Step 3
  url = prefix + "?action=get&name=test2";
  BrowserTestUtils.loadURI(non_private_browser, url);
  await BrowserTestUtils.browserLoaded(non_private_browser, false, url);
  let elts = await getElts(non_private_browser);
  isnot(elts[0], "value2", "public window shouldn't see private storage");
  is(elts[1], "1", "public window should only see public items");


  // Step 4
  url = prefix + "?action=get&name=test";
  BrowserTestUtils.loadURI(private_browser, url);
  await BrowserTestUtils.browserLoaded(private_browser, false, url);
  elts = await getElts(private_browser);
  isnot(elts[0], "value", "private window shouldn't see public storage");
  is(elts[1], "1", "private window should only see private items");


  // Reopen the private window again, without privateBrowsing, which should clear the
  // the private storage.
  private_window.close();
  private_window = await BrowserTestUtils.openNewBrowserWindow({ private: false });
  private_browser = null;
  await new Promise(resolve => Cu.schedulePreciseGC(resolve));
  private_browser = private_window.gBrowser.selectedBrowser;

  url = prefix + "?action=get&name=test2";
  BrowserTestUtils.loadURI(private_browser, url);
  await BrowserTestUtils.browserLoaded(private_browser, false, url);
  elts = await getElts(private_browser);
  isnot(elts[0], "value2", "public window shouldn't see cleared private storage");
  is(elts[1], "1", "public window should only see public items");


  // Making it private again should clear the storage and it shouldn't
  // be able to see the old private storage as well.
  private_window.close();
  private_window = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  private_browser = null;
  await new Promise(resolve => Cu.schedulePreciseGC(resolve));
  private_browser = private_window.gBrowser.selectedBrowser;

  url = prefix + "?action=set&name=test3&value=value3";
  BrowserTestUtils.loadURI(private_browser, url);
  await BrowserTestUtils.browserLoaded(private_browser, false, url);
  elts = await getElts(private_browser);
  is(elts[1], "1", "private window should only see new private items");

  // Cleanup.
  url = prefix + "?final=true";
  BrowserTestUtils.loadURI(non_private_browser, url);
  await BrowserTestUtils.browserLoaded(non_private_browser, false, url);
  private_window.close();
});
