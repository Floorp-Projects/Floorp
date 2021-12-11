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
add_task(async function() {
  for (let useProtonTooltips of [false, true]) {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.proton.places-tooltip.enabled", useProtonTooltips]],
    });

    const tabUrl =
      "data:text/html,<html><head><title>A Tab</title></head><body>Hello</body></html>";
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl);

    let tooltip = document.getElementById("tabbrowser-tab-tooltip");
    await openTooltip(tab, tooltip);

    let tabRect = tab.getBoundingClientRect();
    let tooltipRect = tooltip.getBoundingClientRect();

    if (useProtonTooltips) {
      // Add the margin when using photon tooltips, but not when that is disabled.
      isfuzzy(
        tooltipRect.left,
        tabRect.left + parseInt(getComputedStyle(tooltip).marginLeft),
        1,
        "tooltip left position for tab"
      );
      isfuzzy(
        tooltipRect.top,
        tabRect.bottom + parseInt(getComputedStyle(tooltip).marginTop),
        1,
        "tooltip top position for tab"
      );
    } else {
      isfuzzy(
        tooltipRect.left,
        tabRect.left + MOUSE_OFFSET,
        1,
        "tooltip left position for tab"
      );
      ok(
        tooltipRect.top >=
          tabRect.top + MIN_VERTICAL_TOOLTIP_OFFSET + MOUSE_OFFSET,
        "tooltip top position for tab"
      );
    }
    is(
      tooltip.getAttribute("position"),
      useProtonTooltips ? "after_start" : "",
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
  }
});

// This test verifies that a mouse wheel closes the tooltip.
add_task(async function() {
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
