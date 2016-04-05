/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Test that the DOM element targets displayed in animation player widgets can
// be used to highlight elements in the DOM and select them in the inspector.

add_task(function* () {
  yield addTab(URL_ROOT + "doc_simple_animation.html");
  let {panel} = yield openAnimationInspector();

  let targets = panel.animationsTimelineComponent.targetNodes;

  info("Click on the highlighter icon for the first animated node");
  let domNodePreview1 = targets[0].previewer;
  yield lockHighlighterOn(domNodePreview1);
  ok(domNodePreview1.highlightNodeEl.classList.contains("selected"),
     "The highlighter icon is selected");

  info("Click on the highlighter icon for the second animated node");
  let domNodePreview2 = targets[1].previewer;
  yield lockHighlighterOn(domNodePreview2);
  ok(domNodePreview2.highlightNodeEl.classList.contains("selected"),
     "The highlighter icon is selected");
  ok(!domNodePreview1.highlightNodeEl.classList.contains("selected"),
     "The highlighter icon for the first node is unselected");

  info("Click again to unhighlight");
  yield unlockHighlighterOn(domNodePreview2);
  ok(!domNodePreview2.highlightNodeEl.classList.contains("selected"),
     "The highlighter icon for the second node is unselected");
});

function* lockHighlighterOn(domNodePreview) {
  let onLocked = domNodePreview.once("target-highlighter-locked");
  clickOnHighlighterIcon(domNodePreview);
  yield onLocked;
}

function* unlockHighlighterOn(domNodePreview) {
  let onUnlocked = domNodePreview.once("target-highlighter-unlocked");
  clickOnHighlighterIcon(domNodePreview);
  yield onUnlocked;
}

function clickOnHighlighterIcon(domNodePreview) {
  let lockEl = domNodePreview.highlightNodeEl;
  EventUtils.sendMouseEvent({type: "click"}, lockEl,
                            lockEl.ownerDocument.defaultView);
}
