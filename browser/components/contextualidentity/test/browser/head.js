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
  await openTabMenuFor(tab);

  let reopenItem = tab.ownerDocument.getElementById(
    "context_reopenInContainer"
  );
  ok(!reopenItem.hidden, "Reopen in Container item should be shown");

  const menuPopup = reopenItem.menupopup;
  const menuPopupPromise = BrowserTestUtils.waitForEvent(
    menuPopup,
    "popupshown"
  );
  info(`About to open a popup`);
  reopenItem.openMenu(true);
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
  info(`about to activate item`);
  reopenMenu.activateItem(menuitem);
  return tabPromise;
}

function loadTestSubscript(filePath) {
  Services.scriptloader.loadSubScript(new URL(filePath, gTestPath).href, this);
}

/**
 * Opens `uri' in a new tab with the provided userContextId and focuses it.
 *
 * @param {string} uri The uri which should be opened in the new tab.
 * @param {number} userContextId The id of the user context in which the tab
 *                               should be opened.
 * @returns {object} Keys are `tab` (the newly-opened tab) and `browser` (the
 *                   browser associated with the tab).
 */
async function openTabInUserContext(uri, userContextId) {
  let tab = BrowserTestUtils.addTab(gBrowser, uri, { userContextId });

  gBrowser.selectedTab = tab;
  tab.ownerGlobal.focus();

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);
  return { tab, browser };
}
