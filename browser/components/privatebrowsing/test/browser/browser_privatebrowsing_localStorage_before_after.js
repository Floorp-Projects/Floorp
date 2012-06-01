/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Ensure that a storage instance used by both private and public sessions at different times does not
// allow any data to leak due to cached values.

// Step 1: Load browser_privatebrowsing_localStorage_before_after_page.html in a private tab, causing a storage
//   item to exist. Close the tab.
// Step 2: Load the same page in a non-private tab, ensuring that the storage instance reports only one item
//   existing.

function test() {
  let prefix = 'http://mochi.test:8888/browser/browser/components/privatebrowsing/test/browser/';
  waitForExplicitFinish();
  
  // We wait for a GC to ensure that all previous PB docshells in this test suite are destroyed
  // so that the PB localStorage instance is clean.
  Components.utils.schedulePreciseGC(function() {
    let tab = gBrowser.selectedTab = gBrowser.addTab();
    let browser = gBrowser.selectedBrowser;
    browser.docShell.QueryInterface(Ci.nsILoadContext).usePrivateBrowsing = true;
    browser.addEventListener('load', function() {
      browser.removeEventListener('load', arguments.callee, true);
      is(browser.contentWindow.document.title, '1', "localStorage should contain 1 item");
      browser.docShell.QueryInterface(Ci.nsILoadContext).usePrivateBrowsing = false;

      gBrowser.selectedTab = gBrowser.addTab();
      let browser2 = gBrowser.selectedBrowser;
      gBrowser.removeTab(tab);
      browser2.addEventListener('load', function() {
        browser2.removeEventListener('load', arguments.callee, true);
        is(browser2.contentWindow.document.title, 'null|0', 'localStorage should contain 0 items');
        gBrowser.removeCurrentTab();
        finish();
      }, true);
      browser2.loadURI(prefix + 'browser_privatebrowsing_localStorage_before_after_page2.html');
    }, true);
    browser.loadURI(prefix + 'browser_privatebrowsing_localStorage_before_after_page.html');
  });
}
