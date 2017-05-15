"use strict";

// Tests that an about:privatebrowsing tab with no history will not
// be saved into session store and thus, it will not show up in
// Recently Closed Tabs.

add_task(async function() {
  let tab = BrowserTestUtils.addTab(gBrowser, "about:privatebrowsing");
  let browser = tab.linkedBrowser;
  await promiseBrowserLoaded(browser);

  is(gBrowser.browsers[1].currentURI.spec, "about:privatebrowsing",
     "we will be removing an about:privatebrowsing tab");

  let r = `rand-${Math.random()}`;
  ss.setTabValue(tab, "foobar", r);

  await promiseRemoveTab(tab);
  let closedTabData = ss.getClosedTabData(window);
  ok(!closedTabData.includes(r), "tab not stored in _closedTabs");
});
