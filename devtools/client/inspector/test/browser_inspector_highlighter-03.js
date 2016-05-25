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
  let { inspector, toolbox, testActor } = yield openInspectorForURL(TEST_URI);

  info("Waiting for box mode to show.");
  let body = yield getNodeFront("body", inspector);
  yield toolbox.highlighter.showBoxModel(body);

  info("Waiting for element picker to become active.");
  yield startPicker(toolbox);

  info("Moving mouse over iframe padding.");
  yield moveMouseOver("iframe", 1, 1);

  info("Performing checks");
  yield testActor.isNodeCorrectlyHighlighted("iframe", is);

  info("Scrolling the document");
  yield testActor.setProperty("iframe", "style", "margin-bottom: 2000px");
  yield testActor.eval("window.scrollBy(0, 40);");

  // target the body within the iframe
  let iframeBodySelector = ["iframe", "body"];

  info("Moving mouse over iframe body");
  yield moveMouseOver("iframe", 40, 40);

  ok((yield testActor.assertHighlightedNode(iframeBodySelector)),
     "highlighter shows the right node");
  yield testActor.isNodeCorrectlyHighlighted(iframeBodySelector, is);

  info("Waiting for the element picker to deactivate.");
  yield inspector.toolbox.highlighterUtils.stopPicker();

  function moveMouseOver(selector, x, y) {
    info("Waiting for element " + selector + " to be highlighted");
    testActor.synthesizeMouse({selector, x, y, options: {type: "mousemove"}});
    return inspector.toolbox.once("picker-node-hovered");
  }
});
