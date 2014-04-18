/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test toggling (expand/collapse) elements by dbl-clicking on tag lines

const TEST_URL = TEST_URL_ROOT + "doc_markup_toggle.html";

let test = asyncTest(function*() {
  let {inspector} = yield addTab(TEST_URL).then(openInspector);

  info("Getting the container for the UL parent element");
  let container = getContainerForRawNode("ul", inspector);

  info("Dbl-clicking on the UL parent expander, and waiting for children");
  let onChildren = waitForChildrenUpdated(inspector);
  let onUpdated = inspector.once("inspector-updated");
  EventUtils.synthesizeMouseAtCenter(container.tagLine, {clickCount: 2},
    inspector.markup.doc.defaultView);
  yield onChildren;
  yield onUpdated;

  info("Checking that child LI elements have been created");
  for (let li of content.document.querySelectorAll("li")) {
    ok(getContainerForRawNode(li, inspector),
      "A container for the child LI element was created");
  }
  ok(container.expanded, "Parent UL container is expanded");

  info("Dbl-clicking again on the UL expander");
  // No need to wait, this is a local, synchronous operation where nodes are
  // only hidden from the view, not destroyed
  EventUtils.synthesizeMouseAtCenter(container.tagLine, {clickCount: 2},
    inspector.markup.doc.defaultView);

  info("Checking that child LI elements have been hidden");
  for (let li of content.document.querySelectorAll("li")) {
    let liContainer = getContainerForRawNode(li, inspector);
    is(liContainer.elt.getClientRects().length, 0,
      "The container for the child LI element was hidden");
  }
  ok(!container.expanded, "Parent UL container is collapsed");
});
