/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

add_task(async function() {
  const scale = window.QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsIDocShell).QueryInterface(Ci.nsIBaseWindow)
                      .devicePixelsPerDesktopPixel;
  let {bounds, rects} = TestRunner._findBoundingBox(["#tabbrowser-tabs"]);
  let element = document.querySelector("#tabbrowser-tabs");
  let tabBar = element.ownerDocument.getBoxObjectFor(element);

  // Calculate expected values
  let expectedLeft = scale * (tabBar.screenX - TestRunner.croppingPadding);
  let expectedTop = scale * (tabBar.screenY - TestRunner.croppingPadding);
  let expectedRight = scale * (tabBar.width + TestRunner.croppingPadding * 2) + expectedLeft;
  let expectedBottom = scale * (tabBar.height + TestRunner.croppingPadding * 2) + expectedTop;

  // Calculate browser region
  let windowLeft = window.screenX * scale;
  let windowTop = window.screenY * scale;
  let windowRight = window.outerWidth * scale + windowLeft;
  let windowBottom = window.outerHeight * scale + windowTop;

  // Adjust values based on browser window
  expectedLeft = Math.max(expectedLeft, windowLeft);
  expectedTop = Math.max(expectedTop, windowTop);
  expectedRight = Math.min(expectedRight, windowRight);
  expectedBottom = Math.min(expectedBottom, windowBottom);
  // Check width calculation on simple example
  is(bounds.width, expectedRight - expectedLeft,
     "Checking _findBoundingBox width calculation");
  // Check height calculation on simple example
  is(bounds.height, expectedBottom - expectedTop,
     "Checking _findBoundingBox height caclulation");
  is(bounds.left, rects[0].left,
    "Checking _findBoundingBox union.left and rect.left is the same for a single selector");
  is(bounds.right, rects[0].right,
    "Checking _findBoundingBox union.right and rect.right is the same for a single selector");
  is(bounds.top, rects[0].top,
    "Checking _findBoundingBox union.top and rect.top is the same for a single selector");
  is(bounds.bottom, rects[0].bottom,
    "Checking _findBoundingBox union.bottom and rect.bottom is the same for a single selector");

  let result = TestRunner._findBoundingBox(["#forward-button", "#TabsToolbar"]);
  bounds = result.bounds;
  rects = result.rects;

  element = document.querySelector("#TabsToolbar");
  let tabToolbar = element.ownerDocument.getBoxObjectFor(element);
  element = document.querySelector("#forward-button");
  let fButton = element.ownerDocument.getBoxObjectFor(element);

  // Calculate expected values
  expectedLeft = scale * (Math.min(tabToolbar.screenX, fButton.screenX)
                              - TestRunner.croppingPadding);
  expectedTop = scale * (Math.min(tabToolbar.screenY, fButton.screenY)
                              - TestRunner.croppingPadding);
  expectedRight = scale * (Math.max(tabToolbar.width + tabToolbar.screenX,
                                    fButton.width + fButton.screenX)
                              + TestRunner.croppingPadding);
  expectedBottom = scale * (Math.max(tabToolbar.height + tabToolbar.screenY,
                                     fButton.height + fButton.screenY)
                              + TestRunner.croppingPadding );

  // Adjust values based on browser window
  expectedLeft = Math.max(expectedLeft, windowLeft);
  expectedTop = Math.max(expectedTop, windowTop);
  expectedRight = Math.min(expectedRight, windowRight);
  expectedBottom = Math.min(expectedBottom, windowBottom);

  // Check width calculation on union
  is(bounds.width, expectedRight - expectedLeft,
     "Checking _findBoundingBox union width calculation");
  // Check height calculation on union
  is(bounds.height, expectedBottom - expectedTop,
     "Checking _findBoundingBox union height calculation");
  // Check single selector's left position
  is(rects[0].left, Math.max(scale * (fButton.screenX - TestRunner.croppingPadding), windowLeft),
    "Checking single selector's left position when _findBoundingBox has multiple selectors");
  // Check single selector's right position
  is(rects[0].right, Math.min(scale * (fButton.width + fButton.screenX + TestRunner.croppingPadding), windowRight),
    "Checking single selector's right position when _findBoundingBox has multiple selectors");
  // Check single selector's top position
  is(rects[0].top, Math.max(scale * (fButton.screenY - TestRunner.croppingPadding), windowTop),
    "Checking single selector's top position when _findBoundingBox has multiple selectors");
  // Check single selector's bottom position
  is(rects[0].bottom, Math.min(scale * (fButton.height + fButton.screenY + TestRunner.croppingPadding), windowBottom),
    "Checking single selector's bottom position when _findBoundingBox has multiple selectors");

    // Check that nonexistent selectors throws an exception
  Assert.throws(() => {
    TestRunner._findBoundingBox(["#does_not_exist"]);
  }, /No element for '#does_not_exist' found/, "Checking that nonexistent selectors throws an exception");

  // Check that no selectors throws an exception
  Assert.throws(() => {
    TestRunner._findBoundingBox([]);
  }, /No selectors specified/, "Checking that no selectors throws an exception");
});
