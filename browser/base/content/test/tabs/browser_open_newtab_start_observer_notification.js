"use strict";

add_task(async function test_browser_open_newtab_start_observer_notification() {

  let observerFiredPromise = new Promise(resolve => {
    function observe(subject) {
      Services.obs.removeObserver(observe, "browser-open-newtab-start");
      resolve(subject.wrappedJSObject);
    }
    Services.obs.addObserver(observe, "browser-open-newtab-start");
  });

  // We're calling BrowserOpenTab() (rather the using BrowserTestUtils
  // because we want to be sure that it triggers the event to fire, since
  // it's very close to where various user-actions are triggered.
  BrowserOpenTab();
  const newTabCreatedPromise = await observerFiredPromise;
  const browser = await newTabCreatedPromise;
  const tab = gBrowser.selectedTab;

  ok(true, "browser-open-newtab-start observer not called");
  Assert.deepEqual(browser, tab.linkedBrowser, "browser-open-newtab-start notified with the created browser");

  BrowserTestUtils.removeTab(tab);
});
