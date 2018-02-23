"use strict";

// Check TopSites edit modal and overlay show up.
test_newtab(
  // it should be able to click the topsites add button to reveal the add top site modal and overlay.
  function topsites_edit() {
    const topsitesAddBtn = content.document.querySelector(".add-topsites-button button");
    topsitesAddBtn.click();

    let found = content.document.querySelector(".topsite-form");
    ok(found && !found.hidden, "Should find a visible topsite form");

    found = content.document.querySelector(".modal-overlay");
    ok(found && !found.hidden, "Should find a visible overlay");
  }
);

// Test pin/unpin context menu options.
test_newtab({
  before: setDefaultTopSites,
  // it should pin the website when we click the first option of the topsite context menu.
  test: async function topsites_pin_unpin() {
    await ContentTaskUtils.waitForCondition(() => content.document.querySelector(".top-site-icon"),
      "Topsite tippytop icon not found");
    // There are only topsites on the page, the selector with find the first topsite menu button.
    const topsiteContextBtn = content.document.querySelector(".context-menu-button");
    topsiteContextBtn.click();

    const contextMenu = content.document.querySelector(".context-menu");
    ok(contextMenu && !contextMenu.hidden, "Should find a visible topsite context menu");

    const pinUnpinTopsiteBtn = contextMenu.querySelector(".context-menu-item a");
    // Pin the topsite.
    pinUnpinTopsiteBtn.click();

    // Need to wait for pin action.
    await ContentTaskUtils.waitForCondition(() => content.document.querySelector(".icon-pin-small"),
      "No pinned icon found");

    let pinnedIcon = content.document.querySelectorAll(".icon-pin-small").length;
    is(pinnedIcon, 1, "should find 1 pinned topsite");

    // Unpin the topsite.
    pinUnpinTopsiteBtn.click();

    // Need to wait for unpin action.
    await ContentTaskUtils.waitForCondition(() => !content.document.querySelector(".icon-pin-small"),
      "Topsite should be unpinned");
  }
});
