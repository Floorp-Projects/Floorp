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

add_task(async function() {
  await addTab(URL_ROOT + "doc_keyframes.html");
  const {panel} = await openAnimationInspector();
  const timeline = panel.animationsTimelineComponent;

  // doc_keyframes.html has only one animation.
  // So we don't need to click the animation since already the animation detail shown.

  ok(timeline.rootWrapperEl.querySelectorAll(".frames .keyframes").length,
     "There are container elements for displaying keyframes");

  const data = await getExpectedKeyframesData(timeline.animations[0]);
  for (const propertyName in data) {
    info("Check the keyframe markers for " + propertyName);
    const widthMarkerSelector = ".frame[data-property=" + propertyName + "]";
    const markers = timeline.rootWrapperEl.querySelectorAll(widthMarkerSelector);

    is(markers.length, data[propertyName].length,
       "The right number of keyframes was found for " + propertyName);

    const offsets = [...markers].map(m => parseFloat(m.dataset.offset));
    const values = [...markers].map(m => m.dataset.value);
    for (let i = 0; i < markers.length; i++) {
      is(markers[i].dataset.offset, offsets[i],
         "Marker " + i + " for " + propertyName + " has the right offset");
      is(markers[i].dataset.value, values[i],
         "Marker " + i + " for " + propertyName + " has the right value");
    }
  }
});

async function getExpectedKeyframesData(animation) {
  // We're testing the UI state here, so it's fine to get the list of expected
  // properties from the animation actor.
  const properties = await animation.getProperties();
  const data = {};

  for (const expectedProperty of EXPECTED_PROPERTIES) {
    data[expectedProperty] = [];
    for (const {name, values} of properties) {
      if (name !== expectedProperty) {
        continue;
      }
      for (const {offset, value} of values) {
        data[expectedProperty].push({offset, value});
      }
    }
  }

  return data;
}
