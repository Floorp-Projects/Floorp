/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This test checks that context menus are in touchmode
 * when opened through a touch event (long tap). */

async function openAndCheckContextMenu(contextMenu, target) {
  is(contextMenu.state, "closed", "Context menu is initally closed.");

  let popupshown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  EventUtils.synthesizeNativeTapAtCenter(target, true);
  await popupshown;

  is(contextMenu.state, "open", "Context menu is open.");
  is(
    contextMenu.getAttribute("touchmode"),
    "true",
    "Context menu is in touchmode."
  );

  contextMenu.hidePopup();

  popupshown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(target, { type: "contextmenu" });
  await popupshown;

  is(contextMenu.state, "open", "Context menu is open.");
  ok(
    !contextMenu.hasAttribute("touchmode"),
    "Context menu is not in touchmode."
  );

  contextMenu.hidePopup();
}

// Ensure that we can run touch events properly for windows [10]
add_setup(async function () {
  let isWindows = AppConstants.isPlatformAndVersionAtLeast("win", "10.0");
  await SpecialPowers.pushPrefEnv({
    set: [["apz.test.fails_with_native_injection", isWindows]],
  });
});

// Test the content area context menu.
add_task(async function test_contentarea_contextmenu_touch() {
  await BrowserTestUtils.withNewTab("about:blank", async function (browser) {
    let contextMenu = document.getElementById("contentAreaContextMenu");
    await openAndCheckContextMenu(contextMenu, browser);
  });
});

// Test the back and forward buttons.
add_task(async function test_back_forward_button_contextmenu_touch() {
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  await BrowserTestUtils.withNewTab(
    "http://example.com",
    async function (browser) {
      let contextMenu = document.getElementById("backForwardMenu");

      let backbutton = document.getElementById("back-button");
      let notDisabled = TestUtils.waitForCondition(
        () => !backbutton.hasAttribute("disabled")
      );
      // eslint-disable-next-line @microsoft/sdl/no-insecure-url
      BrowserTestUtils.startLoadingURIString(browser, "http://example.org");
      await notDisabled;
      await openAndCheckContextMenu(contextMenu, backbutton);

      let forwardbutton = document.getElementById("forward-button");
      notDisabled = TestUtils.waitForCondition(
        () => !forwardbutton.hasAttribute("disabled")
      );
      backbutton.click();
      await notDisabled;
      await openAndCheckContextMenu(contextMenu, forwardbutton);
    }
  );
});

// Test the toolbar context menu.
add_task(async function test_toolbar_contextmenu_touch() {
  let toolbarContextMenu = document.getElementById("toolbar-context-menu");
  let target = document.getElementById("PanelUI-menu-button");
  await openAndCheckContextMenu(toolbarContextMenu, target);
});

// Test the urlbar input context menu.
add_task(async function test_urlbar_contextmenu_touch() {
  let urlbar = document.getElementById("urlbar");
  let textBox = urlbar.querySelector("moz-input-box");
  let menu = textBox.menupopup;
  await openAndCheckContextMenu(menu, textBox);
});
