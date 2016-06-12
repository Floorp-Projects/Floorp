/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the inspector has the correct pseudo-class locking menu items and
// that these items actually work

const TEST_URI = "data:text/html;charset=UTF-8," +
                 "pseudo-class lock node menu tests" +
                 "<div>test div</div>";
const PSEUDOS = ["hover", "active", "focus"];

add_task(function* () {
  let {inspector, testActor} = yield openInspectorForURL(TEST_URI);
  yield selectNode("div", inspector);

  let allMenuItems = openContextMenuAndGetAllItems(inspector);

  yield testMenuItems(testActor, allMenuItems, inspector);
});

function* testMenuItems(testActor, allMenuItems, inspector) {
  for (let pseudo of PSEUDOS) {
    let menuItem =
      allMenuItems.find(item => item.id === "node-menu-pseudo-" + pseudo);
    ok(menuItem, ":" + pseudo + " menuitem exists");
    is(menuItem.disabled, false, ":" + pseudo + " menuitem is enabled");

    // Give the inspector panels a chance to update when the pseudoclass changes
    let onPseudo = inspector.selection.once("pseudoclass");
    let onRefresh = inspector.once("rule-view-refreshed");

    // Walker uses SDK-events so calling walker.once does not return a promise.
    let onMutations = once(inspector.walker, "mutations");

    menuItem.click();

    yield onPseudo;
    yield onRefresh;
    yield onMutations;

    let hasLock = yield testActor.hasPseudoClassLock("div", ":" + pseudo);
    ok(hasLock, "pseudo-class lock has been applied");
  }
}
