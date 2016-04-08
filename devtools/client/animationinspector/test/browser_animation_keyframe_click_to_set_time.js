/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that animated properties' keyframes can be clicked, and that doing so
// sets the current time in the timeline.

add_task(function* () {
  yield addTab(URL_ROOT + "doc_keyframes.html");
  let {panel} = yield openAnimationInspector();
  let timeline = panel.animationsTimelineComponent;
  let {scrubberEl} = timeline;

  // XXX: The scrollbar is placed in the timeline in such a way that it causes
  // the animations to be slightly offset with the header when it appears.
  // So for now, let's hide the scrollbar. Bug 1229340 should fix this.
  timeline.animationsEl.style.overflow = "hidden";

  info("Expand the animation");
  yield clickOnAnimation(panel, 0);

  info("Click on the first keyframe of the first animated property");
  yield clickKeyframe(panel, 0, "background-color", 0);

  info("Make sure the scrubber stopped moving and is at the right position");
  yield assertScrubberMoving(panel, false);
  checkScrubberPos(scrubberEl, 0);

  info("Click on a keyframe in the middle");
  yield clickKeyframe(panel, 0, "transform", 2);

  info("Make sure the scrubber is at the right position");
  checkScrubberPos(scrubberEl, 50);
});

function* clickKeyframe(panel, animIndex, property, index) {
  let keyframeComponent = getKeyframeComponent(panel, animIndex, property);
  let keyframeEl = getKeyframeEl(panel, animIndex, property, index);

  let onSelect = keyframeComponent.once("frame-selected");
  EventUtils.sendMouseEvent({type: "click"}, keyframeEl,
                            keyframeEl.ownerDocument.defaultView);
  yield onSelect;
}

function checkScrubberPos(scrubberEl, pos) {
  let newPos = Math.round(parseFloat(scrubberEl.style.left));
  let expectedPos = Math.round(pos);
  is(newPos, expectedPos, `The scrubber is at ${pos}%`);
}
