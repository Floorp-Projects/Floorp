"use strict";

// Test that setting the newNewtabExperience.enabled pref to true does not
// set icons in individual tile and card context menus on newtab page.
test_newtab({
  async before({ pushPrefs }) {
    await pushPrefs([
      "browser.newtabpage.activity-stream.newNewtabExperience.enabled",
      true,
    ]);
  },
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
  async after() {
    Services.prefs.clearUserPref(
      "browser.newtabpage.activity-stream.newNewtabExperience.enabled"
    );
  },
});
