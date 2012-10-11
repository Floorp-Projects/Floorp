/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  waitForExplicitFinish();
  pb.privateBrowsingEnabled = true;
  let tab = gBrowser.selectedTab = gBrowser.addTab();
  let browser = gBrowser.selectedBrowser;
  browser.addEventListener('load', function() {
    browser.removeEventListener('load', arguments.callee, true);
    let tab2 = gBrowser.selectedTab = gBrowser.addTab();
    browser.contentWindow.location = 'http://mochi.test:8888/browser/browser/components/privatebrowsing/test/browser/' +
                     'browser_privatebrowsing_localStorage_page2.html';
    browser.addEventListener('load', function() {
      browser.removeEventListener('load', arguments.callee, true);
      is(browser.contentWindow.document.title, '2', "localStorage should contain 2 items");
      pb.privateBrowsingEnabled = false;
      finish();
    }, true);
  }, true);
  browser.loadURI('http://mochi.test:8888/browser/browser/components/privatebrowsing/test/browser/' +
                  'browser_privatebrowsing_localStorage_page1.html');
}
