/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/**
 * This test verifies behavior from bug 1732375:
 * https://bugzilla.mozilla.org/show_bug.cgi?id=1732375
 *
 * If there are multiple tabs selected, the 'Close' entry
 * under the File menu should correctly reflect the number of
 * selected tabs
 */
add_task(async function test_menu_close_tab_count() {
  // Window should have one tab open already, so we
  // just need to add one more to have a total of two
  info("Adding new tabs");
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com");

  info("Selecting all tabs");
  await gBrowser.selectAllTabs();
  is(gBrowser.multiSelectedTabsCount, 2, "Two (2) tabs are selected");

  let fileMenu = document.getElementById("menu_FilePopup");
  await simulateMenuOpen(fileMenu);

  let closeMenuEntry = document.getElementById("menu_close");
  let closeMenuL10nArgsObject = document.l10n.getAttributes(closeMenuEntry);

  is(
    closeMenuL10nArgsObject.args.tabCount,
    2,
    "Menu bar reflects multi-tab selection number (Close 2 Tabs)"
  );

  let onClose = BrowserTestUtils.waitForEvent(
    gBrowser.tabContainer,
    "TabClose"
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  await onClose;

  info("Tabs closed");
});

async function simulateMenuOpen(menu) {
  return new Promise(resolve => {
    menu.addEventListener("popupshown", resolve, { once: true });
    menu.dispatchEvent(new MouseEvent("popupshowing"));
    menu.dispatchEvent(new MouseEvent("popupshown"));
  });
}

async function simulateMenuClosed(menu) {
  return new Promise(resolve => {
    menu.addEventListener("popuphidden", resolve, { once: true });
    menu.dispatchEvent(new MouseEvent("popuphiding"));
    menu.dispatchEvent(new MouseEvent("popuphidden"));
  });
}
