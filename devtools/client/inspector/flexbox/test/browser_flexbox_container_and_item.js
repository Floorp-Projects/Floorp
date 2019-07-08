/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the flex container accordion and flex item accordion are both rendered when
// the selected element is both a flex container and item.

const TEST_URI = `
  <style type='text/css'>
    .container {
      display: flex;
    }
  </style>
  <div id="container" class="container">
    <div id="item" class="container">
      <div></div>
    </div>
  </div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, flexboxInspector } = await openLayoutView();
  const { document: doc } = flexboxInspector;

  const onAccordionsRendered = waitForDOM(doc, ".accordion > div", 4);
  await selectNode("#item", inspector);
  const [flexItemPane, flexContainerPane] = await onAccordionsRendered;

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
