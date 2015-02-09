/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that highlighter handles geometry changes correctly.

const TEST_URI = "data:text/html;charset=utf-8," +
  "browser_inspector_invalidate.js\n" +
  "<div style=\"width: 100px; height: 100px; background:yellow;\"></div>";

add_task(function*() {
  let {toolbox, inspector} = yield openInspectorForURL(TEST_URI);
  let div = getNode("div");
  let divFront = yield getNodeFront("div", inspector);

  info("Waiting for highlighter to activate");
  yield inspector.toolbox.highlighter.showBoxModel(divFront);

  let rect = yield getSimpleBorderRect(toolbox);
  is(rect.width, 100, "The highlighter has the right width.");

  info("Changing the test element's size and waiting for the highlighter to update");
  let {actorID, connPrefix} = getHighlighterActorID(toolbox.highlighter);
  yield executeInContent("Test:ChangeHighlightedNodeWaitForUpdate", {
    name: "style",
    value: "width: 200px; height: 100px; background:yellow;",
    actorID,
    connPrefix
  });

  rect = yield getSimpleBorderRect(toolbox);
  is(rect.width, 200, "The highlighter has the right width after update");

  info("Waiting for highlighter to hide");
  yield inspector.toolbox.highlighter.hideBoxModel();
});
