/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that an about:privatebrowsing tab with no history will not
// be saved into session store and thus, it will not show up in
// Recently Closed Tabs.

add_task(function* () {
  while (ss.getClosedTabCount(window)) {
    ss.forgetClosedTab(window, 0);
  }

  is(ss.getClosedTabCount(window), 0, "should be no closed tabs");

  let tab = gBrowser.addTab("about:privatebrowsing");
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  is(gBrowser.browsers[1].currentURI.spec, "about:privatebrowsing",
     "we will be removing an about:privatebrowsing tab");

  gBrowser.removeTab(tab);
  is(ss.getClosedTabCount(window), 0, "should still be no closed tabs");
});
