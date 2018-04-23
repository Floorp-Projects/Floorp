/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that markup display node shows for only for grid and flex containers.

const TEST_URI = `
  <style type="text/css">
    #grid {
      display: grid;
    }
    #flex {
      display: flex;
    }
    #block {
      display: block;
    }
  </style>
  <div id="grid">Grid</div>
  <div id="flex">Flex</div>
  <div id="block">Block</div>
  <span>HELLO WORLD</span>
`;

add_task(async function() {
  let {inspector} = await openInspectorForURL("data:text/html;charset=utf-8," +
    encodeURIComponent(TEST_URI));

  info("Check the display node is shown and the value of #grid.");
  await selectNode("#grid", inspector);
  let gridContainer = await getContainerForSelector("#grid", inspector);
  let gridDisplayNode = gridContainer.elt.querySelector(".markupview-display-badge");
  is(gridDisplayNode.textContent, "grid", "Got the correct display type for #grid.");
  is(gridDisplayNode.style.display, "inline-block", "#grid display node is shown.");

  info("Check the display node is shown and the value of #flex.");
  await selectNode("#flex", inspector);
  let flexContainer = await getContainerForSelector("#flex", inspector);
  let flexDisplayNode = flexContainer.elt.querySelector(".markupview-display-badge");
  is(flexDisplayNode.textContent, "flex", "Got the correct display type for #flex");
  is(flexDisplayNode.style.display, "inline-block", "#flex display node is shown.");

  info("Check the display node is shown and the value of #block.");
  await selectNode("#block", inspector);
  let blockContainer = await getContainerForSelector("#block", inspector);
  let blockDisplayNode = blockContainer.elt.querySelector(".markupview-display-badge");
  is(blockDisplayNode.textContent, "block", "Got the correct display type for #block");
  is(blockDisplayNode.style.display, "none", "#block display node is hidden.");

  info("Check the display node is shown and the value of span.");
  await selectNode("span", inspector);
  let spanContainer = await getContainerForSelector("span", inspector);
  let spanDisplayNode = spanContainer.elt.querySelector(".markupview-display-badge");
  is(spanDisplayNode.textContent, "inline", "Got the correct display type for #span");
  is(spanDisplayNode.style.display, "none", "span display node is hidden.");
});
