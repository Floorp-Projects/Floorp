/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function test () {
  waitForExplicitFinish();
  window.maximize();

  // Find where the nav-bar is vertically.
  var navBar = document.getElementById("nav-bar");
  var boundingRect = navBar.getBoundingClientRect();
  var yPixel = boundingRect.top + Math.floor(boundingRect.height / 2);
  var xPixel = boundingRect.width - 1; // Use the last pixel of the screen since it is maximized.

  function onPopupHidden() {
    PanelUI.panel.removeEventListener("popuphidden", onPopupHidden);
    window.restore();
    finish();
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
}
