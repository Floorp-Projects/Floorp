"use strict";

const TEST_HOST = "example.com";
const TEST_URL = "http://" + TEST_HOST + "/browser/browser/components/contextualidentity/test/browser/";

async function openTabMenuFor(tab) {
  let tabMenu = tab.ownerDocument.getElementById("tabContextMenu");

  let tabMenuShown = BrowserTestUtils.waitForEvent(tabMenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(tab, {type: "contextmenu"},
                                     tab.ownerGlobal);
  await tabMenuShown;

  return tabMenu;
}

async function openReopenMenuForTab(tab) {
  openTabMenuFor(tab);

  let reopenItem = tab.ownerDocument.getElementById("context_reopenInContainer");
  ok(!reopenItem.hidden, "Reopen in Container item should be shown");

  let reopenMenu = reopenItem.getElementsByTagName("menupopup")[0];
  let reopenMenuShown = BrowserTestUtils.waitForEvent(reopenMenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(reopenItem, {type: "mousemove"},
                                     tab.ownerGlobal);
  await reopenMenuShown;

  return reopenMenu;
}

function checkMenuItem(reopenMenu, shown, hidden) {
  for (let id of shown) {
    ok(reopenMenu.querySelector(`menuitem[data-usercontextid="${id}"]`),
       `User context id ${id} should exist`);
  }
  for (let id of hidden) {
    ok(!reopenMenu.querySelector(`menuitem[data-usercontextid="${id}"]`),
       `User context id ${id} shouldn't exist`);
  }
}

function openTabInContainer(gBrowser, reopenMenu, id) {
  let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, TEST_URL, true);
  let menuitem = reopenMenu.querySelector(`menuitem[data-usercontextid="${id}"]`);
  EventUtils.synthesizeMouseAtCenter(menuitem, {}, menuitem.ownerGlobal);
  return tabPromise;
}

add_task(async function testReopen() {
  await SpecialPowers.pushPrefEnv({"set": [
      ["privacy.userContext.enabled", true],
  ]});

  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_URL,
  });
  ok(!tab.hasAttribute("usercontextid"), "Tab with No Container should be opened");

  let reopenMenu = await openReopenMenuForTab(tab);
  checkMenuItem(reopenMenu, [1, 2, 3, 4], [0]);

  let containerTab = await openTabInContainer(gBrowser, reopenMenu, "1");

  is(containerTab.getAttribute("usercontextid"), "1",
     "Tab with UCI=1 should be opened");
  is(containerTab.linkedBrowser.currentURI.spec, TEST_URL,
     "Same page should be opened");

  reopenMenu = await openReopenMenuForTab(containerTab);
  checkMenuItem(reopenMenu, [0, 2, 3, 4], [1]);

  let noContainerTab = await openTabInContainer(gBrowser, reopenMenu, "0");

  ok(!noContainerTab.hasAttribute("usercontextid"),
     "Tab with no UCI should be opened");
  is(noContainerTab.linkedBrowser.currentURI.spec, TEST_URL,
     "Same page should be opened");

  BrowserTestUtils.removeTab(tab);
  BrowserTestUtils.removeTab(containerTab);
  BrowserTestUtils.removeTab(noContainerTab);
});

add_task(async function testDisabled() {
  await SpecialPowers.pushPrefEnv({"set": [
      ["privacy.userContext.enabled", false],
  ]});

  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_URL,
  });
  ok(!tab.hasAttribute("usercontextid"), "Tab with No Container should be opened");

  openTabMenuFor(tab);
  let reopenItem = document.getElementById("context_reopenInContainer");
  ok(reopenItem.hidden, "Reopen in Container item should be hidden");

  // Close the tab menu.
  EventUtils.synthesizeKey("KEY_Escape");

  BrowserTestUtils.removeTab(tab);
});

add_task(async function testPrivateMode() {
  await SpecialPowers.pushPrefEnv({"set": [
      ["privacy.userContext.enabled", true],
  ]});

  let privateWindow = await BrowserTestUtils.openNewBrowserWindow({private: true});
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser: privateWindow.gBrowser,
    url: TEST_URL,
  });
  ok(!tab.hasAttribute("usercontextid"), "Tab with No Container should be opened");

  openTabMenuFor(tab);
  let reopenItem = privateWindow.document.getElementById("context_reopenInContainer");
  ok(reopenItem.hidden, "Reopen in Container item should be hidden");

  // Close the tab menu.
  EventUtils.synthesizeKey("KEY_Escape");

  await BrowserTestUtils.closeWindow(privateWindow);
});
