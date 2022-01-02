/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the flexbox sizing info updates on changes to the flex item properties.

const TEST_URI = `
  <style>
    #container {
      display: flex;
    }
  </style>
  <div id="container">
    <div id="item"></div>
  </div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, flexboxInspector } = await openLayoutView();
  const { document: doc } = flexboxInspector;

  const onFlexItemSizingRendered = waitForDOM(doc, "ul.flex-item-sizing");
  await selectNode("#item", inspector);
  const [flexItemSizingContainer] = await onFlexItemSizingRendered;

  info("Checking the initial state of the flex item list.");
  is(
    flexItemSizingContainer.querySelectorAll("li").length,
    2,
    "Got the correct number of flex item sizing properties in the list."
  );

  info("Changing the flexbox in the page.");
  const onFlexItemSizingChanged = waitForDOM(
    doc,
    "ul.flex-item-sizing > li",
    3
  );
  SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => (content.document.getElementById("item").style.minWidth = "100px")
  );
  const elements = await onFlexItemSizingChanged;

  info("Checking the flex item sizing info is correct.");
  is(elements.length, 3, "Flex item sizing info was changed.");
});
