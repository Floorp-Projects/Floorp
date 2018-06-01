/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the custom selector highlighter creates as many box-model
// highlighters as there are nodes that match the given selector

const TEST_URL = "data:text/html;charset=utf-8," +
                 "<div id='test-node'>test node</div>" +
                 "<ul>" +
                  "  <li class='item'>item</li>" +
                  "  <li class='item'>item</li>" +
                  "  <li class='item'>item</li>" +
                  "  <li class='item'>item</li>" +
                  "  <li class='item'>item</li>" +
                 "</ul>";

const TEST_DATA = [{
  selector: "#test-node",
  containerCount: 1
}, {
  selector: null,
  containerCount: 0,
}, {
  selector: undefined,
  containerCount: 0,
}, {
  selector: ".invalid-class",
  containerCount: 0
}, {
  selector: ".item",
  containerCount: 5
}, {
  selector: "#test-node, ul, .item",
  containerCount: 7
}];

requestLongerTimeout(5);

add_task(async function() {
  const {inspector, testActor} = await openInspectorForURL(TEST_URL);
  const front = inspector.inspector;
  const highlighter = await front.getHighlighterByType("SelectorHighlighter");

  const contextNode = await getNodeFront("body", inspector);

  for (const {selector, containerCount} of TEST_DATA) {
    info("Showing the highlighter on " + selector + ". Expecting " +
      containerCount + " highlighter containers");

    await highlighter.show(contextNode, {selector});

    const nb = await testActor.getSelectorHighlighterBoxNb(highlighter.actorID);
    ok(nb !== null, "The number of highlighters was retrieved");

    is(nb, containerCount, "The correct number of highlighers were created");
    await highlighter.hide();
  }

  await highlighter.finalize();
});
