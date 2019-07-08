"use strict";

const TEST_PAGE =
  getRootDirectory(gTestPath) + "file_view_image_data_navigation.html";

add_task(async function test_principal_right_click_open_link_in_new_tab() {
  await SpecialPowers.pushPrefEnv({
    set: [["security.data_uri.block_toplevel_data_uri_navigations", true]],
  });

  await BrowserTestUtils.withNewTab(TEST_PAGE, async function(browser) {
    let loadPromise = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser,
      true
    );

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

    await ContentTask.spawn(gBrowser.selectedBrowser, {}, async function() {
      ok(
        content.document.location.toString().startsWith("data:image/svg+xml;"),
        "data:image/svg navigation allowed through right-click view-image"
      );
    });
  });
});
