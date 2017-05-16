// Tests that an about:blank tab with no history will not be saved into
// session store and thus, it will not show up in Recently Closed Tabs.

"use strict";

add_task(async function() {
  let tab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  await promiseBrowserLoaded(tab.linkedBrowser);

  is(tab.linkedBrowser.currentURI.spec, "about:blank",
     "we will be removing an about:blank tab");

  let r = `rand-${Math.random()}`;
  ss.setTabValue(tab, "foobar", r);

  await promiseRemoveTab(tab);
  let closedTabData = ss.getClosedTabData(window);
  ok(!closedTabData.includes(r), "tab not stored in _closedTabs");
});
