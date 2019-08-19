/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the flex item sizing UI also appears for pseudo elements.

const TEST_URI = URL_ROOT + "doc_flexbox_pseudos.html";

add_task(async function() {
  await addTab(TEST_URI);
  const { inspector, flexboxInspector } = await openLayoutView();
  const { document: doc } = flexboxInspector;

  info("Select the ::before pseudo-element in the inspector");
  const containerNode = await getNodeFront(".container", inspector);
  const { nodes } = await inspector.walker.children(containerNode);
  const beforeNode = nodes[0];

  const onFlexItemSizingRendered = waitForDOM(doc, "ul.flex-item-sizing");
  const onFlexItemOutlineRendered = waitForDOM(doc, ".flex-outline-container");
  await selectNode(beforeNode, inspector);
  const [flexSizingContainer] = await onFlexItemSizingRendered;
  const [flexOutlineContainer] = await onFlexItemOutlineRendered;

  ok(flexSizingContainer, "The flex sizing exists in the DOM");
  ok(flexOutlineContainer, "The flex outline exists in the DOM");

  info("Check that the various sizing sections are displayed");
  const allSections = [...flexSizingContainer.querySelectorAll(".section")];
  ok(allSections.length, "Sizing sections are displayed");

  info("Check that the various parts of the outline are displayed");
  const [basis, final] = [
    ...flexOutlineContainer.querySelectorAll(
      ".flex-outline-basis, .flex-outline-final"
    ),
  ];
  ok(basis && final, "The final and basis parts of the outline exist");
});
