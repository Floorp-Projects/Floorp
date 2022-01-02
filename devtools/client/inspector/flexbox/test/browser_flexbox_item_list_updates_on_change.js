/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the flex item list updates on changes to the number of flex items in the
// flex container.

const TEST_URI = `
  <style>
    #container {
      display: flex;
    }
  </style>
  <div id="container">
    <div></div>
  </div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, flexboxInspector } = await openLayoutView();
  const { document: doc } = flexboxInspector;

  const onFlexItemListRendered = waitForDOM(doc, ".flex-item-list");
  await selectNode("#container", inspector);
  const [flexItemList] = await onFlexItemListRendered;

  info("Checking the initial state of the flex item list.");
  ok(flexItemList, "The flex item list is rendered.");
  is(
    flexItemList.querySelectorAll("button").length,
    1,
    "Got the correct number of flex items in the list."
  );

  info("Changing the flexbox in the page.");
  const onFlexItemListChanged = waitForDOM(doc, ".flex-item-list > button", 2);
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    const div = content.document.createElement("div");
    content.document.getElementById("container").appendChild(div);
  });
  const elements = await onFlexItemListChanged;

  info("Checking the flex item list is correct.");
  is(elements.length, 2, "Flex item list was changed.");
});
