/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that highlighter handles geometry changes correctly.
const TEST_URI = "data:text/html;charset=utf-8," +
  "browser_inspector_invalidate.js\n" +
  "<div style=\"width: 100px; height: 100px; background:yellow;\"></div>";

let test = asyncTest(function*() {
  let { inspector } = yield openInspectorForURL(TEST_URI);
  let div = getNode("div");

  info("Waiting for highlighter to activate");
  yield inspector.toolbox.highlighter.showBoxModel(getNodeFront(div));

  let rect = getSimpleBorderRect();
  is(rect.width, 100, "Outline has the right width.");

  info("Changing the test element's size");
  let boxModelUpdated = waitForBoxModelUpdate();
  div.style.width = "200px";

  info("Waiting for the box model to update.");
  yield boxModelUpdated;

  rect = getSimpleBorderRect();
  is(rect.width, 200, "Outline has the right width after update.");

  info("Waiting for highlighter to hide");
  yield inspector.toolbox.highlighter.hideBoxModel();
});
