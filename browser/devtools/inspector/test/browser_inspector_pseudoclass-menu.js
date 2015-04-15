/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the inspector has the correct pseudo-class locking menu items and
// that these items actually work

const DOMUtils = Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
const TEST_URI = "data:text/html;charset=UTF-8," +
                 "pseudo-class lock node menu tests";
const PSEUDOS = ["hover", "active", "focus"];

add_task(function*() {
  let {inspector} = yield openInspectorForURL(TEST_URI);

  info("Creating the test element");
  let div = content.document.createElement("div");
  div.textContent = "test div";
  content.document.body.appendChild(div);

  yield selectNode("div", inspector);

  info("Getting the inspector ctx menu and opening it");
  let menu = inspector.panelDoc.getElementById("inspector-node-popup");
  yield openMenu(menu);

  yield testMenuItems(div, menu, inspector);
});

function openMenu(menu) {
  let promise = once(menu, "popupshowing", true);
  menu.openPopup();
  return promise;
}

function* testMenuItems(div, menu, inspector) {
  for (let pseudo of PSEUDOS) {
    let menuitem = inspector.panelDoc.getElementById("node-menu-pseudo-" + pseudo);
    ok(menuitem, ":" + pseudo + " menuitem exists");

    // Give the inspector panels a chance to update when the pseudoclass changes
    let onPseudo = inspector.selection.once("pseudoclass");
    let onRefresh = inspector.once("rule-view-refreshed");

    // Walker uses SDK-events so calling walker.once does not return a promise.
    let onMutations = once(inspector.walker, "mutations");

    menuitem.doCommand();

    yield onPseudo;
    yield onRefresh;
    yield onMutations;

    let {data: hasLock} = yield executeInContent("Test:HasPseudoClassLock",
                                                 {pseudo: ":" + pseudo},
                                                 {node: div});
    ok(hasLock, "pseudo-class lock has been applied");
  }
}
