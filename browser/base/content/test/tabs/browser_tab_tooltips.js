// Offset within the tab for the mouse event
const MOUSE_OFFSET = 7;

// Normal tooltips are positioned vertically at least this amount
const MIN_VERTICAL_TOOLTIP_OFFSET = 18;

function openTooltip(node, tooltip) {
  let tooltipShownPromise = BrowserTestUtils.waitForEvent(
    tooltip,
    "popupshown"
  );
  window.windowUtils.disableNonTestMouseEvents(true);
  EventUtils.synthesizeMouse(node, 2, 2, { type: "mouseover" });
  EventUtils.synthesizeMouse(node, 4, 4, { type: "mousemove" });
  EventUtils.synthesizeMouse(node, MOUSE_OFFSET, MOUSE_OFFSET, {
    type: "mousemove",
  });
  EventUtils.synthesizeMouse(node, 2, 2, { type: "mouseout" });
  window.windowUtils.disableNonTestMouseEvents(false);
  return tooltipShownPromise;
}

function closeTooltip(node, tooltip) {
  let tooltipHiddenPromise = BrowserTestUtils.waitForEvent(
    tooltip,
    "popuphidden"
  );
  EventUtils.synthesizeMouse(document.documentElement, 2, 2, {
    type: "mousemove",
  });
  return tooltipHiddenPromise;
}

// This test verifies that the tab tooltip appears at the correct location, aligned
// with the bottom of the tab, and that the tooltip appears near the close button.
add_task(async function () {
  const tabUrl =
    "data:text/html,<html><head><title>A Tab</title></head><body>Hello</body></html>";
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl);

  let tooltip = document.getElementById("tabbrowser-tab-tooltip");
  await openTooltip(tab, tooltip);

  let tabRect = tab.getBoundingClientRect();
  let tooltipRect = tooltip.getBoundingClientRect();

  isfuzzy(
    tooltipRect.left,
    tabRect.left + MOUSE_OFFSET,
    1,
    "tooltip left position for tab"
  );
  ok(
    tooltipRect.top >= tabRect.top + MIN_VERTICAL_TOOLTIP_OFFSET + MOUSE_OFFSET,
    "tooltip top position for tab"
  );
  is(
    tooltip.getAttribute("position"),
    "",
    "tooltip position attribute for tab"
  );

  await closeTooltip(tab, tooltip);

  await openTooltip(tab.closeButton, tooltip);

  let closeButtonRect = tab.closeButton.getBoundingClientRect();
  tooltipRect = tooltip.getBoundingClientRect();

  isfuzzy(
    tooltipRect.left,
    closeButtonRect.left + MOUSE_OFFSET,
    1,
    "tooltip left position for close button"
  );
  ok(
    tooltipRect.top >
      closeButtonRect.top + MIN_VERTICAL_TOOLTIP_OFFSET + MOUSE_OFFSET,
    "tooltip top position for close button"
  );
  ok(
    !tooltip.hasAttribute("position"),
    "tooltip position attribute for close button"
  );

  await closeTooltip(tab, tooltip);

  BrowserTestUtils.removeTab(tab);
});

// This test verifies that a mouse wheel closes the tooltip.
add_task(async function () {
  const tabUrl =
    "data:text/html,<html><head><title>A Tab</title></head><body>Hello</body></html>";
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl);

  let tooltip = document.getElementById("tabbrowser-tab-tooltip");
  await openTooltip(tab, tooltip);

  EventUtils.synthesizeWheel(tab, 4, 4, {
    deltaMode: WheelEvent.DOM_DELTA_LINE,
    deltaY: 1.0,
  });

  is(tooltip.state, "closed", "wheel event closed the tooltip");

  BrowserTestUtils.removeTab(tab);
});

// This test verifies that the tooltip in the tab manager panel matches the
// tooltip in the tab strip.
add_task(async function () {
  // Open a new tab
  const tabUrl = "https://example.com";
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl);

  // Make the popup of allTabs showing up
  gTabsPanel.init();
  let allTabsView = document.getElementById("allTabsMenu-allTabsView");
  let allTabsPopupShownPromise = BrowserTestUtils.waitForEvent(
    allTabsView,
    "ViewShown"
  );
  gTabsPanel.showAllTabsPanel();
  await allTabsPopupShownPromise;

  // Get tooltips and compare them
  let tabInPanel = Array.from(
    gTabsPanel.allTabsViewTabs.querySelectorAll(".all-tabs-button")
  ).at(-1).label;
  let tabInTabStrip = tab.getAttribute("label");

  is(
    tabInPanel,
    tabInTabStrip,
    "Tooltip in tab manager panel matches tooltip in tab strip"
  );

  // Close everything
  let allTabsPopupHiddenPromise = BrowserTestUtils.waitForEvent(
    allTabsView.panelMultiView,
    "PanelMultiViewHidden"
  );
  gTabsPanel.hideAllTabsPanel();
  await allTabsPopupHiddenPromise;

  BrowserTestUtils.removeTab(tab);
});
