/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the grid item's element rep will display the box model higlighter on hover
// and select the node on click.

const TEST_URI = `
  <style type='text/css'>
    #grid {
      display: grid;
    }
  </style>
  <div id="grid">
    <div id="cell1">cell1</div>
    <div id="cell2">cell2</div>
  </div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, gridInspector, toolbox } = await openLayoutView();
  const { document: doc } = gridInspector;
  const { store } = inspector;

  const gridList = doc.querySelector("#grid-list");
  const elementRep = gridList.children[0].querySelector(".open-inspector");
  info("Scrolling into the view the #grid element node rep.");
  elementRep.scrollIntoView();

  info("Listen to node-highlight event and mouse over the widget");
  const onHighlight = toolbox.once("node-highlight");
  EventUtils.synthesizeMouse(elementRep, 10, 5, {type: "mouseover"}, doc.defaultView);
  const nodeFront = await onHighlight;

  ok(nodeFront, "nodeFront was returned from highlighting the node.");
  is(nodeFront.tagName, "DIV", "The highlighted node has the correct tagName.");
  is(nodeFront.attributes[0].name, "id",
    "The highlighted node has the correct attributes.");
  is(nodeFront.attributes[0].value, "grid", "The highlighted node has the correct id.");

  const onSelection = inspector.selection.once("new-node-front");
  EventUtils.sendMouseEvent({type: "click"}, elementRep, doc.defaultView);
  await onSelection;

  is(inspector.selection.nodeFront, store.getState().grids[0].nodeFront,
    "The selected node is the one stored on the grid item's state.");
});
