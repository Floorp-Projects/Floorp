/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Check clicking on open-in-inspector icon does select the node in the inspector.

const HTML = `
  <!DOCTYPE html>
  <html>
    <body>
      <h1>Select node in inspector test</h1>
    </body>
    <script>
      function logNode(selector) {
        console.log(document.querySelector(selector));
      }
    </script>
  </html>
`;
const TEST_URI = "data:text/html;charset=utf-8," + encodeURI(HTML);

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const toolbox = gDevTools.getToolbox(hud.target);

  // Loading the inspector panel at first, to make it possible to listen for
  // new node selections
  await toolbox.loadTool("inspector");
  const inspector = toolbox.getPanel("inspector");

  await ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    content.wrappedJSObject.logNode("h1");
  });

  const msg = await waitFor(() => findMessage(hud, "<h1>"));
  const node = msg.querySelector(".objectBox-node");
  ok(node !== null, "Node was logged as expected");

  const openInInspectorIcon = node.querySelector(".open-inspector");
  ok(openInInspectorIcon !== null, "The is an open in inspector icon");

  info("Clicking on the inspector icon and waiting for the " +
       "inspector to be selected");
  const onInspectorSelected = toolbox.once("inspector-selected");
  const onInspectorUpdated = inspector.once("inspector-updated");
  const onNewNode = toolbox.selection.once("new-node-front");

  openInInspectorIcon.click();

  await onInspectorSelected;
  await onInspectorUpdated;
  const nodeFront = await onNewNode;

  ok(true, "Inspector selected and new node got selected");
  is(nodeFront.displayName, "h1", "The expected node was selected");

  is(msg.querySelector(".arrow").classList.contains("expanded"), false,
    "The object inspector wasn't expanded");
});
