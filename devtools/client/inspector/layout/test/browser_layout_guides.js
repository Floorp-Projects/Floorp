/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that hovering over regions in the box-model shows the highlighter with
// the right options.
// Tests that actually check the highlighter is displayed and correct are in the
// devtools/inspector/test folder. This test only cares about checking that the
// layout-view does call the highlighter, and it does so by mocking it.

const STYLE = "div { position: absolute; top: 50px; left: 50px; height: 10px; " +
              "width: 10px; border: 10px solid black; padding: 10px; margin: 10px;}";
const HTML = "<style>" + STYLE + "</style><div></div>";
const TEST_URL = "data:text/html;charset=utf-8," + encodeURIComponent(HTML);

var highlightedNodeFront, highlighterOptions;

add_task(function*() {
  yield addTab(TEST_URL);
  let {toolbox, inspector, view} = yield openLayoutView();
  yield selectNode("div", inspector);

  // Mock the highlighter by replacing the showBoxModel method.
  toolbox.highlighter.showBoxModel = function(nodeFront, options) {
    highlightedNodeFront = nodeFront;
    highlighterOptions = options;
  }

  let elt = view.doc.getElementById("margins");
  yield testGuideOnLayoutHover(elt, "margin", inspector, view);

  elt = view.doc.getElementById("borders");
  yield testGuideOnLayoutHover(elt, "border", inspector, view);

  elt = view.doc.getElementById("padding");
  yield testGuideOnLayoutHover(elt, "padding", inspector, view);

  elt = view.doc.getElementById("content");
  yield testGuideOnLayoutHover(elt, "content", inspector, view);
});

function* testGuideOnLayoutHover(elt, expectedRegion, inspector, view) {
  info("Synthesizing mouseover on the layout-view");
  EventUtils.synthesizeMouse(elt, 2, 2, {type:'mouseover'},
    elt.ownerDocument.defaultView);

  info("Waiting for the node-highlight event from the toolbox");
  yield inspector.toolbox.once("node-highlight");

  is(highlightedNodeFront, inspector.selection.nodeFront,
    "The right nodeFront was highlighted");
  is(highlighterOptions.region, expectedRegion,
    "Region " + expectedRegion + " was highlighted");
}
