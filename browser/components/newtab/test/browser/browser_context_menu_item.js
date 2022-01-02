"use strict";

// Test that we do not set icons in individual tile and card context menus on
// newtab page.
test_newtab({
  test: async function test_contextMenuIcons() {
    const siteSelector = ".top-sites-list:not(.search-shortcut, .placeholder)";
    await ContentTaskUtils.waitForCondition(
      () => content.document.querySelector(siteSelector),
      "Topsites have loaded"
    );
    const contextMenuItems = await content.openContextMenuAndGetOptions(
      siteSelector
    );
    let icon = contextMenuItems[0].querySelector(".icon");
    ok(!icon, "icon was not rendered");
  },
});
