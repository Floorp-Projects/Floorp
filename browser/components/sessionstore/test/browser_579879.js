"use strict";

add_task(function* () {
  let tab1 = gBrowser.addTab("data:text/plain;charset=utf-8,foo");
  gBrowser.pinTab(tab1);

  yield promiseBrowserLoaded(tab1.linkedBrowser);
  let tab2 = gBrowser.addTab();
  gBrowser.pinTab(tab2);

  is(Array.indexOf(gBrowser.tabs, tab1), 0, "pinned tab 1 is at the first position");
  yield promiseRemoveTab(tab1);

  tab1 = undoCloseTab();
  ok(tab1.pinned, "pinned tab 1 has been restored as a pinned tab");
  is(Array.indexOf(gBrowser.tabs, tab1), 0, "pinned tab 1 has been restored to the first position");

  yield BrowserTestUtils.removeTab(tab1);
  yield BrowserTestUtils.removeTab(tab2);
});
