/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that when an animation is selected and its list of properties is shown,
// there are keyframes markers next to each property being animated.

const EXPECTED_PROPERTIES = [
  "backgroundColor",
  "backgroundPosition",
  "backgroundSize",
  "borderBottomLeftRadius",
  "borderBottomRightRadius",
  "borderTopLeftRadius",
  "borderTopRightRadius",
  "filter",
  "height",
  "transform",
  "width"
];

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_keyframes.html");
  let {panel} = yield openAnimationInspector();
  let timeline = panel.animationsTimelineComponent;

  info("Expand the animation");
  yield clickOnAnimation(panel, 0);

  ok(timeline.rootWrapperEl.querySelectorAll(".frames .keyframes").length,
     "There are container elements for displaying keyframes");

  let data = yield getExpectedKeyframesData(timeline.animations[0]);
  for (let propertyName in data) {
    info("Check the keyframe markers for " + propertyName);
    let widthMarkerSelector = ".frame[data-property=" + propertyName + "]";
    let markers = timeline.rootWrapperEl.querySelectorAll(widthMarkerSelector);

    is(markers.length, data[propertyName].length,
       "The right number of keyframes was found for " + propertyName);

    let offsets = [...markers].map(m => parseFloat(m.dataset.offset));
    let values = [...markers].map(m => m.dataset.value);
    for (let i = 0; i < markers.length; i++) {
      is(markers[i].dataset.offset, offsets[i],
         "Marker " + i + " for " + propertyName + " has the right offset");
      is(markers[i].dataset.value, values[i],
         "Marker " + i + " for " + propertyName + " has the right value");
    }
  }
});

function* getExpectedKeyframesData(animation) {
  // We're testing the UI state here, so it's fine to get the list of expected
  // keyframes from the animation actor.
  let frames = yield animation.getFrames();
  let data = {};

  for (let property of EXPECTED_PROPERTIES) {
    data[property] = [];
    for (let frame of frames) {
      if (typeof frame[property] !== "undefined") {
        data[property].push({
          offset: frame.computedOffset,
          value: frame[property]
        });
      }
    }
  }

  return data;
}
