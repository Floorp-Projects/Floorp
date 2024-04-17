/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function dragAndDrop(
  tab1,
  tab2,
  initialWindow = window,
  destWindow = window,
  afterTab = true,
  context
) {
  let rect = tab2.getBoundingClientRect();
  let event = {
    ctrlKey: false,
    altKey: false,
    clientX: rect.left + rect.width / 2 + 10 * (afterTab ? 1 : -1),
    clientY: rect.top + rect.height / 2,
  };

  if (destWindow != initialWindow) {
    // Make sure that both tab1 and tab2 are visible
    initialWindow.focus();
    initialWindow.moveTo(rect.left, rect.top + rect.height * 3);
  }

  EventUtils.synthesizeDrop(
    tab1,
    tab2,
    null,
    "move",
    initialWindow,
    destWindow,
    event
  );

  // Ensure dnd suppression is cleared.
  EventUtils.synthesizeMouseAtCenter(tab2, { type: "mouseup" }, context);
}

add_task(async function () {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, URLs[0]);
  await BrowserTestUtils.openNewForegroundTab(gBrowser, URLs[1]);
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    let win1 = browser.ownerGlobal;
    await navigateToViewAndWait(document, "opentabs");

    let openTabs = document.querySelector("view-opentabs[name=opentabs]");
    await openTabs.updateComplete;
    await TestUtils.waitForCondition(
      () => openTabs.viewCards[0].tabList.rowEls.length
    );
    await openTabs.openTabsTarget.readyWindowsPromise;
    let card = openTabs.viewCards[0];
    let tabRows = card.tabList.rowEls;
    let tabChangeRaised;

    // Pin first two tabs
    for (var i = 0; i < 2; i++) {
      tabChangeRaised = BrowserTestUtils.waitForEvent(
        NonPrivateTabs,
        "TabChange"
      );
      let currentTabEl = tabRows[i];
      let currentTab = currentTabEl.tabElement;
      info(`Pinning tab ${i + 1} with label: ${currentTab.label}`);
      win1.gBrowser.pinTab(currentTab);
      await tabChangeRaised;
      await openTabs.updateComplete;
      tabRows = card.tabList.rowEls;
      currentTabEl = tabRows[i];

      await TestUtils.waitForCondition(
        () => currentTabEl.indicators.includes("pinned"),
        `Tab ${i + 1} is pinned.`
      );
    }

    info(`First two tabs are pinned.`);

    let win2 = await BrowserTestUtils.openNewBrowserWindow();

    await openTabs.updateComplete;
    await TestUtils.waitForCondition(
      () => openTabs.viewCards.length === 2,
      "Two windows are shown for Open Tabs in in Fx View."
    );

    let pinnedTab = win1.gBrowser.visibleTabs[0];
    let newWindowTab = win2.gBrowser.visibleTabs[0];

    dragAndDrop(newWindowTab, pinnedTab, win2, win1, true, content);

    await switchToFxViewTab();
    await openTabs.updateComplete;
    await TestUtils.waitForCondition(
      () => openTabs.viewCards.length === 1,
      "One window is shown for Open Tabs in in Fx View."
    );
  });
  cleanupTabs();
});
