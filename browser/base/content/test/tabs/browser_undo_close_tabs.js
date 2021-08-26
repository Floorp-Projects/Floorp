/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const PREF_WARN_ON_CLOSE = "browser.tabs.warnOnCloseOtherTabs";

add_task(async function setPref() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_WARN_ON_CLOSE, false]],
  });
});

add_task(async function withMultiSelectedTabs() {
  let initialTab = gBrowser.selectedTab;
  let tab1 = await addTab("https://example.com/1");
  let tab2 = await addTab("https://example.com/2");
  let tab3 = await addTab("https://example.com/3");
  let tab4 = await addTab("https://example.com/4");

  is(gBrowser.multiSelectedTabsCount, 0, "Zero multiselected tabs");

  gBrowser.selectedTab = tab2;
  await triggerClickOn(tab4, { shiftKey: true });

  ok(!initialTab.multiselected, "InitialTab is not multiselected");
  ok(!tab1.multiselected, "Tab1 is not multiselected");
  ok(tab2.multiselected, "Tab2 is multiselected");
  ok(tab3.multiselected, "Tab3 is multiselected");
  ok(tab4.multiselected, "Tab4 is multiselected");
  is(gBrowser.multiSelectedTabsCount, 3, "Two multiselected tabs");

  gBrowser.removeMultiSelectedTabs();
  await TestUtils.waitForCondition(
    () => gBrowser.tabs.length == 2,
    "wait for the multiselected tabs to close"
  );
  is(
    SessionStore.getLastClosedTabCount(window),
    3,
    "SessionStore should know how many tabs were just closed"
  );

  undoCloseTab();
  await TestUtils.waitForCondition(
    () => gBrowser.tabs.length == 5,
    "wait for the tabs to reopen"
  );
  info("waiting for the browsers to finish loading");
  // Check that the tabs are restored in the correct order
  for (let tabId of [2, 3, 4]) {
    let browser = gBrowser.tabs[tabId].linkedBrowser;
    await ContentTask.spawn(browser, tabId, async aTabId => {
      await ContentTaskUtils.waitForCondition(() => {
        return (
          content?.document?.readyState == "complete" &&
          content?.document?.location.href == "https://example.com/" + aTabId
        );
      }, "waiting for tab " + aTabId + " to load");
    });
  }

  gBrowser.removeAllTabsBut(initialTab);
});

add_task(async function withCloseTabsToTheRight() {
  let initialTab = gBrowser.selectedTab;
  let tab1 = await addTab("https://example.com/1");
  await addTab("https://example.com/2");
  await addTab("https://example.com/3");
  await addTab("https://example.com/4");

  gBrowser.removeTabsToTheEndFrom(tab1);
  await TestUtils.waitForCondition(
    () => gBrowser.tabs.length == 2,
    "wait for the multiselected tabs to close"
  );
  is(
    SessionStore.getLastClosedTabCount(window),
    3,
    "SessionStore should know how many tabs were just closed"
  );

  undoCloseTab();
  await TestUtils.waitForCondition(
    () => gBrowser.tabs.length == 5,
    "wait for the tabs to reopen"
  );
  info("waiting for the browsers to finish loading");
  // Check that the tabs are restored in the correct order
  for (let tabId of [2, 3, 4]) {
    let browser = gBrowser.tabs[tabId].linkedBrowser;
    ContentTask.spawn(browser, tabId, async aTabId => {
      await ContentTaskUtils.waitForCondition(() => {
        return (
          content?.document?.readyState == "complete" &&
          content?.document?.location.href == "https://example.com/" + aTabId
        );
      }, "waiting for tab " + aTabId + " to load");
    });
  }

  gBrowser.removeAllTabsBut(initialTab);
});
