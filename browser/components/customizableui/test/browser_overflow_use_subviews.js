"use strict";

const kOverflowPanel = document.getElementById("widget-overflow");

var gOriginalWidth;
async function stopOverflowing() {
  kOverflowPanel.removeAttribute("animate");
  window.resizeTo(gOriginalWidth, window.outerHeight);
  await waitForCondition(
    () => !document.getElementById("nav-bar").hasAttribute("overflowing")
  );
  CustomizableUI.reset();
}

registerCleanupFunction(stopOverflowing);

/**
 * This checks that subview-compatible items show up as subviews rather than
 * re-anchored panels. If we ever remove the library widget, please
 * replace this test with another subview - don't remove it.
 */
add_task(async function check_library_subview_in_overflow() {
  kOverflowPanel.setAttribute("animate", "false");
  gOriginalWidth = window.outerWidth;

  CustomizableUI.addWidgetToArea("library-button", CustomizableUI.AREA_NAVBAR);

  let navbar = document.getElementById(CustomizableUI.AREA_NAVBAR);
  ok(
    !navbar.hasAttribute("overflowing"),
    "Should start with a non-overflowing toolbar."
  );
  window.resizeTo(kForceOverflowWidthPx, window.outerHeight);

  await waitForCondition(() => navbar.hasAttribute("overflowing"));

  let chevron = document.getElementById("nav-bar-overflow-button");
  let shownPanelPromise = BrowserTestUtils.waitForEvent(
    kOverflowPanel,
    "ViewShown"
  );
  chevron.click();
  await shownPanelPromise;

  let button = document.getElementById("library-button");
  button.click();

  let libraryView = document.getElementById("appMenu-libraryView");
  await BrowserTestUtils.waitForEvent(libraryView, "ViewShown");
  let hasSubviews = !!kOverflowPanel.querySelector("panelmultiview");
  let expectedPanel = hasSubviews
    ? kOverflowPanel
    : document.getElementById("customizationui-widget-panel");
  is(libraryView.closest("panel"), expectedPanel, "Should be inside the panel");
  expectedPanel.hidePopup();
  await Promise.resolve(); // wait for popup to hide fully.
  await stopOverflowing();
});

/**
 * This checks that non-subview-compatible items still work correctly.
 * Ideally we should make the downloads panel and bookmarks/library item
 * proper subview items, then this test can go away, and potentially we can
 * simplify some of the subview anchoring code.
 */
add_task(async function check_downloads_panel_in_overflow() {
  let button = document.getElementById("downloads-button");
  await gCustomizeMode.addToPanel(button);
  await waitForOverflowButtonShown();

  let chevron = document.getElementById("nav-bar-overflow-button");
  let shownPanelPromise = promisePanelElementShown(window, kOverflowPanel);
  chevron.click();
  await shownPanelPromise;

  button.click();
  await waitForCondition(() => {
    let panel = document.getElementById("downloadsPanel");
    return panel && panel.state != "closed";
  });
  let downloadsPanel = document.getElementById("downloadsPanel");
  isnot(
    downloadsPanel.state,
    "closed",
    "Should be attempting to show the downloads panel."
  );
  downloadsPanel.hidePopup();
});
