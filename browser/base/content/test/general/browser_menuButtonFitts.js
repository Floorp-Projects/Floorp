/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

/**
 * Clicking the right end of a maximized window should open the hamburger menu.
 */
add_task(async function test_clicking_hamburger_edge_fitts() {
  let oldWidth = window.outerWidth;
  let maximizeDone = BrowserTestUtils.waitForEvent(
    window,
    "resize",
    false,
    () => window.outerWidth >= screen.width - 1
  );
  window.maximize();
  // Ensure we actually finish the resize before continuing.
  await maximizeDone;

  // Find where the nav-bar is vertically.
  var navBar = document.getElementById("nav-bar");
  var boundingRect = navBar.getBoundingClientRect();
  var yPixel = boundingRect.top + Math.floor(boundingRect.height / 2);
  var xPixel = boundingRect.width - 1; // Use the last pixel of the screen since it is maximized.

  let popupHiddenResolve;
  let popupHiddenPromise = new Promise(resolve => {
    popupHiddenResolve = resolve;
  });
  async function onPopupHidden() {
    PanelUI.panel.removeEventListener("popuphidden", onPopupHidden);
    let restoreDone = BrowserTestUtils.waitForEvent(
      window,
      "resize",
      false,
      () => {
        let w = window.outerWidth;
        return w > oldWidth - 5 && w < oldWidth + 5;
      }
    );
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
  EventUtils.synthesizeMouseAtPoint(xPixel, yPixel, {}, window);
  await popupHiddenPromise;
});
