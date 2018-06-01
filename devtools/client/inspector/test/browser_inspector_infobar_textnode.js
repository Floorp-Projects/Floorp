/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Bug 1309212 - Make sure info-bar is displayed with dimensions for text nodes.

const TEST_URI = URL_ROOT + "doc_inspector_infobar_textnode.html";

add_task(async function() {
  const { inspector, testActor } = await openInspectorForURL(TEST_URI);
  const { walker } = inspector;

  info("Retrieve the children of #textnode-container");
  const div = await walker.querySelector(walker.rootNode, "#textnode-container");
  const { nodes } = await inspector.walker.children(div);

  // Children 0, 2 and 4 are text nodes, for which we expect to see an infobar containing
  // dimensions.

  // Regular text node.
  info("Select the first text node");
  await selectNode(nodes[0], inspector, "test-highlight");
  await checkTextNodeInfoBar(testActor);

  // Whitespace-only text node.
  info("Select the second text node");
  await selectNode(nodes[2], inspector, "test-highlight");
  await checkTextNodeInfoBar(testActor);

  // Regular text node.
  info("Select the third text node");
  await selectNode(nodes[4], inspector, "test-highlight");
  await checkTextNodeInfoBar(testActor);
});

async function checkTextNodeInfoBar(testActor) {
  const tag = await testActor.getHighlighterNodeTextContent(
    "box-model-infobar-tagname");
  is(tag, "#text", "node display name is #text");
  const dims = await testActor.getHighlighterNodeTextContent(
      "box-model-infobar-dimensions");
  // Do not assert dimensions as they might be platform specific.
  ok(!!dims, "node has dims");
}
