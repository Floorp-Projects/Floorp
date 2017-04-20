"use strict";

const kOverflowPanel = document.getElementById("widget-overflow");

var gOriginalWidth;
registerCleanupFunction(function*() {
  kOverflowPanel.removeAttribute("animate");
  window.resizeTo(gOriginalWidth, window.outerHeight);
  CustomizableUI.reset();
});

/**
 * This checks that subview-compatible items show up as subviews rather than
 * re-anchored panels. If we ever remove the character encoding widget, please
 * replace this test with another subview - don't remove it.
 */
add_task(async function check_character_encoding_subview_in_overflow() {
  kOverflowPanel.setAttribute("animate", "false");
  gOriginalWidth = window.outerWidth;

  CustomizableUI.addWidgetToArea("developer-button", CustomizableUI.AREA_NAVBAR);

  let navbar = document.getElementById(CustomizableUI.AREA_NAVBAR);
  ok(!navbar.hasAttribute("overflowing"), "Should start with a non-overflowing toolbar.");
  window.resizeTo(400, window.outerHeight);

  await waitForCondition(() => navbar.hasAttribute("overflowing"));

  let chevron = document.getElementById("nav-bar-overflow-button");
  let shownPanelPromise = promisePanelElementShown(window, kOverflowPanel);
  chevron.click();
  await shownPanelPromise;

  let developerView = document.getElementById("PanelUI-developer");
  let button = document.getElementById("developer-button");
  let subviewShownPromise = subviewShown(developerView);
  button.click();
  await subviewShownPromise;
  is(developerView.closest("panel"), kOverflowPanel, "Should be inside the panel");
  kOverflowPanel.hidePopup();
  await Promise.resolve(); // wait for popup to hide fully.

  CustomizableUI.reset();
});

/**
 * This checks that non-subview-compatible items still work correctly.
 * Ideally we should make the downloads panel and bookmarks/library item
 * proper subview items, then this test can go away, and potentially we can
 * simplify some of the subview anchoring code.
 */
add_task(async function check_downloads_panel_in_overflow() {
  let navbar = document.getElementById(CustomizableUI.AREA_NAVBAR);
  ok(navbar.hasAttribute("overflowing"), "Should still be overflowing");
  let chevron = document.getElementById("nav-bar-overflow-button");
  let shownPanelPromise = promisePanelElementShown(window, kOverflowPanel);
  chevron.click();
  await shownPanelPromise;

  let button = document.getElementById("downloads-button");
  button.click();
  await waitForCondition(() => {
    let panel = document.getElementById("downloadsPanel");
    return panel && panel.state != "closed";
  });
  let downloadsPanel = document.getElementById("downloadsPanel");
  isnot(downloadsPanel.state, "closed", "Should be attempting to show the downloads panel.");
  downloadsPanel.hidePopup();
});
