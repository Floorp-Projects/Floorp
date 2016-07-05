/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function* () {
  is(gBrowser.tabs.length, 1, "one tab is open initially");

  // Add a few tabs.
  let tabs = [];
  function addTab(aURL, aReferrer) {
    let tab = gBrowser.addTab(aURL, {referrerURI: aReferrer});
    tabs.push(tab);
    return BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  }

  yield addTab("http://mochi.test:8888/#0");
  yield addTab("http://mochi.test:8888/#1");
  yield addTab("http://mochi.test:8888/#2");
  yield addTab("http://mochi.test:8888/#3");

  // Create a new tab page with a "www.example.com" tile and move it to the 2nd tab position.
  yield setLinks("-1");
  yield* addNewTabPageTab();
  gBrowser.moveTabTo(gBrowser.selectedTab, 1);

  // Send a middle-click and confirm that the clicked site opens immediately next to the new tab page.
  yield BrowserTestUtils.synthesizeMouseAtCenter(".newtab-cell",
                                                 {button: 1}, gBrowser.selectedBrowser);

  yield BrowserTestUtils.browserLoaded(gBrowser.getBrowserAtIndex(2));
  is(gBrowser.getBrowserAtIndex(2).currentURI.spec, "http://example.com/",
     "Middle click opens site in a new tab immediately to the right.");
});
