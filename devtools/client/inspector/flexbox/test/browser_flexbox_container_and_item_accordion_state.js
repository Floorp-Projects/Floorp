/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the order in which accordion items are shown for container-item elements.
// For those combined types, the container accordion is shown first if the selection came
// from the markup-view, because we assume in this case that users do want to see the
// element selected as a container first.
// However when users select an item in the list of items in the container accordion (or
// in the item selector dropdown), then the item accordion should be shown first.

const TEST_URI = URL_ROOT + "doc_flexbox_specific_cases.html";

add_task(async function() {
  await addTab(TEST_URI);
  const { inspector, flexboxInspector } = await openLayoutView();
  const { document: doc } = flexboxInspector;

  info("Select a flex container-only node");
  await selectNode("#container-only", inspector);
  await waitUntil(
    () => doc.querySelectorAll(".flex-header-container-properties").length
  );

  info("Check that there is only 1 accordion for displayed");
  let accordions = doc.querySelectorAll(".flex-accordion");
  is(accordions.length, 1, "There's only 1 accordion");
  ok(
    accordions[0].classList.contains("container"),
    "The accordion is the container type"
  );

  info("Select a flex container+item node by clicking in the markup-view");
  await clickOnNodeInMarkupView("#container-and-item", inspector);
  await waitUntil(() => doc.querySelectorAll(".flex-accordion").length === 2);

  info(
    "Check that the 2 accordions are displayed, with container type being first"
  );
  accordions = doc.querySelectorAll(".flex-accordion");
  is(accordions.length, 2, "There are 2 accordions");
  ok(
    accordions[0].classList.contains("container"),
    "The first accordion is the container type"
  );
  ok(
    accordions[1].classList.contains("item"),
    "The second accordion is the item type"
  );

  info("Select the container-only node again");
  await selectNode("#container-only", inspector);
  await waitUntil(() => doc.querySelectorAll(".flex-accordion").length === 1);

  info(
    "Click on the container+item node right there in the accordion item list"
  );
  doc.querySelector(".flex-item-list button").click();
  await waitUntil(() => doc.querySelectorAll(".flex-accordion").length === 2);

  info(
    "Check that the 2 accordions are displayed again, with item type being first"
  );
  accordions = doc.querySelectorAll(".flex-accordion");
  is(accordions.length, 2, "There are 2 accordions again");
  ok(
    accordions[0].classList.contains("item"),
    "The first accordion is the item type"
  );
  ok(
    accordions[1].classList.contains("container"),
    "The second accordion is the container type"
  );
});

async function clickOnNodeInMarkupView(selector, inspector) {
  const { selection, markup } = inspector;

  await markup.expandAll(selection.nodeFront);
  const nodeFront = await getNodeFront(selector, inspector);
  const markupContainer = markup.getContainer(nodeFront);

  const onSelected = inspector.once("inspector-updated");
  EventUtils.synthesizeMouseAtCenter(
    markupContainer.tagLine,
    { type: "mousedown" },
    markup.doc.defaultView
  );
  EventUtils.synthesizeMouseAtCenter(
    markupContainer.tagLine,
    { type: "mouseup" },
    markup.doc.defaultView
  );
  await onSelected;
}
