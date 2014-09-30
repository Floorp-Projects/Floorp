/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that pseudoelements are displayed correctly in the rule view

const TEST_URI = TEST_URL_ROOT + "doc_pseudoelement.html";

let test = asyncTest(function*() {
  yield addTab(TEST_URI);
  let {toolbox, inspector, view} = yield openRuleView();

  yield testTopLeft(inspector, view);
});

function* testTopLeft(inspector, view) {
  let node = inspector.markup.walker.frontForRawNode(getNode("#topleft"));
  let children = yield inspector.markup.walker.children(node);

  is (children.nodes.length, 3, "Element has correct number of children");

  let beforeElement = children.nodes[0];
  is (beforeElement.tagName, "_moz_generated_content_before", "tag name is correct");
  yield selectNode(beforeElement, inspector);

  let afterElement = children.nodes[children.nodes.length-1];
  is (afterElement.tagName, "_moz_generated_content_after", "tag name is correct");
  yield selectNode(afterElement, inspector);
}

