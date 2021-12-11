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

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const toolbox = hud.toolbox;

  const highlighterTestFront = await getHighlighterTestFront(toolbox);
  const highlighter = toolbox.getHighlighter();
  let onHighlighterShown;
  let onHighlighterHidden;

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.logNode("h1");
  });

  const msg = await waitFor(() => findMessage(hud, "<h1>"));
  const node = msg.querySelector(".objectBox-node");
  ok(node !== null, "Node was logged as expected");
  const view = node.ownerDocument.defaultView;
  const nonHighlightEl = toolbox.doc.getElementById(
    "toolbox-meatball-menu-button"
  );

  info("Highlight the node by moving the cursor on it");
  onHighlighterShown = highlighter.waitForHighlighterShown();

  EventUtils.synthesizeMouseAtCenter(node, { type: "mousemove" }, view);

  const { nodeFront } = await onHighlighterShown;
  is(nodeFront.displayName, "h1", "The correct node was highlighted");
  isVisible = await highlighterTestFront.isHighlighting();
  ok(isVisible, "Highlighter is displayed");

  info("Unhighlight the node by moving away from the node");
  onHighlighterHidden = highlighter.waitForHighlighterHidden();
  EventUtils.synthesizeMouseAtCenter(
    nonHighlightEl,
    { type: "mousemove" },
    view
  );

  await onHighlighterHidden;
  ok(true, "node-unhighlight event was fired when moving away from the node");

  info("Check we don't have zombie highlighters when briefly hovering a node");
  onHighlighterShown = highlighter.waitForHighlighterShown();
  onHighlighterHidden = highlighter.waitForHighlighterHidden();
  // Move hover the node and then right after move out.
  EventUtils.synthesizeMouseAtCenter(node, { type: "mousemove" }, view);
  EventUtils.synthesizeMouseAtCenter(
    nonHighlightEl,
    { type: "mousemove" },
    view
  );
  await Promise.all([onHighlighterShown, onHighlighterHidden]);
  ok(true, "The highlighter was removed");

  isVisible = await highlighterTestFront.isHighlighting();
  is(isVisible, false, "The highlighter is not displayed anymore");
});
