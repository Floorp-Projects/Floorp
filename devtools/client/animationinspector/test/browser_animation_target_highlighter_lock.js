/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Test that the DOM element targets displayed in animation player widgets can
// be used to highlight elements in the DOM and select them in the inspector.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");
  let {toolbox, inspector, panel} = yield openAnimationInspector();

  let targets = panel.animationsTimelineComponent.targetNodes;

  info("Click on the highlighter icon for the first animated node");
  yield lockHighlighterOn(targets[0]);
  ok(targets[0].highlightNodeEl.classList.contains("selected"),
     "The highlighter icon is selected");

  info("Click on the highlighter icon for the second animated node");
  yield lockHighlighterOn(targets[1]);
  ok(targets[1].highlightNodeEl.classList.contains("selected"),
     "The highlighter icon is selected");
  ok(!targets[0].highlightNodeEl.classList.contains("selected"),
     "The highlighter icon for the first node is unselected");

  info("Click again to unhighlight");
  yield unlockHighlighterOn(targets[1]);
  ok(!targets[1].highlightNodeEl.classList.contains("selected"),
     "The highlighter icon for the second node is unselected");
});

function* lockHighlighterOn(targetComponent) {
  let onLocked = targetComponent.once("target-highlighter-locked");
  clickOnHighlighterIcon(targetComponent);
  yield onLocked;
}

function* unlockHighlighterOn(targetComponent) {
  let onUnlocked = targetComponent.once("target-highlighter-unlocked");
  clickOnHighlighterIcon(targetComponent);
  yield onUnlocked;
}

function clickOnHighlighterIcon(targetComponent) {
  let lockEl = targetComponent.highlightNodeEl;
  EventUtils.sendMouseEvent({type: "click"}, lockEl,
                            lockEl.ownerDocument.defaultView);
}
