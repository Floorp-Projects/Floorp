/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the computed view refreshes when the current node has its style
// changed

const TESTCASE_URI = 'data:text/html;charset=utf-8,' +
                     '<div id="testdiv" style="font-size:10px;">Test div!</div>';

let test = asyncTest(function*() {
  yield addTab(TESTCASE_URI);

  info("Opening the computed view and selecting the test node");
  let {toolbox, inspector, view} = yield openComputedView();
  yield selectNode("#testdiv", inspector);

  let fontSize = getComputedViewPropertyValue(view, "font-size");
  is(fontSize, "10px", "The computed view shows the right font-size");

  info("Changing the node's style and waiting for the update");
  let onUpdated = inspector.once("computed-view-refreshed");
  getNode("#testdiv").style.cssText = "font-size: 15px; color: red;";
  yield onUpdated;

  fontSize = getComputedViewPropertyValue(view, "font-size");
  is(fontSize, "15px", "The computed view shows the updated font-size");
  let color = getComputedViewPropertyValue(view, "color");
  is(color, "#F00", "The computed view also shows the color now");
});
