/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that nodes that are not displayed appear differently in the markup-view
// when these nodes are imported in the view.

// Note that nodes inside a display:none parent are obviously not displayed too
// but the markup-view uses css inheritance to mark those as hidden instead of
// having to visit each and every child of a hidden node. So there's no sense
// testing children nodes.

const TEST_URL = URL_ROOT + "doc_markup_not_displayed.html";
const TEST_DATA = [
  {selector: "#normal-div", isDisplayed: true},
  {selector: "head", isDisplayed: false},
  {selector: "#display-none", isDisplayed: false},
  {selector: "#hidden-true", isDisplayed: false},
  {selector: "#visibility-hidden", isDisplayed: true},
  {selector: "#hidden-via-hide-shortcut", isDisplayed: false},
];

add_task(function* () {
  let {inspector} = yield openInspectorForURL(TEST_URL);

  for (let {selector, isDisplayed} of TEST_DATA) {
    info("Getting node " + selector);
    let nodeFront = yield getNodeFront(selector, inspector);
    let container = getContainerForNodeFront(nodeFront, inspector);
    is(!container.elt.classList.contains("not-displayed"), isDisplayed,
       `The container for ${selector} is marked as displayed ${isDisplayed}`);
  }
});
