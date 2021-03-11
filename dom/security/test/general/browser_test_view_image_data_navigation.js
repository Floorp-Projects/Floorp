"use strict";

add_task(async function test_principal_right_click_open_link_in_new_tab() {
  await SpecialPowers.pushPrefEnv({
    set: [["security.data_uri.block_toplevel_data_uri_navigations", true]],
  });

  const TEST_PAGE =
    getRootDirectory(gTestPath) + "file_view_image_data_navigation.html";

  await BrowserTestUtils.withNewTab(TEST_PAGE, async function(browser) {
    let loadPromise = BrowserTestUtils.waitForNewTab(gBrowser, null, true);

    // simulate right-click->view-image
    BrowserTestUtils.waitForEvent(document, "popupshown", false, event => {
      // These are operations that must be executed synchronously with the event.
      document.getElementById("context-viewimage").doCommand();
      event.target.hidePopup();
      return true;
    });
    BrowserTestUtils.synthesizeMouseAtCenter(
      "#testimage",
      { type: "contextmenu", button: 2 },
      gBrowser.selectedBrowser
    );
    await loadPromise;

    let spec = gBrowser.selectedBrowser.currentURI.spec;
    ok(
      spec.startsWith("data:image/svg+xml;"),
      "data:image/svg navigation allowed through right-click view-image"
    );

    gBrowser.removeCurrentTab();
  });
});

add_task(async function test_right_click_open_bg_image() {
  await SpecialPowers.pushPrefEnv({
    set: [["security.data_uri.block_toplevel_data_uri_navigations", true]],
  });

  const TEST_PAGE =
    getRootDirectory(gTestPath) + "file_view_bg_image_data_navigation.html";

  await BrowserTestUtils.withNewTab(TEST_PAGE, async function(browser) {
    let loadPromise = BrowserTestUtils.waitForNewTab(gBrowser, null, true);

    // simulate right-click->view-image
    BrowserTestUtils.waitForEvent(document, "popupshown", false, event => {
      // These are operations that must be executed synchronously with the event.
      document.getElementById("context-viewimage").doCommand();
      event.target.hidePopup();
      return true;
    });
    BrowserTestUtils.synthesizeMouseAtCenter(
      "#testbody",
      { type: "contextmenu", button: 2 },
      gBrowser.selectedBrowser
    );
    await loadPromise;

    let spec = gBrowser.selectedBrowser.currentURI.spec;
    ok(
      spec.startsWith("data:image/svg+xml;"),
      "data:image/svg navigation allowed through right-click view-image with background image"
    );

    gBrowser.removeCurrentTab();
  });
});
