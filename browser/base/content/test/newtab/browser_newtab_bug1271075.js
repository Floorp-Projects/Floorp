/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  is(gBrowser.tabs.length, 1, "one tab is open initially");

  // Add a few tabs.
  let tabs = [];
  function addTab(aURL, aReferrer) {
    let tab = BrowserTestUtils.addTab(gBrowser, aURL, {referrerURI: aReferrer});
    tabs.push(tab);
    return BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  }

  await addTab("http://mochi.test:8888/#0");
  await addTab("http://mochi.test:8888/#1");
  await addTab("http://mochi.test:8888/#2");
  await addTab("http://mochi.test:8888/#3");

  // Create a new tab page with a "www.example.com" tile and move it to the 2nd tab position.
  await setLinks("-1");
  await addNewTabPageTab();
  gBrowser.moveTabTo(gBrowser.selectedTab, 1);

  // Send a middle-click and confirm that the clicked site opens immediately next to the new tab page.
  await BrowserTestUtils.synthesizeMouseAtCenter(".newtab-cell",
                                                 {button: 1}, gBrowser.selectedBrowser);

  await BrowserTestUtils.browserLoaded(gBrowser.getBrowserAtIndex(2));
  is(gBrowser.getBrowserAtIndex(2).currentURI.spec, "http://example.com/",
     "Middle click opens site in a new tab immediately to the right.");
});
