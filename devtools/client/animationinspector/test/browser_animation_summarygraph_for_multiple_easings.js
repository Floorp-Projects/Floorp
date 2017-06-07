/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Test summary graph for animations that have multiple easing.
// There are two ways that we can set the easing for animations.
// One is effect easing, another one is keyframe easing for properties.
// The summary graph shows effect easing as dashed line if the easing is not 'linear'.
// If 'linear', does not show.
// Also, shows graph which combine the keyframe easings of all properties.

const TEST_CASES = {
  "no-easing": {
    expectedKeyframeEasingGraphs: [
      [
        { x: 0, y: 0 },
        { x: 25000, y: 0.25 },
        { x: 50000, y: 0.5 },
        { x: 75000, y: 0.75 },
        { x: 99000, y: 0.99 },
        { x: 100000, y: 1 },
        { x: 100000, y: 0 },
      ]
    ]
  },
  "effect-easing": {
    expectedEffectEasingGraph: [
      { x: 0, y: 0 },
      { x: 19999, y: 0.0 },
      { x: 20000, y: 0.25 },
      { x: 39999, y: 0.25 },
      { x: 40000, y: 0.5 },
      { x: 59999, y: 0.5 },
      { x: 60000, y: 0.75 },
      { x: 79999, y: 0.75 },
      { x: 80000, y: 1 },
      { x: 99999, y: 1 },
      { x: 100000, y: 0 },
    ],
    expectedKeyframeEasingGraphs: [
      [
        { x: 0, y: 0 },
        { x: 19999, y: 0.0 },
        { x: 20000, y: 0.25 },
        { x: 39999, y: 0.25 },
        { x: 40000, y: 0.5 },
        { x: 59999, y: 0.5 },
        { x: 60000, y: 0.75 },
        { x: 79999, y: 0.75 },
        { x: 80000, y: 1 },
        { x: 99999, y: 1 },
        { x: 100000, y: 0 },
      ]
    ]
  },
  "keyframe-easing": {
    expectedKeyframeEasingGraphs: [
      [
        { x: 0, y: 0 },
        { x: 49999, y: 0.0 },
        { x: 50000, y: 0.5 },
        { x: 99999, y: 0.5 },
        { x: 100000, y: 1 },
        { x: 100000, y: 0 },
      ]
    ]
  },
  "both-easing": {
    expectedEffectEasingGraph: [
      { x: 0, y: 0 },
      { x: 9999, y: 0.0 },
      { x: 10000, y: 0.1 },
      { x: 19999, y: 0.1 },
      { x: 20000, y: 0.2 },
      { x: 29999, y: 0.2 },
      { x: 30000, y: 0.3 },
      { x: 39999, y: 0.3 },
      { x: 40000, y: 0.4 },
      { x: 49999, y: 0.4 },
      { x: 50000, y: 0.5 },
      { x: 59999, y: 0.5 },
      { x: 60000, y: 0.6 },
      { x: 69999, y: 0.6 },
      { x: 70000, y: 0.7 },
      { x: 79999, y: 0.7 },
      { x: 80000, y: 0.8 },
      { x: 89999, y: 0.8 },
      { x: 90000, y: 0.9 },
      { x: 99999, y: 0.9 },
      { x: 100000, y: 0 },
    ],
    expectedKeyframeEasingGraphs: [
      [
        // KeyframeEffect::GetProperties returns sorted properties by the name.
        // Therefor, the test html, the 'marginLeft' is upper than 'opacity'.
        { x: 0, y: 0 },
        { x: 19999, y: 0.0 },
        { x: 20000, y: 0.2 },
        { x: 39999, y: 0.2 },
        { x: 40000, y: 0.4 },
        { x: 59999, y: 0.4 },
        { x: 60000, y: 0.6 },
        { x: 79999, y: 0.6 },
        { x: 80000, y: 0.8 },
        { x: 99999, y: 0.8 },
        { x: 100000, y: 1 },
        { x: 100000, y: 0 },
      ],
      [
        { x: 0, y: 0 },
        { x: 49999, y: 0.0 },
        { x: 50000, y: 0.5 },
        { x: 99999, y: 0.5 },
        { x: 100000, y: 1 },
        { x: 100000, y: 0 },
      ]
    ]
  }
};

add_task(function* () {
  yield addTab(URL_ROOT + "doc_multiple_easings.html");
  const { panel } = yield openAnimationInspector();
  const timelineComponent = panel.animationsTimelineComponent;
  timelineComponent.timeBlocks.forEach(timeBlock => {
    const state = timeBlock.animation.state;
    const testcase = TEST_CASES[state.name];
    if (!testcase) {
      return;
    }

    info(`Test effect easing graph of ${ state.name }`);
    const effectEl = timeBlock.containerEl.querySelector(".effect-easing");
    if (testcase.expectedEffectEasingGraph) {
      ok(effectEl, "<g> element for effect easing should exist");
      const pathEl = effectEl.querySelector("path");
      ok(pathEl, "<path> element for effect easing should exist");
      assertPathSegments(pathEl, state.duration, false,
                         testcase.expectedEffectEasingGraph);
    } else {
      ok(!effectEl, "<g> element for effect easing should not exist");
    }

    info(`Test keyframes easing graph of ${ state.name }`);
    const keyframeEls = timeBlock.containerEl.querySelectorAll(".keyframes-easing");
    const expectedKeyframeEasingGraphs = testcase.expectedKeyframeEasingGraphs;
    is(keyframeEls.length, expectedKeyframeEasingGraphs.length,
       `There should be ${ expectedKeyframeEasingGraphs.length } <g> elements `
       + `for keyframe easing`);
    expectedKeyframeEasingGraphs.forEach((expectedValues, index) => {
      const pathEl = keyframeEls[index].querySelector("path");
      ok(pathEl, "<path> element for keyframe easing should exist");
      assertPathSegments(pathEl, state.duration, false, expectedValues);
    });
  });
});
