/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test toggling (expand/collapse) elements by clicking on twisties

const TEST_URL = URL_ROOT + "doc_markup_toggle.html";

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_URL);

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
  ok(!container.selected, "UL container is not selected");

  info("Clicking on the UL parent expander, and waiting for children");
  await toggleContainerByClick(inspector, container);
  ok(!container.selected, "UL container is still not selected after expand");

  info("Checking that child LI elements have been created");
  let numLi = await getNumberOfMatchingElementsInContentPage("li");
  for (let i = 0; i < numLi; i++) {
    const liContainer = await getContainerForSelector(
      `li:nth-child(${i + 1})`,
      inspector
    );
    ok(liContainer, "A container for the child LI element was created");
  }
  ok(container.expanded, "Parent UL container is expanded");

  info("Clicking again on the UL expander");
  await toggleContainerByClick(inspector, container);
  ok(!container.selected, "UL container is still not selected after collapse");

  info("Checking that child LI elements have been hidden");
  numLi = await getNumberOfMatchingElementsInContentPage("li");
  for (let i = 0; i < numLi; i++) {
    const liContainer = await getContainerForSelector(
      `li:nth-child(${i + 1})`,
      inspector
    );
    is(
      liContainer.elt.getClientRects().length,
      0,
      "The container for the child LI element was hidden"
    );
  }
  ok(!container.expanded, "Parent UL container is collapsed");
});
