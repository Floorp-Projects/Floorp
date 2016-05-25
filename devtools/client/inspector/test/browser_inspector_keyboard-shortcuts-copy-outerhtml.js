/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test copy outer HTML from the keyboard/copy event

const TEST_URL = URL_ROOT + "doc_inspector_outerhtml.html";

add_task(function* () {
  let { inspector } = yield openInspectorForURL(TEST_URL);
  let root = inspector.markup._elt;

  info("Test copy outerHTML for COMMENT node");
  let comment = getElementByType(inspector, Ci.nsIDOMNode.COMMENT_NODE);
  yield setSelectionNodeFront(comment, inspector);
  yield checkClipboard("<!-- Comment -->", root);

  info("Test copy outerHTML for DOCTYPE node");
  let doctype = getElementByType(inspector, Ci.nsIDOMNode.DOCUMENT_TYPE_NODE);
  yield setSelectionNodeFront(doctype, inspector);
  yield checkClipboard("<!DOCTYPE html>", root);

  info("Test copy outerHTML for ELEMENT node");
  yield selectAndHighlightNode("div", inspector);
  yield checkClipboard("<div><p>Test copy OuterHTML</p></div>", root);
});

function* setSelectionNodeFront(node, inspector) {
  let updated = inspector.once("inspector-updated");
  inspector.selection.setNodeFront(node);
  yield updated;
}

function* checkClipboard(expectedText, node) {
  try {
    yield waitForClipboard(() => fireCopyEvent(node), expectedText);
    ok(true, "Clipboard successfully filled with : " + expectedText);
  } catch (e) {
    ok(false, "Clipboard could not be filled with the expected text : " +
              expectedText);
  }
}

function getElementByType(inspector, type) {
  for (let [node] of inspector.markup._containers) {
    if (node.nodeType === type) {
      return node;
    }
  }
  return null;
}
