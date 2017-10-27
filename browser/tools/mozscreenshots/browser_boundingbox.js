/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

add_task(async function() {
  const scale = window.QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsIDocShell).QueryInterface(Ci.nsIBaseWindow)
                      .devicePixelsPerDesktopPixel;
  let rect = TestRunner._findBoundingBox(["#tabbrowser-tabs"]);
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
  is(rect.width, expectedRight - expectedLeft,
     "Checking _findBoundingBox width calculation");
  // Check height calculation on simple example
  is(rect.height, expectedBottom - expectedTop,
     "Checking _findBoundingBox height caclulation");

  rect = TestRunner._findBoundingBox(["#forward-button", "#TabsToolbar"]);
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
  is(rect.width, expectedRight - expectedLeft,
     "Checking _findBoundingBox union width calculation");
  // Check height calculation on union
  is(rect.height, expectedBottom - expectedTop,
     "Checking _findBoundingBox union height calculation");

  rect = TestRunner._findBoundingBox(["#does_not_exist"]);
  // Check that 'selector not found' returns null
  is(rect, null, "Checking that selector not found returns null");

  rect = TestRunner._findBoundingBox([]);
  // Check that no selectors returns null
  is(rect, null, "Checking that no selectors returns null");
});
