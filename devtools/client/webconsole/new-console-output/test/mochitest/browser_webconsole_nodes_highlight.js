/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Check hovering logged nodes highlight them in the content page.

const HTML = `
  <!DOCTYPE html>
  <html>
    <body>
      <h1>Node Highlight  Test</h1>
    </body>
    <script>
      function logNode(selector) {
        console.log(document.querySelector(selector));
      }
    </script>
  </html>
`;
const TEST_URI = "data:text/html;charset=utf-8," + encodeURI(HTML);

add_task(function* () {
  const hud = yield openNewTabAndConsole(TEST_URI);
  const toolbox = gDevTools.getToolbox(hud.target);

  yield ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    content.wrappedJSObject.logNode("h1");
  });

  let msg = yield waitFor(() => findMessage(hud, "<h1>"));
  let node = msg.querySelector(".objectBox-node");
  ok(node !== null, "Node was logged as expected");
  const view = node.ownerDocument.defaultView;

  info("Highlight the node by moving the cursor on it");
  let onNodeHighlight = toolbox.once("node-highlight");
  EventUtils.synthesizeMouseAtCenter(node, {type: "mousemove"}, view);

  let nodeFront = yield onNodeHighlight;
  is(nodeFront.displayName, "h1", "The correct node was highlighted");

  info("Unhighlight the node by moving away from the node");
  let onNodeUnhighlight = toolbox.once("node-unhighlight");
  let btn = toolbox.doc.querySelector(".toolbox-dock-button");
  EventUtils.synthesizeMouseAtCenter(btn, {type: "mousemove"}, view);

  yield onNodeUnhighlight;
  ok(true, "node-unhighlight event was fired when moving away from the node");
});
