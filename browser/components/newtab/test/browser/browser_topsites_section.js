"use strict";

// Check TopSites edit modal and overlay show up.
test_newtab(
  // it should be able to click the topsites add button to reveal the add top site modal and overlay.
  function topsites_edit() {
    // Open the section context menu.
    content.document.querySelector(".top-sites .section-top-bar .context-menu-button").click();
    let contextMenu = content.document.querySelector(".top-sites .section-top-bar .context-menu");
    ok(contextMenu, "Should find a visible topsite context menu");

    const topsitesAddBtn = content.document.querySelector(".top-sites .context-menu-item a");
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
    let topsiteContextBtn = content.document.querySelector(".top-sites-list .context-menu-button");
    topsiteContextBtn.click();

    await ContentTaskUtils.waitForCondition(() => content.document.querySelector(".top-sites-list .context-menu"),
      "No context menu found");

    let contextMenu = content.document.querySelector(".top-sites-list .context-menu");
    ok(contextMenu, "Should find a topsite context menu");

    const pinUnpinTopsiteBtn = contextMenu.querySelector(".top-sites .context-menu-item a");
    // Pin the topsite.
    pinUnpinTopsiteBtn.click();

    // Need to wait for pin action.
    await ContentTaskUtils.waitForCondition(() => content.document.querySelector(".icon-pin-small"),
      "No pinned icon found");

    let pinnedIcon = content.document.querySelectorAll(".icon-pin-small").length;
    is(pinnedIcon, 1, "should find 1 pinned topsite");

    // Unpin the topsite.
    topsiteContextBtn = content.document.querySelector(".top-sites-list .context-menu-button");
    ok(topsiteContextBtn, "Should find a context menu button");
    topsiteContextBtn.click();
    content.document.querySelector(".top-sites-list .context-menu-item a").click();

    // Need to wait for unpin action.
    await ContentTaskUtils.waitForCondition(() => !content.document.querySelector(".icon-pin-small"),
      "Topsite should be unpinned");
  }
});

// Check Topsites add
test_newtab({
  // it should be able to click the topsites edit button to reveal the edit topsites modal and overlay.
  test: async function topsites_add() {
    let nativeInputValueSetter = Object.getOwnPropertyDescriptor(content.window.HTMLInputElement.prototype, "value").set;
    let event = new content.Event("input", {bubbles: true});

    // Find the add topsites button
    content.document.querySelector(".top-sites .section-top-bar .context-menu-button").click();
    let contextMenu = content.document.querySelector(".top-sites .section-top-bar .context-menu");
    ok(contextMenu, "Should find a visible topsite context menu");

    const topsitesAddBtn = content.document.querySelector(".top-sites .context-menu-item a");
    topsitesAddBtn.click();

    let found = content.document.querySelector(".modal-overlay");
    ok(found && !found.hidden, "Should find a visible overlay");

    // Write field title
    let fieldTitle = content.document.querySelector(".field input");
    ok(fieldTitle && !fieldTitle.hidden, "Should find field title input");

    nativeInputValueSetter.call(fieldTitle, "Bugzilla");
    fieldTitle.dispatchEvent(event);
    is(fieldTitle.value, "Bugzilla", "The field title should match");

    // Write field url
    let fieldURL = content.document.querySelector(".field.url input");
    ok(fieldURL && !fieldURL.hidden, "Should find field url input");

    nativeInputValueSetter.call(fieldURL, "https://bugzilla.mozilla.org");
    fieldURL.dispatchEvent(event);
    is(fieldURL.value, "https://bugzilla.mozilla.org", "The field url should match");

    // Click the "Add" button
    let addBtn = content.document.querySelector(".done");
    addBtn.click();

    // Wait for Topsite to be populated
    await ContentTaskUtils.waitForCondition(() => (content.document.querySelector(".top-site-outer:first-child a").getAttribute("href") === "https://bugzilla.mozilla.org"), "No Topsite found");

    // Remove topsite after test is complete
    let topsiteContextBtn = content.document.querySelector(".top-sites-list .context-menu-button");
    topsiteContextBtn.click();
    await ContentTaskUtils.waitForCondition(() => content.document.querySelector(".top-sites-list .context-menu"),
      "No context menu found");

    let contextMen = content.document.querySelector(".top-sites-list .context-menu");

    const dismissBtn = contextMen.querySelector(".top-sites .context-menu-item a .icon-dismiss");
    dismissBtn.click();

    // Wait for Topsite to be removed
    await ContentTaskUtils.waitForCondition(() => (content.document.querySelector(".top-site-outer:first-child a").getAttribute("href") !== "https://bugzilla.mozilla.org"), "Topsite not removed");
  }
});
