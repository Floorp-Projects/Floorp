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

  const menuPopup = tab.ownerDocument.getElementById(
    "context_reopenInContainer"
  ).menupopup;
  const menuPopupPromise = BrowserTestUtils.waitForEvent(
    menuPopup,
    "popupshown"
  );
  info(`About to open a popup`);
  menuPopup.openPopup();
  info(`Waiting for the menu popup promise`);
  await menuPopupPromise;
  info(`Awaited menu popup promise`);
  return menuPopup;
}

function openTabInContainer(gBrowser, url, reopenMenu, id) {
  let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, url, true);
  let menuitem = reopenMenu.querySelector(
    `menuitem[data-usercontextid="${id}"]`
  );
  EventUtils.synthesizeMouseAtCenter(menuitem, {}, menuitem.ownerGlobal);
  return tabPromise;
}

function loadTestSubscript(filePath) {
  Services.scriptloader.loadSubScript(new URL(filePath, gTestPath).href, this);
}
