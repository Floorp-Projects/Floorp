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
    #subgrid {
      display: grid;
      grid: subgrid;
    }
    #flex {
      display: flex;
    }
    #block {
      display: block;
    }
  </style>
  <div id="grid">
    <div id="subgrid"></div>
  </div>
  <div id="flex">Flex</div>
  <div id="block">Block</div>
  <span>HELLO WORLD</span>
`;

add_task(async function() {
  info("Enable subgrid in order to see the subgrid display type.");
  await pushPref("layout.css.grid-template-subgrid-value.enabled", true);

  const {inspector} = await openInspectorForURL("data:text/html;charset=utf-8," +
    encodeURIComponent(TEST_URI));

  info("Check the display node is shown and the value of #grid.");
  await selectNode("#grid", inspector);
  const gridContainer = await getContainerForSelector("#grid", inspector);
  const gridDisplayNode = gridContainer.elt.querySelector(
    ".markup-badge[data-display]");
  is(gridDisplayNode.textContent, "grid", "Got the correct display type for #grid.");
  is(gridDisplayNode.style.display, "inline-block", "#grid display node is shown.");

  info("Check the display node is shown and the value of #subgrid.");
  await selectNode("#subgrid", inspector);
  const subgridContainer = await getContainerForSelector("#subgrid", inspector);
  const subgridDisplayNode = subgridContainer.elt.querySelector(
    ".markup-badge[data-display]");
  is(subgridDisplayNode.textContent, "subgrid",
    "Got the correct display type for #subgrid");
  is(subgridDisplayNode.style.display, "inline-block", "#subgrid display node is shown");

  info("Check the display node is shown and the value of #flex.");
  await selectNode("#flex", inspector);
  const flexContainer = await getContainerForSelector("#flex", inspector);
  const flexDisplayNode = flexContainer.elt.querySelector(
    ".markup-badge[data-display]");
  is(flexDisplayNode.textContent, "flex", "Got the correct display type for #flex");
  is(flexDisplayNode.style.display, "inline-block", "#flex display node is shown.");

  info("Check the display node is shown and the value of #block.");
  await selectNode("#block", inspector);
  const blockContainer = await getContainerForSelector("#block", inspector);
  const blockDisplayNode = blockContainer.elt.querySelector(
    ".markup-badge[data-display]");
  is(blockDisplayNode.textContent, "block", "Got the correct display type for #block");
  is(blockDisplayNode.style.display, "none", "#block display node is hidden.");

  info("Check the display node is shown and the value of span.");
  await selectNode("span", inspector);
  const spanContainer = await getContainerForSelector("span", inspector);
  const spanDisplayNode = spanContainer.elt.querySelector(
    ".markup-badge[data-display]");
  is(spanDisplayNode.textContent, "inline", "Got the correct display type for #span");
  is(spanDisplayNode.style.display, "none", "span display node is hidden.");
});
