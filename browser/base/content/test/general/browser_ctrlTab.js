add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.ctrlTab.sortByRecentlyUsed", true]],
  });

  BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.addTab(gBrowser);

  // While doing this test, we should make sure the selected tab in the tab
  // preview is not changed by mouse events.  That may happen after closing
  // the selected tab with ctrl+W.  Disable all mouse events to prevent it.
  for (let node of ctrlTab.previews) {
    node.style.pointerEvents = "none";
  }
  registerCleanupFunction(function() {
    for (let node of ctrlTab.previews) {
      try {
        node.style.removeProperty("pointer-events");
      } catch (e) {}
    }
  });

  checkTabs(4);

  await ctrlTabTest([2], 1, 0);
  await ctrlTabTest([2, 3, 1], 2, 2);
  await ctrlTabTest([], 4, 2);

  {
    let selectedIndex = gBrowser.tabContainer.selectedIndex;
    await pressCtrlTab();
    await pressCtrlTab(true);
    await releaseCtrl();
    is(
      gBrowser.tabContainer.selectedIndex,
      selectedIndex,
      "Ctrl+Tab -> Ctrl+Shift+Tab keeps the selected tab"
    );
  }

  {
    info("test for bug 445369");
    let tabs = gBrowser.tabs.length;
    await pressCtrlTab();
    await synthesizeCtrlW();
    is(gBrowser.tabs.length, tabs - 1, "Ctrl+Tab -> Ctrl+W removes one tab");
    await releaseCtrl();
  }

  {
    info("test for bug 667314");
    let tabs = gBrowser.tabs.length;
    await pressCtrlTab();
    await pressCtrlTab(true);
    await synthesizeCtrlW();
    is(
      gBrowser.tabs.length,
      tabs - 1,
      "Ctrl+Tab -> Ctrl+W removes the selected tab"
    );
    await releaseCtrl();
  }

  BrowserTestUtils.addTab(gBrowser);
  checkTabs(3);
  await ctrlTabTest([2, 1, 0], 7, 1);

  {
    info("test for bug 1292049");
    let tabToClose = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      "about:buildconfig"
    );
    checkTabs(4);
    selectTabs([0, 1, 2, 3]);

    let promise = BrowserTestUtils.waitForSessionStoreUpdate(tabToClose);
    BrowserTestUtils.removeTab(tabToClose);
    await promise;
    checkTabs(3);
    undoCloseTab();
    checkTabs(4);
    is(
      gBrowser.tabContainer.selectedIndex,
      3,
      "tab is selected after closing and restoring it"
    );

    await ctrlTabTest([], 1, 2);
  }

  {
    info("test for bug 445369");
    checkTabs(4);
    selectTabs([1, 2, 0]);

    let selectedTab = gBrowser.selectedTab;
    let tabToRemove = gBrowser.tabs[1];

    await pressCtrlTab();
    await pressCtrlTab();
    await synthesizeCtrlW();
    ok(
      !tabToRemove.parentNode,
      "Ctrl+Tab*2 -> Ctrl+W removes the second most recently selected tab"
    );

    await pressCtrlTab(true);
    await pressCtrlTab(true);
    await releaseCtrl();
    ok(
      selectedTab.selected,
      "Ctrl+Tab*2 -> Ctrl+W -> Ctrl+Shift+Tab*2 keeps the selected tab"
    );
  }
  gBrowser.removeTab(gBrowser.tabs[gBrowser.tabs.length - 1]);
  checkTabs(2);

  await ctrlTabTest([1], 1, 0);

  gBrowser.removeTab(gBrowser.tabs[gBrowser.tabs.length - 1]);
  checkTabs(1);

  {
    info("test for bug 445768");
    let focusedWindow = document.commandDispatcher.focusedWindow;
    let eventConsumed = true;
    let detectKeyEvent = function(event) {
      eventConsumed = event.defaultPrevented;
    };
    document.addEventListener("keypress", detectKeyEvent);
    await pressCtrlTab();
    document.removeEventListener("keypress", detectKeyEvent);
    ok(
      eventConsumed,
      "Ctrl+Tab consumed by the tabbed browser if one tab is open"
    );
    is(
      focusedWindow,
      document.commandDispatcher.focusedWindow,
      "Ctrl+Tab doesn't change focus if one tab is open"
    );
  }

  // eslint-disable-next-line no-lone-blocks
  {
    info("Bug 1731050: test hidden tabs");
    checkTabs(1);
    await BrowserTestUtils.addTab(gBrowser);
    await BrowserTestUtils.addTab(gBrowser);
    await BrowserTestUtils.addTab(gBrowser);
    await BrowserTestUtils.addTab(gBrowser);

    selectTabs([1, 2, 3, 4, 3]);
    gBrowser.hideTab(gBrowser.tabs[4]);
    selectTabs([2]);
    gBrowser.hideTab(gBrowser.tabs[3]);

    is(gBrowser.tabs[4].hidden, true, "Tab at index 4 is hidden");
    is(gBrowser.tabs[3].hidden, true, "Tab at index 3 is hidden");
    is(gBrowser.tabs[2].hidden, false, "Tab at index 2 is still shown");
    is(gBrowser.tabs[1].hidden, false, "Tab at index 1 is still shown");
    is(gBrowser.tabs[0].hidden, false, "Tab at index 0 is still shown");

    await ctrlTabTest([], 1, 1);
    await ctrlTabTest([], 2, 0);
    gBrowser.showTab(gBrowser.tabs[4]);
    await ctrlTabTest([2], 3, 4);
    await ctrlTabTest([], 4, 4);
    gBrowser.showTab(gBrowser.tabs[3]);
    await ctrlTabTest([], 4, 3);
    await ctrlTabTest([], 6, 4);

    await BrowserTestUtils.removeTab(gBrowser.tabs[4]);
    await BrowserTestUtils.removeTab(gBrowser.tabs[3]);
    await BrowserTestUtils.removeTab(gBrowser.tabs[2]);
    await BrowserTestUtils.removeTab(gBrowser.tabs[1]);
    info("End hidden tabs test");
  }

  /* private utility functions */

  /**
   * @return the number of times (Shift+)Ctrl+Tab was pressed
   */
  async function pressCtrlTab(aShiftKey = false) {
    let promise;
    if (!isOpen() && canOpen()) {
      ok(
        !aShiftKey,
        "Shouldn't attempt to open the panel by pressing Shift+Ctrl+Tab"
      );
      info("Pressing Ctrl+Tab to open the panel");
      promise = BrowserTestUtils.waitForEvent(ctrlTab.panel, "popupshown");
    } else {
      info(
        `Pressing ${aShiftKey ? "Shift+" : ""}Ctrl+Tab while the panel is open`
      );
      promise = BrowserTestUtils.waitForEvent(document, "keyup");
    }
    EventUtils.synthesizeKey("VK_TAB", {
      ctrlKey: true,
      shiftKey: !!aShiftKey,
    });
    await promise;
    if (document.activeElement == ctrlTab.showAllButton) {
      info("Repeating keypress to skip over the 'List all tabs' button");
      return 1 + (await pressCtrlTab(aShiftKey));
    }
    return 1;
  }

  async function releaseCtrl() {
    let promise;
    if (isOpen()) {
      promise = BrowserTestUtils.waitForEvent(ctrlTab.panel, "popuphidden");
    } else {
      promise = BrowserTestUtils.waitForEvent(document, "keyup");
    }
    EventUtils.synthesizeKey("VK_CONTROL", { type: "keyup" });
    await promise;
  }

  async function synthesizeCtrlW() {
    let promise = BrowserTestUtils.waitForEvent(
      gBrowser.tabContainer,
      "TabClose"
    );
    EventUtils.synthesizeKey("w", { ctrlKey: true });
    await promise;
  }

  function isOpen() {
    return ctrlTab.isOpen;
  }

  function canOpen() {
    return (
      Services.prefs.getBoolPref("browser.ctrlTab.sortByRecentlyUsed") &&
      gBrowser.tabs.length > 2
    );
  }

  function checkTabs(aTabs) {
    is(gBrowser.tabs.length, aTabs, "number of open tabs should be " + aTabs);
  }

  function selectTabs(tabs) {
    tabs.forEach(function(index) {
      gBrowser.selectedTab = gBrowser.tabs[index];
    });
  }

  async function ctrlTabTest(tabsToSelect, tabTimes, expectedIndex) {
    selectTabs(tabsToSelect);

    var indexStart = gBrowser.tabContainer.selectedIndex;
    var tabCount = gBrowser.visibleTabs.length;
    var normalized = tabTimes % tabCount;
    var where =
      normalized == 1
        ? "back to the previously selected tab"
        : normalized + " tabs back in most-recently-selected order";

    // Add keyup listener to all content documents.
    await Promise.all(
      gBrowser.tabs.map(tab =>
        SpecialPowers.spawn(tab.linkedBrowser, [], () => {
          content.window.addEventListener("keyup", () => {
            content.window._ctrlTabTestKeyupHappend = true;
          });
        })
      )
    );

    let numTimesPressed = 0;
    for (let i = 0; i < tabTimes; i++) {
      numTimesPressed += await pressCtrlTab();

      if (tabCount > 2) {
        is(
          gBrowser.tabContainer.selectedIndex,
          indexStart,
          "Selected tab doesn't change while tabbing"
        );
      }
    }

    if (tabCount > 2) {
      ok(
        isOpen(),
        "With " + tabCount + " visible tabs, Ctrl+Tab opens the preview panel"
      );

      await releaseCtrl();

      ok(!isOpen(), "Releasing Ctrl closes the preview panel");
    } else {
      ok(
        !isOpen(),
        "With " +
          tabCount +
          " visible tabs, Ctrl+Tab doesn't open the preview panel"
      );
    }

    is(
      gBrowser.tabContainer.selectedIndex,
      expectedIndex,
      "With " +
        tabCount +
        " visible tabs and tab " +
        indexStart +
        " selected, Ctrl+Tab*" +
        numTimesPressed +
        " goes " +
        where
    );

    const keyupEvents = await Promise.all(
      gBrowser.tabs.map(tab =>
        SpecialPowers.spawn(
          tab.linkedBrowser,
          [],
          () => !!content.window._ctrlTabTestKeyupHappend
        )
      )
    );
    ok(
      keyupEvents.every(isKeyupHappned => !isKeyupHappned),
      "Content document doesn't capture Keyup event during cycling tabs"
    );
  }
});
