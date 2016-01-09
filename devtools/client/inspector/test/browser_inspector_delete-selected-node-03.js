/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test to ensure inspector can handle destruction of selected node inside an
// iframe.

const TEST_URL = URL_ROOT + "doc_inspector_delete-selected-node-01.html";

add_task(function* () {
  let { inspector } = yield openInspectorForURL(TEST_URL);

  let iframe = yield getNodeFront("iframe", inspector);
  let node = yield getNodeFrontInFrame("span", iframe, inspector);
  yield selectNode(node, inspector);

  info("Removing iframe.");
  yield inspector.walker.removeNode(iframe);
  yield inspector.selection.once("detached-front");

  let body = yield getNodeFront("body", inspector);

  is(inspector.selection.nodeFront, body, "Selection is now the body node");

  yield inspector.once("inspector-updated");
});
