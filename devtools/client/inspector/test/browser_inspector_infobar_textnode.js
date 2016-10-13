/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Bug 1309212 - Make sure info-bar is displayed with dimensions for text nodes.

const TEST_URI = URL_ROOT + "doc_inspector_infobar_textnode.html";

add_task(function* () {
  let { inspector, testActor } = yield openInspectorForURL(TEST_URI);
  let { walker } = inspector;

  info("Retrieve the children of #textnode-container");
  let div = yield walker.querySelector(walker.rootNode, "#textnode-container");
  let { nodes } = yield inspector.walker.children(div);

  // Children 0, 2 and 4 are text nodes, for which we expect to see an infobar containing
  // dimensions.

  // Regular text node.
  info("Select the first text node");
  yield selectNode(nodes[0], inspector, "test-highlight");
  yield checkTextNodeInfoBar(testActor);

  // Whitespace-only text node.
  info("Select the second text node");
  yield selectNode(nodes[2], inspector, "test-highlight");
  yield checkTextNodeInfoBar(testActor);

  // Regular text node.
  info("Select the third text node");
  yield selectNode(nodes[4], inspector, "test-highlight");
  yield checkTextNodeInfoBar(testActor);
});

function* checkTextNodeInfoBar(testActor) {
  let tag = yield testActor.getHighlighterNodeTextContent(
    "box-model-infobar-tagname");
  is(tag, "#text", "node display name is #text");
  let dims = yield testActor.getHighlighterNodeTextContent(
      "box-model-infobar-dimensions");
  // Do not assert dimensions as they might be platform specific.
  ok(!!dims, "node has dims");
}
