/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = `data:text/html,<meta charset=utf8>
  <style>
    button {
      cursor: unknownCursor;
    }
  </style>
  <button id=1>Button 1</button><button id=2>Button 2</button>`;

add_task(async function() {
  // Enable CSS Warnings
  await pushPref("devtools.webconsole.filter.css", true);
  const hud = await openNewTabAndConsole(TEST_URI);
  const toolbox = hud.toolbox;

  // Load the inspector panel to make it possible to listen for new node selections
  await toolbox.loadTool("inspector");
  const inspector = toolbox.getPanel("inspector");

  info("Wait for the CSS warning message");
  const messageNode = await waitFor(() =>
    findMessage(hud, "Error in parsing value", ".message.css")
  );

  info("Click on the expand arrow");
  messageNode.querySelector(".arrow").click();

  await waitFor(
    () => messageNode.querySelectorAll(".objectBox-node").length == 2
  );
  ok(
    messageNode.textContent.includes("NodeList [ button#1, button#2 ]"),
    "The message was expanded and shows the impacted elements"
  );

  const node = messageNode.querySelector(".objectBox-node");
  const openInInspectorIcon = node.querySelector(".open-inspector");
  ok(openInInspectorIcon !== null, "The is an open in inspector icon");

  info(
    "Clicking on the inspector icon and waiting for the inspector to be selected"
  );
  const onInspectorSelected = toolbox.once("inspector-selected");
  const onInspectorUpdated = inspector.once("inspector-updated");
  const onNewNode = toolbox.selection.once("new-node-front");

  openInInspectorIcon.click();

  await onInspectorSelected;
  await onInspectorUpdated;
  const nodeFront = await onNewNode;

  ok(true, "Inspector selected and new node got selected");
  is(nodeFront.displayName, "button", "The expected node was selected");
  is(nodeFront.id, "1", "The expected node was selected");
});
