"use strict";

add_task(async function test_browser_open_newtab_start_observer_notification() {

  let observerFiredPromise = new Promise(resolve => {
    function observe() {
      resolve();
    }
    Services.obs.addObserver(observe, "browser-open-newtab-start");
  });

  // We're calling BrowserOpenTab() (rather the using BrowserTestUtils
  // because we want to be sure that it triggers the event to fire, since
  // it's very close to where various user-actions are triggered.
  BrowserOpenTab();
  await observerFiredPromise;
  const tab = gBrowser.selectedTab;

  ok(true, "browser-open-newtab-start observer not called");

  await BrowserTestUtils.removeTab(tab);
});
