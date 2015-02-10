/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that iframes are correctly highlighted.

const IFRAME_SRC = "<style>" +
    "body {" +
      "margin:0;" +
      "height:100%;" +
      "background-color:red" +
    "}" +
  "</style><body>hello from iframe</body>";

const DOCUMENT_SRC = "<style>" +
    "iframe {" +
      "height:200px;" +
      "border: 11px solid black;" +
      "padding: 13px;" +
    "}" +
    "body,iframe {" +
      "margin:0" +
    "}" +
  "</style>" +
  "<body>" +
   "<iframe src='data:text/html;charset=utf-8," + IFRAME_SRC + "'></iframe>" +
  "</body>";

const TEST_URI = "data:text/html;charset=utf-8," + DOCUMENT_SRC;

add_task(function* () {
  let { inspector, toolbox } = yield openInspectorForURL(TEST_URI);

  let iframeNode = getNode("iframe");

  info("Waiting for box mode to show.");
  let body = yield getNodeFront("body", inspector);
  yield toolbox.highlighter.showBoxModel(body);

  info("Waiting for element picker to become active.");
  yield toolbox.highlighterUtils.startPicker();

  info("Moving mouse over iframe padding.");
  yield moveMouseOver(iframeNode, 1, 1);

  info("Performing checks");
  yield isNodeCorrectlyHighlighted(iframeNode, toolbox);

  info("Scrolling the document");
  iframeNode.style.marginBottom = content.innerHeight + "px";
  content.scrollBy(0, 40);

  let iframeBodyNode = iframeNode.contentDocument.body;

  info("Moving mouse over iframe body");
  yield moveMouseOver(iframeNode, 40, 40);

  let highlightedNode = yield getHighlitNode(toolbox);
  is(highlightedNode, iframeBodyNode, "highlighter shows the right node");
  yield isNodeCorrectlyHighlighted(iframeBodyNode, toolbox);

  info("Waiting for the element picker to deactivate.");
  yield inspector.toolbox.highlighterUtils.stopPicker();

  function moveMouseOver(node, x, y) {
    info("Waiting for element " + node + " to be highlighted");
    executeInContent("Test:SynthesizeMouse", {x, y, options: {type: "mousemove"}},
                     {node}, false);
    return inspector.toolbox.once("picker-node-hovered");
  }
});
