/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

function getNavBarEndPosition() {
  let navBar = document.getElementById("nav-bar");
  let boundingRect = navBar.getBoundingClientRect();

  // Find where the nav-bar is vertically.
  let y = boundingRect.top + Math.floor(boundingRect.height / 2);
  // Use the last pixel of the screen since it is maximized.
  let x = boundingRect.width - 1;
  return { x, y };
}

/**
 * Clicking the right end of a maximized window should open the hamburger menu.
 */
add_task(async function test_clicking_hamburger_edge_fitts() {
  if (window.windowState != window.STATE_MAXIMIZED) {
    info(`Waiting for maximize, current state: ${window.windowState}`);
    let resizeDone = BrowserTestUtils.waitForEvent(
      window,
      "resize",
      false,
      () => window.outerWidth >= screen.width - 1
    );
    let maximizeDone = BrowserTestUtils.waitForEvent(window, "sizemodechange");
    window.maximize();
    await maximizeDone;
    await resizeDone;
  }

  is(window.windowState, window.STATE_MAXIMIZED, "should be maximized");

  let { x, y } = getNavBarEndPosition();
  info(`Clicking in ${x}, ${y}`);

  let popupHiddenResolve;
  let popupHiddenPromise = new Promise(resolve => {
    popupHiddenResolve = resolve;
  });
  async function onPopupHidden() {
    PanelUI.panel.removeEventListener("popuphidden", onPopupHidden);

    info("Waiting for restore");

    let restoreDone = BrowserTestUtils.waitForEvent(window, "sizemodechange");
    window.restore();
    await restoreDone;

    popupHiddenResolve();
  }
  function onPopupShown() {
    PanelUI.panel.removeEventListener("popupshown", onPopupShown);
    ok(true, "Clicking at the far edge of the window opened the menu popup.");
    PanelUI.panel.addEventListener("popuphidden", onPopupHidden);
    PanelUI.hide();
  }
  registerCleanupFunction(function() {
    PanelUI.panel.removeEventListener("popupshown", onPopupShown);
    PanelUI.panel.removeEventListener("popuphidden", onPopupHidden);
  });
  PanelUI.panel.addEventListener("popupshown", onPopupShown);
  EventUtils.synthesizeMouseAtPoint(x, y, {}, window);
  await popupHiddenPromise;
});
