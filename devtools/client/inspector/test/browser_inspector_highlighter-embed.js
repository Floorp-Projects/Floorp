/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the highlighter can go inside <embed> elements

const TEST_URL = URL_ROOT + "doc_inspector_embed.html";

add_task(function* () {
  let {inspector} = yield openInspectorForURL(TEST_URL);

  info("Get a node inside the <embed> element and select/highlight it");
  let body = yield getEmbeddedBody(inspector);
  yield selectAndHighlightNode(body, inspector);

  let selectedNode = inspector.selection.nodeFront;
  is(selectedNode.tagName.toLowerCase(), "body", "The selected node is <body>");
  ok(selectedNode.baseURI.endsWith("doc_inspector_menu.html"),
     "The selected node is the <body> node inside the <embed> element");
});

function* getEmbeddedBody({walker}) {
  let embed = yield walker.querySelector(walker.rootNode, "embed");
  let {nodes} = yield walker.children(embed);
  let contentDoc = nodes[0];
  let body = yield walker.querySelector(contentDoc, "body");
  return body;
}
