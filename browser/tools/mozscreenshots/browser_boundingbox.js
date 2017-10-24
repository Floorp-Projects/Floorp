/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

add_task(async function() {
  let rect = TestRunner._findBoundingBox(["#tabbrowser-tabs"]);
  let tabBar = document.querySelector("#tabbrowser-tabs").getBoundingClientRect();
  // Check width calculation on simple example
  is(rect.width, tabBar.width + TestRunner.croppingPadding * 2,
     "Checking _findBoundingBox width calculation");
  // Check height calculation on simple example
  is(rect.height, tabBar.height + TestRunner.croppingPadding * 2,
     "Checking _findBoundingBox height caclulation");

  rect = TestRunner._findBoundingBox(["#forward-button", "#TabsToolbar"]);
  let tabToolbar = document.querySelector("#TabsToolbar").getBoundingClientRect();
  let fButton = document.querySelector("#forward-button").getBoundingClientRect();
  let width = Math.max(tabToolbar.right, fButton.right) - Math.min(tabToolbar.left, fButton.left);
  let height = Math.max(tabToolbar.bottom, fButton.bottom) - Math.min(tabToolbar.top, fButton.top);
  // Check width calculation on union
  is(rect.width, width + TestRunner.croppingPadding * 2,
     "Checking _findBoundingBox union width calculation");
  // Check height calculation on union
  is(rect.height, height + TestRunner.croppingPadding * 2,
     "Checking _findBoundingBox union height calculation");

  rect = TestRunner._findBoundingBox(["#does_not_exist"]);
  // Check that 'selector not found' returns null
  is(rect, null, "Checking that selector not found returns null");

  rect = TestRunner._findBoundingBox([]);
  // Check that no selectors returns null
  is(rect, null, "Checking that no selectors returns null");
});
