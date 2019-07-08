/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the flex item list can be used to navigated to the selected flex item.

const TEST_URI = URL_ROOT + "doc_flexbox_specific_cases.html";

add_task(async function() {
  await addTab(TEST_URI);
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

  info("Clicking on the first flex item to navigate to the flex item.");
  const onFlexItemOutlineRendered = waitForDOM(doc, ".flex-outline-container");
  flexItemList.querySelector("button").click();
  const [flexOutlineContainer] = await onFlexItemOutlineRendered;

  info("Checking the selected flex item state.");
  ok(flexOutlineContainer, "The flex outline is rendered.");
  ok(!doc.querySelector(".flex-item-list"), "The flex item list is not shown.");
});
