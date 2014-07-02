/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the inspector has the correct pseudo-class locking menu items and
// that these items actually work

const DOMUtils = Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
const PSEUDOS = ["hover", "active", "focus"];

let test = asyncTest(function*() {
  yield addTab("data:text/html,pseudo-class lock node menu tests");

  info("Creating the test element");
  let div = content.document.createElement("div");
  div.textContent = "test div";
  content.document.body.appendChild(div);

  let {inspector} = yield openInspector();
  yield selectNode(div, inspector);

  info("Getting the inspector ctx menu and opening it");
  let menu = inspector.panelDoc.getElementById("inspector-node-popup");
  yield openMenu(menu);

  yield testMenuItems(div, menu, inspector);

  gBrowser.removeCurrentTab();
});

function openMenu(menu) {
  let def = promise.defer();

  menu.addEventListener("popupshowing", function onOpen() {
    menu.removeEventListener("popupshowing", onOpen, true);
    def.resolve();
  }, true);
  menu.openPopup();

  return def.promise;
}

function* testMenuItems(div,menu, inspector) {
  for (let pseudo of PSEUDOS) {
    let menuitem = inspector.panelDoc.getElementById("node-menu-pseudo-" + pseudo);
    ok(menuitem, ":" + pseudo + " menuitem exists");

    // Give the inspector panels a chance to update when the pseudoclass changes
    let onPseudo = inspector.selection.once("pseudoclass");
    let onRefresh = inspector.once("rule-view-refreshed");
    let onMutations = waitForMutation(inspector);

    menuitem.doCommand();

    yield onPseudo;
    yield onRefresh;
    yield onMutations;

    is(DOMUtils.hasPseudoClassLock(div, ":" + pseudo), true,
      "pseudo-class lock has been applied");
  }
}

function waitForMutation(inspector) {
  let def = promise.defer();
  inspector.walker.once("mutations", def.resolve);
  return def.promise;
}
