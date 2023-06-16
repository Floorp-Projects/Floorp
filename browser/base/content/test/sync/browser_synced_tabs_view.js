/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function promiseLayout() {
  // Wait for layout to have happened.
  return new Promise(resolve =>
    requestAnimationFrame(() => requestAnimationFrame(resolve))
  );
}

add_setup(async function () {
  registerCleanupFunction(() => CustomizableUI.reset());
});

async function withOpenSyncPanel(cb) {
  let promise = BrowserTestUtils.waitForEvent(
    window,
    "ViewShown",
    true,
    e => e.target.id == "PanelUI-remotetabs"
  ).then(e => e.target.closest("panel"));

  let panel;
  try {
    gSync.openSyncedTabsPanel();
    panel = await promise;
    is(panel.state, "open", "Panel should have opened.");
    await cb(panel);
  } finally {
    panel?.hidePopup();
  }
}

add_task(async function test_button_in_bookmarks_toolbar() {
  CustomizableUI.addWidgetToArea("sync-button", CustomizableUI.AREA_BOOKMARKS);
  CustomizableUI.setToolbarVisibility(CustomizableUI.AREA_BOOKMARKS, "never");
  await promiseLayout();

  await withOpenSyncPanel(async panel => {
    is(
      panel.anchorNode.closest("toolbarbutton"),
      PanelUI.menuButton,
      "Should have anchored on the menu button because the sync button isn't visible."
    );
  });
});

add_task(async function test_button_in_navbar() {
  CustomizableUI.addWidgetToArea("sync-button", CustomizableUI.AREA_NAVBAR, 0);
  await promiseLayout();
  await withOpenSyncPanel(async panel => {
    is(
      panel.anchorNode.closest("toolbarbutton").id,
      "sync-button",
      "Should have anchored on the sync button itself."
    );
  });
});

add_task(async function test_button_in_overflow() {
  CustomizableUI.addWidgetToArea(
    "sync-button",
    CustomizableUI.AREA_FIXED_OVERFLOW_PANEL,
    0
  );
  await promiseLayout();
  await withOpenSyncPanel(async panel => {
    is(
      panel.anchorNode.closest("toolbarbutton").id,
      "nav-bar-overflow-button",
      "Should have anchored on the overflow button."
    );
  });
});
