/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test toggling (expand/collapse) elements by clicking on twisties

const TEST_URL = URL_ROOT + "doc_markup_toggle.html";

add_task(async function() {
  const {inspector, testActor} = await openInspectorForURL(TEST_URL);

  info("Getting the container for the html element");
  let container = await getContainerForSelector("html", inspector);
  ok(container.mustExpand, "HTML element mustExpand");
  ok(container.canExpand, "HTML element canExpand");
  is(container.expander.style.visibility, "hidden", "HTML twisty is hidden");

  info("Getting the container for the UL parent element");
  container = await getContainerForSelector("ul", inspector);
  ok(!container.mustExpand, "UL element !mustExpand");
  ok(container.canExpand, "UL element canExpand");
  is(container.expander.style.visibility, "visible", "HTML twisty is visible");

  info("Clicking on the UL parent expander, and waiting for children");
  const onChildren = waitForChildrenUpdated(inspector);
  const onUpdated = inspector.once("inspector-updated");
  EventUtils.synthesizeMouseAtCenter(container.expander, {},
    inspector.markup.doc.defaultView);
  await onChildren;
  await onUpdated;

  info("Checking that child LI elements have been created");
  let numLi = await testActor.getNumberOfElementMatches("li");
  for (let i = 0; i < numLi; i++) {
    const liContainer = await getContainerForSelector(
      `li:nth-child(${i + 1})`, inspector);
    ok(liContainer, "A container for the child LI element was created");
  }
  ok(container.expanded, "Parent UL container is expanded");

  info("Clicking again on the UL expander");
  // No need to wait, this is a local, synchronous operation where nodes are
  // only hidden from the view, not destroyed
  EventUtils.synthesizeMouseAtCenter(container.expander, {},
    inspector.markup.doc.defaultView);

  info("Checking that child LI elements have been hidden");
  numLi = await testActor.getNumberOfElementMatches("li");
  for (let i = 0; i < numLi; i++) {
    const liContainer = await getContainerForSelector(
      `li:nth-child(${i + 1})`, inspector);
    is(liContainer.elt.getClientRects().length, 0,
      "The container for the child LI element was hidden");
  }
  ok(!container.expanded, "Parent UL container is collapsed");
});
