/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test toggling (expand/collapse) elements by clicking on twisties

const TEST_URL = TEST_URL_ROOT + "doc_markup_toggle.html";

add_task(function*() {
  let {inspector} = yield addTab(TEST_URL).then(openInspector);

  info("Getting the container for the html element");
  let container = yield getContainerForSelector("html", inspector);
  ok(container.mustExpand, "HTML element mustExpand");
  ok(container.canExpand, "HTML element canExpand");
  is(container.expander.style.visibility, "hidden", "HTML twisty is hidden");

  info("Getting the container for the UL parent element");
  container = yield getContainerForSelector("ul", inspector);
  ok(!container.mustExpand, "UL element !mustExpand");
  ok(container.canExpand, "UL element canExpand");
  is(container.expander.style.visibility, "visible", "HTML twisty is visible");

  info("Clicking on the UL parent expander, and waiting for children");
  let onChildren = waitForChildrenUpdated(inspector);
  let onUpdated = inspector.once("inspector-updated");
  EventUtils.synthesizeMouseAtCenter(container.expander, {},
    inspector.markup.doc.defaultView);
  yield onChildren;
  yield onUpdated;

  info("Checking that child LI elements have been created");
  for (let i = 0; i < content.document.querySelectorAll("li").length; i ++) {
    let liContainer = yield getContainerForSelector(
      "li:nth-child(" + (i + 1) + ")", inspector);
    ok(liContainer, "A container for the child LI element was created");
  }
  ok(container.expanded, "Parent UL container is expanded");

  info("Clicking again on the UL expander");
  // No need to wait, this is a local, synchronous operation where nodes are
  // only hidden from the view, not destroyed
  EventUtils.synthesizeMouseAtCenter(container.expander, {},
    inspector.markup.doc.defaultView);

  info("Checking that child LI elements have been hidden");
  for (let i = 0; i < content.document.querySelectorAll("li").length; i ++) {
    let liContainer = yield getContainerForSelector(
      "li:nth-child(" + (i + 1) + ")", inspector);
    is(liContainer.elt.getClientRects().length, 0,
      "The container for the child LI element was hidden");
  }
  ok(!container.expanded, "Parent UL container is collapsed");
});
