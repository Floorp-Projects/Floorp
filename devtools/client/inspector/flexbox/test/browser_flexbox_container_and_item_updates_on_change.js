/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the flex container accordion is rendered when a flex item is updated to
// also be a flex container.

const TEST_URI = `
  <style>
    .container {
      display: flex;
    }
  </style>
  <div id="container" class="container">
    <div id="item">
      <div></div>
    </div>
  </div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, flexboxInspector } = await openLayoutView();
  const { document: doc } = flexboxInspector;

  const onFlexItemSizingRendered = waitForDOM(doc, "ul.flex-item-sizing");
  await selectNode("#item", inspector);
  const [flexSizingContainer] = await onFlexItemSizingRendered;

  ok(flexSizingContainer, "The flex sizing info is rendered.");

  info("Changing the flexbox in the page.");
  const onAccordionsChanged = waitForDOM(doc, ".accordion-item", 4);
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => (content.document.getElementById("item").className = "container")
  );
  const [flexItemPane, flexContainerPane] = await onAccordionsChanged;

  ok(flexItemPane, "The flex item accordion pane is rendered.");
  ok(flexContainerPane, "The flex container accordion pane is rendered.");
  is(
    flexItemPane.children[0].textContent,
    "Flex Item of div#container.container",
    "Got the correct header for the flex item pane."
  );
  is(
    flexContainerPane.children[0].textContent,
    "Flex Container",
    "Got the correct header for the flex container pane."
  );
});
