add_task(async function testMoveStartEnabledClickedFromNonSelectedTab() {
  let tab = gBrowser.selectedTab;
  let tab2 = await addTab();
  let tab3 = await addTab();

  let tabs = [tab2, tab3];

  let menuItemMoveStartTab = document.getElementById("context_moveToStart");

  await triggerClickOn(tab, {});
  await triggerClickOn(tab2, { ctrlKey: true });

  ok(tab.multiselected, "Tab is multiselected");
  ok(tab2.multiselected, "Tab2 is multiselected");

  updateTabContextMenu(tab3);
  is(menuItemMoveStartTab.disabled, false, "Move Tab to Start is enabled");

  for (let tabToRemove of tabs) {
    BrowserTestUtils.removeTab(tabToRemove);
  }
});

add_task(async function testMoveStartDisabledFromFirstUnpinnedTab() {
  let tab = gBrowser.selectedTab;
  let tab2 = await addTab();

  let menuItemMoveStartTab = document.getElementById("context_moveToStart");

  gBrowser.pinTab(tab);

  updateTabContextMenu(tab2);
  is(menuItemMoveStartTab.disabled, true, "Move Tab to Start is disabled");

  BrowserTestUtils.removeTab(tab2);
  gBrowser.unpinTab(tab);
});

add_task(async function testMoveStartDisabledFromFirstPinnedTab() {
  let tab = gBrowser.selectedTab;
  let tab2 = await addTab();

  let menuItemMoveStartTab = document.getElementById("context_moveToStart");

  gBrowser.pinTab(tab);

  updateTabContextMenu(tab);
  is(menuItemMoveStartTab.disabled, true, "Move Tab to Start is disabled");

  BrowserTestUtils.removeTab(tab2);
  gBrowser.unpinTab(tab);
});

add_task(async function testMoveStartDisabledFromOnlyTab() {
  let tab = gBrowser.selectedTab;

  let menuItemMoveStartTab = document.getElementById("context_moveToStart");

  updateTabContextMenu(tab);
  is(menuItemMoveStartTab.disabled, true, "Move Tab to Start is disabled");
});

add_task(async function testMoveStartDisabledFromOnlyPinnedTab() {
  let tab = gBrowser.selectedTab;

  let menuItemMoveStartTab = document.getElementById("context_moveToStart");

  gBrowser.pinTab(tab);

  updateTabContextMenu(tab);
  is(menuItemMoveStartTab.disabled, true, "Move Tab to Start is disabled");

  gBrowser.unpinTab(tab);
});

add_task(async function testMoveStartEnabledFromLastPinnedTab() {
  let tab = gBrowser.selectedTab;
  let tab2 = await addTab();
  let tab3 = await addTab();

  let tabs = [tab2, tab3];

  let menuItemMoveStartTab = document.getElementById("context_moveToStart");

  gBrowser.pinTab(tab);
  gBrowser.pinTab(tab2);

  updateTabContextMenu(tab2);
  is(menuItemMoveStartTab.disabled, false, "Move Tab to Start is enabled");

  for (let tabToRemove of tabs) {
    BrowserTestUtils.removeTab(tabToRemove);
  }

  gBrowser.unpinTab(tab);
});

add_task(async function testMoveStartDisabledFromFirstVisibleTab() {
  let tab = gBrowser.selectedTab;
  let tab2 = await addTab();

  let menuItemMoveStartTab = document.getElementById("context_moveToStart");

  gBrowser.selectTabAtIndex(1);
  gBrowser.hideTab(tab);

  updateTabContextMenu(tab2);
  is(menuItemMoveStartTab.disabled, true, "Move Tab to Start is disabled");

  BrowserTestUtils.removeTab(tab2);
  gBrowser.showTab(tab);
});

add_task(async function testMoveEndEnabledClickedFromNonSelectedTab() {
  let tab = gBrowser.selectedTab;
  let tab2 = await addTab();
  let tab3 = await addTab();

  let tabs = [tab2, tab3];

  let menuItemMoveEndTab = document.getElementById("context_moveToEnd");

  await triggerClickOn(tab2, {});
  await triggerClickOn(tab3, { ctrlKey: true });

  ok(tab2.multiselected, "Tab2 is multiselected");
  ok(tab3.multiselected, "Tab3 is multiselected");

  updateTabContextMenu(tab);
  is(menuItemMoveEndTab.disabled, false, "Move Tab to End is enabled");

  for (let tabToRemove of tabs) {
    BrowserTestUtils.removeTab(tabToRemove);
  }
});

add_task(async function testMoveEndDisabledFromLastPinnedTab() {
  let tab = gBrowser.selectedTab;
  let tab2 = await addTab();
  let tab3 = await addTab();

  let tabs = [tab2, tab3];

  let menuItemMoveEndTab = document.getElementById("context_moveToEnd");

  gBrowser.pinTab(tab);
  gBrowser.pinTab(tab2);

  updateTabContextMenu(tab2);
  is(menuItemMoveEndTab.disabled, true, "Move Tab to End is disabled");

  for (let tabToRemove of tabs) {
    BrowserTestUtils.removeTab(tabToRemove);
  }
});

add_task(async function testMoveEndDisabledFromLastVisibleTab() {
  let tab = gBrowser.selectedTab;
  let tab2 = await addTab();

  let menuItemMoveEndTab = document.getElementById("context_moveToEnd");

  gBrowser.hideTab(tab2);

  updateTabContextMenu(tab);
  is(menuItemMoveEndTab.disabled, true, "Move Tab to End is disabled");

  BrowserTestUtils.removeTab(tab2);
  gBrowser.showTab(tab);
});

add_task(async function testMoveEndDisabledFromOnlyTab() {
  let tab = gBrowser.selectedTab;

  let menuItemMoveEndTab = document.getElementById("context_moveToEnd");

  updateTabContextMenu(tab);
  is(menuItemMoveEndTab.disabled, true, "Move Tab to End is disabled");
});

add_task(async function testMoveEndDisabledFromOnlyPinnedTab() {
  let tab = gBrowser.selectedTab;

  let menuItemMoveEndTab = document.getElementById("context_moveToEnd");

  gBrowser.pinTab(tab);

  updateTabContextMenu(tab);
  is(menuItemMoveEndTab.disabled, true, "Move Tab to End is disabled");

  gBrowser.unpinTab(tab);
});
