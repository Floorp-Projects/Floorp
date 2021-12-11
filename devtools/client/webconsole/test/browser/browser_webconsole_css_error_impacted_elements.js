/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Create a simple page for the iframe
const httpServer = createTestHTTPServer();
httpServer.registerPathHandler(`/`, function(request, response) {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write(`
    <html>
      <head>
        <meta charset="utf-8">
        <style>
          .subframe {
            color: blouge;
          }
        </style>
      </head>
      <body class="subframe">
        <h1 class="subframe">Hello</h1>
        <p class="subframe">sub-frame</p>
      </body>
    </html>`);
});

const TEST_URI = `data:text/html,<meta charset=utf8>
  <style>
    button {
      cursor: unknownCursor;
    }
  </style>
  <button id=1>Button 1</button>
  <button id=2>Button 2</button>
  <iframe src="http://localhost:${httpServer.identity.primaryPort}/"></iframe>
  `;

add_task(async function() {
  // Enable CSS Warnings
  await pushPref("devtools.webconsole.filter.css", true);

  const hud = await openNewTabAndConsole(TEST_URI);
  const toolbox = hud.toolbox;

  // Load the inspector panel to make it possible to listen for new node selections
  await toolbox.loadTool("inspector");
  const inspector = toolbox.getPanel("inspector");

  info("Check the CSS warning message for the top level document");
  let messageNode = await waitFor(() =>
    findMessage(hud, "Error in parsing value for ‘cursor’", ".message.css")
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

  let node = messageNode.querySelector(".objectBox-node");
  let openInInspectorIcon = node.querySelector(".open-inspector");
  ok(openInInspectorIcon !== null, "The is an open in inspector icon");

  info(
    "Clicking on the inspector icon and waiting for the inspector to be selected"
  );
  let onInspectorSelected = toolbox.once("inspector-selected");
  let onInspectorUpdated = inspector.once("inspector-updated");
  let onNewNode = toolbox.selection.once("new-node-front");

  openInInspectorIcon.click();

  await onInspectorSelected;
  await onInspectorUpdated;
  let nodeFront = await onNewNode;

  ok(true, "Inspector selected and new node got selected");
  is(nodeFront.displayName, "button", "The expected node was selected");
  is(nodeFront.id, "1", "The expected node was selected");

  info("Go back to the console");
  await toolbox.selectTool("webconsole");

  info("Check the CSS warning message for the third-party iframe");
  messageNode = await waitFor(() =>
    findMessage(hud, "Error in parsing value for ‘color’", ".message.css")
  );

  info("Click on the expand arrow");
  messageNode.querySelector(".arrow").click();

  await waitFor(
    () => messageNode.querySelectorAll(".objectBox-node").length == 3
  );
  ok(
    messageNode.textContent.includes(
      "NodeList(3) [ body.subframe, h1.subframe, p.subframe ]"
    ),
    "The message was expanded and shows the impacted elements"
  );
  node = messageNode.querySelectorAll(".objectBox-node")[2];
  openInInspectorIcon = node.querySelector(".open-inspector");
  ok(openInInspectorIcon !== null, "The is an open in inspector icon");

  info(
    "Clicking on the inspector icon and waiting for the inspector to be selected"
  );
  onInspectorSelected = toolbox.once("inspector-selected");
  onInspectorUpdated = inspector.once("inspector-updated");
  onNewNode = toolbox.selection.once("new-node-front");

  openInInspectorIcon.click();

  await onInspectorSelected;
  await onInspectorUpdated;
  nodeFront = await onNewNode;

  ok(true, "Inspector selected and new node got selected");
  is(nodeFront.displayName, "p", "The expected node was selected");
  is(nodeFront.className, "subframe", "The expected node was selected");
});
