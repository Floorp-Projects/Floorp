/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that iframes are correctly highlighted.

const IFRAME_SRC =
  "<style>" +
  "body {" +
  "margin:0;" +
  "height:100%;" +
  "background-color:red" +
  "}" +
  "</style><body>hello from iframe</body>";

const DOCUMENT_SRC =
  "<style>" +
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
  "<iframe src='data:text/html;charset=utf-8," +
  IFRAME_SRC +
  "'></iframe>" +
  "</body>";

const TEST_URI = "data:text/html;charset=utf-8," + DOCUMENT_SRC;

add_task(async function() {
  const { inspector, toolbox, testActor } = await openInspectorForURL(TEST_URI);

  info("Waiting for box mode to show.");
  const body = await getNodeFront("body", inspector);
  await inspector.highlighters.showHighlighterTypeForNode(
    inspector.highlighters.TYPES.BOXMODEL,
    body
  );

  info("Waiting for element picker to become active.");
  await startPicker(toolbox);

  info("Moving mouse over iframe padding.");
  await hoverElement(inspector, "iframe", 1, 1);

  info("Performing checks");
  await testActor.isNodeCorrectlyHighlighted("iframe", is);

  info("Scrolling the document");
  await testActor.setProperty("iframe", "style", "margin-bottom: 2000px");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () =>
    content.scrollBy(0, 40)
  );

  // target the body within the iframe
  const iframeBodySelector = ["iframe", "body"];

  info("Moving mouse over iframe body");
  await hoverElement(inspector, "iframe", 40, 40);

  ok(
    await testActor.assertHighlightedNode(iframeBodySelector),
    "highlighter shows the right node"
  );
  await testActor.isNodeCorrectlyHighlighted(iframeBodySelector, is);

  info("Waiting for the element picker to deactivate.");
  await toolbox.nodePicker.stop();
});
