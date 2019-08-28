/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the flex container's element rep will display the box model highlighter on
// hover.

const TEST_URI = URL_ROOT + "doc_flexbox_specific_cases.html";

add_task(async function() {
  await addTab(TEST_URI);
  const { inspector, flexboxInspector } = await openLayoutView();
  const { document: doc } = flexboxInspector;

  const onFlexContainerRepRendered = waitForDOM(
    doc,
    ".flex-header-content .objectBox"
  );
  await selectNode("#container", inspector);
  const [flexContainerRep] = await onFlexContainerRepRendered;

  ok(flexContainerRep, "The flex container element rep is rendered.");

  info("Listen to node-highlight event and mouse over the rep");
  const onHighlight = inspector.highlighter.once("node-highlight");
  EventUtils.synthesizeMouse(
    flexContainerRep,
    10,
    5,
    { type: "mouseover" },
    doc.defaultView
  );
  const nodeFront = await onHighlight;

  ok(nodeFront, "nodeFront was returned from highlighting the node.");
  is(nodeFront.tagName, "DIV", "The highlighted node has the correct tagName.");
  is(
    nodeFront.attributes[0].name,
    "id",
    "The highlighted node has the correct attributes."
  );
  is(
    nodeFront.attributes[0].value,
    "container",
    "The highlighted node has the correct id."
  );
});
