/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_removing_button_should_close_tab() {
  registerCleanupFunction(() => CustomizableUI.reset());
  await withFirefoxView({}, async browser => {
    let win = browser.ownerGlobal;
    let tab = browser.getTabBrowser().getTabForBrowser(browser);
    let button = win.document.getElementById("firefox-view-button");
    await win.gCustomizeMode.removeFromArea(button, "toolbar-context-menu");
    ok(!tab.isConnected, "Tab should have been removed.");
    isnot(win.gBrowser.selectedTab, tab, "A different tab should be selected.");
  });
});
