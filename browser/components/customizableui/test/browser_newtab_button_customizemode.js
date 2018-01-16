/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests in this file check that user customizations to the tabstrip show
 * the correct type of new tab button while the tabstrip isn't overflowing.
 */

const kGlobalNewTabButton = document.getElementById("new-tab-button");
const kInnerNewTabButton = document.getAnonymousElementByAttribute(gBrowser.tabContainer, "anonid", "tabs-newtab-button");

function assertNewTabButton(which) {
  if (which == "global") {
    isnot(kGlobalNewTabButton.getBoundingClientRect().width, 0,
      "main new tab button should be visible");
    is(kInnerNewTabButton.getBoundingClientRect().width, 0,
      "inner new tab button should be hidden");
  } else if (which == "inner") {
    is(kGlobalNewTabButton.getBoundingClientRect().width, 0,
      "main new tab button should be hidden");
    isnot(kInnerNewTabButton.getBoundingClientRect().width, 0,
      "inner new tab button should be visible");
  } else {
    ok(false, "Unexpected button: " + which);
  }
}

/**
 * Add and remove items *after* the new tab button in customize mode.
 */
add_task(async function addremove_after_newtab_customizemode() {
  await startCustomizing();
  await waitForElementShown(kGlobalNewTabButton);
  simulateItemDrag(document.getElementById("home-button"), kGlobalNewTabButton, "end");
  ok(gBrowser.tabContainer.hasAttribute("hasadjacentnewtabbutton"),
    "tabs should have the adjacent newtab attribute");
  await endCustomizing();
  assertNewTabButton("inner");

  await startCustomizing();
  let dropTarget = document.getElementById("stop-reload-button");
  await waitForElementShown(dropTarget);
  simulateItemDrag(document.getElementById("home-button"), dropTarget, "end");
  ok(gBrowser.tabContainer.hasAttribute("hasadjacentnewtabbutton"),
    "tabs should still have the adjacent newtab attribute");
  await endCustomizing();
  assertNewTabButton("inner");
  ok(CustomizableUI.inDefaultState, "Should be in default state");
});

/**
 * Add and remove items *before* the new tab button in customize mode.
 */
add_task(async function addremove_before_newtab_customizemode() {
  await startCustomizing();
  await waitForElementShown(kGlobalNewTabButton);
  simulateItemDrag(document.getElementById("home-button"), kGlobalNewTabButton, "start");
  ok(!gBrowser.tabContainer.hasAttribute("hasadjacentnewtabbutton"),
    "tabs should no longer have the adjacent newtab attribute");
  await endCustomizing();
  assertNewTabButton("global");
  await startCustomizing();
  let dropTarget = document.getElementById("stop-reload-button");
  await waitForElementShown(dropTarget);
  simulateItemDrag(document.getElementById("home-button"), dropTarget, "end");
  ok(gBrowser.tabContainer.hasAttribute("hasadjacentnewtabbutton"),
    "tabs should have the adjacent newtab attribute again");
  await endCustomizing();
  assertNewTabButton("inner");
  ok(CustomizableUI.inDefaultState, "Should be in default state");
});

/**
 * Add and remove items *after* the new tab button outside of customize mode.
 */
add_task(async function addremove_after_newtab_api() {
  CustomizableUI.addWidgetToArea("home-button", "TabsToolbar");
  ok(gBrowser.tabContainer.hasAttribute("hasadjacentnewtabbutton"),
    "tabs should have the adjacent newtab attribute");
  assertNewTabButton("inner");

  CustomizableUI.reset();
  ok(gBrowser.tabContainer.hasAttribute("hasadjacentnewtabbutton"),
    "tabs should still have the adjacent newtab attribute");
  assertNewTabButton("inner");
  ok(CustomizableUI.inDefaultState, "Should be in default state");

});

/**
 * Add and remove items *before* the new tab button outside of customize mode.
 */
add_task(async function addremove_before_newtab_api() {
  let index = CustomizableUI.getWidgetIdsInArea("TabsToolbar").indexOf("new-tab-button");
  CustomizableUI.addWidgetToArea("home-button", "TabsToolbar", index);
  ok(!gBrowser.tabContainer.hasAttribute("hasadjacentnewtabbutton"),
    "tabs should no longer have the adjacent newtab attribute");
  assertNewTabButton("global");

  CustomizableUI.removeWidgetFromArea("home-button");
  ok(gBrowser.tabContainer.hasAttribute("hasadjacentnewtabbutton"),
    "tabs should have the adjacent newtab attribute again");
  assertNewTabButton("inner");

  CustomizableUI.reset();
  ok(CustomizableUI.inDefaultState, "Should be in default state");
});

/**
  * Reset to defaults in customize mode to see if that doesn't break things.
  */
add_task(async function reset_before_newtab_customizemode() {
  await startCustomizing();
  await waitForElementShown(kGlobalNewTabButton);
  simulateItemDrag(document.getElementById("home-button"), kGlobalNewTabButton, "start");
  ok(!gBrowser.tabContainer.hasAttribute("hasadjacentnewtabbutton"),
    "tabs should no longer have the adjacent newtab attribute");
  await endCustomizing();
  assertNewTabButton("global");
  await startCustomizing();
  await gCustomizeMode.reset();
  ok(gBrowser.tabContainer.hasAttribute("hasadjacentnewtabbutton"),
    "tabs should have the adjacent newtab attribute again");
  await endCustomizing();
  assertNewTabButton("inner");
  ok(CustomizableUI.inDefaultState, "Should be in default state");
});
