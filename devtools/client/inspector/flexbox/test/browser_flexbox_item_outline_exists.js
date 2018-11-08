/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the flex item outline exists when a flex item is selected.

const TEST_URI = URL_ROOT + "doc_flexbox_specific_cases.html";

add_task(async function() {
  await addTab(TEST_URI);
  const { inspector, flexboxInspector } = await openLayoutView();
  const { document: doc } = flexboxInspector;

  // Select a flex item in the test document and wait for the outline to be rendered.
  const onFlexItemOutlineRendered = waitForDOM(doc, ".flex-outline-container");
  await selectNode(".item", inspector);
  const [flexOutlineContainer] = await onFlexItemOutlineRendered;

  ok(flexOutlineContainer, "The flex outline exists in the DOM");

  const [basis, final] = [...flexOutlineContainer.querySelectorAll(
    ".flex-outline-basis, .flex-outline-final")];

  ok(basis, "The basis outline exists");
  ok(final, "The final outline exists");
});
