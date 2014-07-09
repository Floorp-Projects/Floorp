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
   "<iframe src='data:text/html," + IFRAME_SRC + "'></iframe>" +
  "</body>";

const TEST_URI = "data:text/html;charset=utf-8," + DOCUMENT_SRC;

let test = asyncTest(function* () {
  let { inspector, toolbox } = yield openInspectorForURL(TEST_URI);
  let outerDocument = content.document;

  let iframeNode = getNode("iframe");
  let iframeBodyNode = getNode("body", { document: iframeNode.contentDocument });

  info("Waiting for box mode to show.");
  yield toolbox.highlighter.showBoxModel(getNodeFront(outerDocument.body));

  info("Waiting for element picker to become active.");
  yield toolbox.highlighterUtils.startPicker();

  info("Moving mouse over iframe padding.");
  yield moveMouseOver(iframeNode, 1, 1);

  info("Performing checks");
  isNodeCorrectlyHighlighted(iframeNode);

  info("Scrolling the document");
  iframeNode.style.marginBottom = outerDocument.defaultView.innerHeight + "px";
  outerDocument.defaultView.scrollBy(0, 40);

  info("Moving mouse over iframe body");
  yield moveMouseOver(iframeNode, 40, 40);

  is(getHighlitNode(), iframeBodyNode, "highlighter shows the right node");
  isNodeCorrectlyHighlighted(iframeBodyNode);

  info("Waiting for the element picker to deactivate.");
  yield inspector.toolbox.highlighterUtils.stopPicker();

  function moveMouseOver(aElement, x, y) {
    info("Waiting for element " + aElement + " to be highlighted");
    EventUtils.synthesizeMouse(aElement, x, y, {type: "mousemove"},
                               aElement.ownerDocument.defaultView);
    return inspector.toolbox.once("picker-node-hovered");
  }
});
