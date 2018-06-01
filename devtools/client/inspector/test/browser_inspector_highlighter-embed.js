/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the highlighter can go inside <embed> elements

const TEST_URL = URL_ROOT + "doc_inspector_embed.html";

add_task(async function() {
  const {inspector} = await openInspectorForURL(TEST_URL);

  info("Get a node inside the <embed> element and select/highlight it");
  const body = await getEmbeddedBody(inspector);
  await selectAndHighlightNode(body, inspector);

  const selectedNode = inspector.selection.nodeFront;
  is(selectedNode.tagName.toLowerCase(), "body", "The selected node is <body>");
  ok(selectedNode.baseURI.endsWith("doc_inspector_menu.html"),
     "The selected node is the <body> node inside the <embed> element");
});

async function getEmbeddedBody({walker}) {
  const embed = await walker.querySelector(walker.rootNode, "embed");
  const {nodes} = await walker.children(embed);
  const contentDoc = nodes[0];
  const body = await walker.querySelector(contentDoc, "body");
  return body;
}
