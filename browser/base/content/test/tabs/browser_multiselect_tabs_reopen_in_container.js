"use strict";

const PREF_PRIVACY_USER_CONTEXT_ENABLED = "privacy.userContext.enabled";

async function openTabMenuFor(tab) {
  let tabMenu = tab.ownerDocument.getElementById("tabContextMenu");

  let tabMenuShown = BrowserTestUtils.waitForEvent(tabMenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(
    tab,
    { type: "contextmenu" },
    tab.ownerGlobal
  );
  await tabMenuShown;

  return tabMenu;
}

async function openReopenMenuForTab(tab) {
  openTabMenuFor(tab);

  let reopenItem = tab.ownerDocument.getElementById(
    "context_reopenInContainer"
  );
  ok(!reopenItem.hidden, "Reopen in Container item should be shown");

  let reopenMenu = reopenItem.getElementsByTagName("menupopup")[0];
  let reopenMenuShown = BrowserTestUtils.waitForEvent(reopenMenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(
    reopenItem,
    { type: "mousemove" },
    tab.ownerGlobal
  );
  await reopenMenuShown;

  return reopenMenu;
}

function checkMenuItem(reopenMenu, shown, hidden) {
  for (let id of shown) {
    ok(
      reopenMenu.querySelector(`menuitem[data-usercontextid="${id}"]`),
      `User context id ${id} should exist`
    );
  }
  for (let id of hidden) {
    ok(
      !reopenMenu.querySelector(`menuitem[data-usercontextid="${id}"]`),
      `User context id ${id} shouldn't exist`
    );
  }
}

function openTabInContainer(gBrowser, tab, reopenMenu, id) {
  let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, getUrl(tab), true);
  let menuitem = reopenMenu.querySelector(
    `menuitem[data-usercontextid="${id}"]`
  );
  EventUtils.synthesizeMouseAtCenter(menuitem, {}, menuitem.ownerGlobal);
  return tabPromise;
}

add_task(async function testReopen() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_PRIVACY_USER_CONTEXT_ENABLED, true]],
  });

  let tab1 = await addTab("http://mochi.test:8888/1");
  let tab2 = await addTab("http://mochi.test:8888/2");
  let tab3 = await addTab("http://mochi.test:8888/3");
  let tab4 = BrowserTestUtils.addTab(gBrowser, "http://mochi.test:8888/3", {
    createLazyBrowser: true,
  });

  await BrowserTestUtils.switchTab(gBrowser, tab1);

  await triggerClickOn(tab2, { ctrlKey: true });
  await triggerClickOn(tab4, { ctrlKey: true });

  for (let tab of [tab1, tab2, tab3, tab4]) {
    ok(
      !tab.hasAttribute("usercontextid"),
      "Tab with No Container should be opened"
    );
  }

  ok(tab1.multiselected, "Tab1 is multi-selected");
  ok(tab2.multiselected, "Tab2 is multi-selected");
  ok(!tab3.multiselected, "Tab3 is not multi-selected");
  ok(tab4.multiselected, "Tab4 is multi-selected");

  is(gBrowser.visibleTabs.length, 5, "We have 5 tabs open");

  let reopenMenu1 = await openReopenMenuForTab(tab1);
  checkMenuItem(reopenMenu1, [1, 2, 3, 4], [0]);
  let containerTab1 = await openTabInContainer(
    gBrowser,
    tab1,
    reopenMenu1,
    "1"
  );

  let tabs = gBrowser.visibleTabs;
  is(tabs.length, 8, "Now we have 8 tabs open");

  is(containerTab1._tPos, 2, "containerTab1 position is 3");
  is(
    containerTab1.getAttribute("usercontextid"),
    "1",
    "Tab(1) with UCI=1 should be opened"
  );
  is(getUrl(containerTab1), getUrl(tab1), "Same page (tab1) should be opened");

  let containerTab2 = tabs[4];
  is(
    containerTab2.getAttribute("usercontextid"),
    "1",
    "Tab(2) with UCI=1 should be opened"
  );
  await TestUtils.waitForCondition(function() {
    return getUrl(containerTab2) == getUrl(tab2);
  }, "Same page (tab2) should be opened");

  let containerTab4 = tabs[7];
  is(
    containerTab2.getAttribute("usercontextid"),
    "1",
    "Tab(4) with UCI=1 should be opened"
  );
  await TestUtils.waitForCondition(function() {
    return getUrl(containerTab4) == getUrl(tab4);
  }, "Same page (tab4) should be opened");

  for (let tab of tabs.filter(t => t != tabs[0])) {
    BrowserTestUtils.removeTab(tab);
  }
});
