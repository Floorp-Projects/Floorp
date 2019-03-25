/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_DATA = [
  {
    propertyName: "caret-color",
    expectedMarkers: ["auto", "rgb(0, 255, 0)"],
  },
  {
    propertyName: "scrollbar-color",
    expectedMarkers: ["rgb(0, 255, 0) rgb(255, 0, 0)", "auto"],
  },
];

// Test for animatable property which can specify the non standard CSS color value.
add_task(async function() {
  await addTab(URL_ROOT + "doc_special_colors.html");
  const { panel } = await openAnimationInspector();

  for (const { propertyName, expectedMarkers } of TEST_DATA) {
    const animatedPropertyEl = panel.querySelector(`.${ propertyName }`);
    ok(animatedPropertyEl, `Animated property ${ propertyName } exists`);

    const markerEls = animatedPropertyEl.querySelectorAll(".keyframe-marker-item");
    is(markerEls.length, expectedMarkers.length,
       `The length of keyframe markers should ${ expectedMarkers.length }`);
    for (let i = 0; i < expectedMarkers.length; i++) {
      const actualTitle = markerEls[i].title;
      const expectedTitle = expectedMarkers[i];
      is(actualTitle, expectedTitle, `Value of keyframes[${ i }] is correct`);
    }
  }
});
