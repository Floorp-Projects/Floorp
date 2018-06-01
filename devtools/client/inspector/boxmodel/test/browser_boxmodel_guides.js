/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that hovering over regions in the box-model shows the highlighter with
// the right options.
// Tests that actually check the highlighter is displayed and correct are in the
// devtools/inspector/test folder. This test only cares about checking that the
// box model view does call the highlighter, and it does so by mocking it.

const STYLE = "div { position: absolute; top: 50px; left: 50px; " +
              "height: 10px; width: 10px; border: 10px solid black; " +
              "padding: 10px; margin: 10px;}";
const HTML = "<style>" + STYLE + "</style><div></div>";
const TEST_URL = "data:text/html;charset=utf-8," + encodeURIComponent(HTML);

var highlightedNodeFront, highlighterOptions;

add_task(async function() {
  await addTab(TEST_URL);
  const {toolbox, inspector, boxmodel} = await openLayoutView();
  await selectNode("div", inspector);

  // Mock the highlighter by replacing the showBoxModel method.
  toolbox.highlighter.showBoxModel = function(nodeFront, options) {
    highlightedNodeFront = nodeFront;
    highlighterOptions = options;
  };

  let elt = boxmodel.document.querySelector(".boxmodel-margins");
  await testGuideOnLayoutHover(elt, "margin", inspector);

  elt = boxmodel.document.querySelector(".boxmodel-borders");
  await testGuideOnLayoutHover(elt, "border", inspector);

  elt = boxmodel.document.querySelector(".boxmodel-paddings");
  await testGuideOnLayoutHover(elt, "padding", inspector);

  elt = boxmodel.document.querySelector(".boxmodel-content");
  await testGuideOnLayoutHover(elt, "content", inspector);
});

async function testGuideOnLayoutHover(elt, expectedRegion, inspector) {
  info("Synthesizing mouseover on the boxmodel-view");
  EventUtils.synthesizeMouse(elt, 50, 2, {type: "mouseover"},
    elt.ownerDocument.defaultView);

  info("Waiting for the node-highlight event from the toolbox");
  await inspector.toolbox.once("node-highlight");

  // Wait for the next event tick to make sure the remaining part of the
  // test is executed after finishing synthesizing mouse event.
  await new Promise(executeSoon);

  is(highlightedNodeFront, inspector.selection.nodeFront,
    "The right nodeFront was highlighted");
  is(highlighterOptions.region, expectedRegion,
    "Region " + expectedRegion + " was highlighted");
}
