/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// NB: This is testing what happens if something that /isn't/ a customizable
// widget gets used in CustomizableUI APIs. Don't use this as an example of
// what should happen in a "normal" case or how you should use the API.
function test() {
  // First create a button that isn't customizable, and add it in the nav-bar,
  // but not in the customizable part of it (the customization target) but
  // next to the main (hamburger) menu button.
  const buttonID = "Test-non-widget-non-removable-button";
  let btn = document.createElement("toolbarbutton");
  btn.id = buttonID;
  btn.label = "Hi";
  btn.setAttribute("style", "width: 20px; height: 20px; background-color: red");
  document.getElementById("nav-bar").appendChild(btn);
  registerCleanupFunction(function() {
    btn.remove();
  });

  // Now try to add this non-customizable button to the tabstrip. This will
  // update the internal bookkeeping (ie placements) information, but shouldn't
  // move the node.
  CustomizableUI.addWidgetToArea(buttonID, CustomizableUI.AREA_TABSTRIP);
  let placement = CustomizableUI.getPlacementOfWidget(buttonID);
  // Check our bookkeeping
  ok(placement, "Button should be placed");
  is(placement && placement.area, CustomizableUI.AREA_TABSTRIP, "Should be placed on tabstrip.");
  // Check we didn't move the node.
  is(btn.parentNode && btn.parentNode.id, "nav-bar", "Actual button should still be on navbar.");

  // Now remove the node again. This should remove the bookkeeping, but again
  // not affect the actual node.
  CustomizableUI.removeWidgetFromArea(buttonID);
  placement = CustomizableUI.getPlacementOfWidget(buttonID);
  // Check our bookkeeping:
  ok(!placement, "Button should no longer have a placement.");
  // Check our node.
  is(btn.parentNode && btn.parentNode.id, "nav-bar", "Actual button should still be on navbar.");
}

