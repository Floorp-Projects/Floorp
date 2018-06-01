/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test toggling (expand/collapse) elements by dbl-clicking on tag lines

const TEST_URL = URL_ROOT + "doc_markup_toggle.html";

add_task(async function() {
  const {inspector, testActor} = await openInspectorForURL(TEST_URL);

  info("Getting the container for the UL parent element");
  const container = await getContainerForSelector("ul", inspector);

  info("Dbl-clicking on the UL parent expander, and waiting for children");
  const onChildren = waitForChildrenUpdated(inspector);
  const onUpdated = inspector.once("inspector-updated");
  EventUtils.synthesizeMouseAtCenter(container.tagLine, {clickCount: 2},
    inspector.markup.doc.defaultView);
  await onChildren;
  await onUpdated;

  info("Checking that child LI elements have been created");
  let numLi = await testActor.getNumberOfElementMatches("li");
  for (let i = 0; i < numLi; i++) {
    const liContainer = await getContainerForSelector(
      "li:nth-child(" + (i + 1) + ")", inspector);
    ok(liContainer, "A container for the child LI element was created");
  }
  ok(container.expanded, "Parent UL container is expanded");

  info("Dbl-clicking again on the UL expander");
  // No need to wait, this is a local, synchronous operation where nodes are
  // only hidden from the view, not destroyed
  EventUtils.synthesizeMouseAtCenter(container.tagLine, {clickCount: 2},
    inspector.markup.doc.defaultView);

  info("Checking that child LI elements have been hidden");
  numLi = await testActor.getNumberOfElementMatches("li");
  for (let i = 0; i < numLi; i++) {
    const liContainer = await getContainerForSelector(
      "li:nth-child(" + (i + 1) + ")", inspector);
    is(liContainer.elt.getClientRects().length, 0,
      "The container for the child LI element was hidden");
  }
  ok(!container.expanded, "Parent UL container is collapsed");
});
