"use strict";

const kOverflowPanel = document.getElementById("widget-overflow");

var gOriginalWidth;
registerCleanupFunction(async function() {
  kOverflowPanel.removeAttribute("animate");
  window.resizeTo(gOriginalWidth, window.outerHeight);
  await waitForCondition(() => !document.getElementById("nav-bar").hasAttribute("overflowing"));
  CustomizableUI.reset();
});

/**
 * This checks that subview-compatible items show up as subviews rather than
 * re-anchored panels. If we ever remove the developer widget, please
 * replace this test with another subview - don't remove it.
 */
add_task(async function check_developer_subview_in_overflow() {
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
  let hasSubviews = !!kOverflowPanel.querySelector("photonpanelmultiview,panelmultiview");
  let expectedPanel = hasSubviews ? kOverflowPanel : document.getElementById("customizationui-widget-panel");
  is(developerView.closest("panel"), expectedPanel, "Should be inside the panel");
  expectedPanel.hidePopup();
  await Promise.resolve(); // wait for popup to hide fully.
});
