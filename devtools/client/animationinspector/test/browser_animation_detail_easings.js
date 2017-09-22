/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(5);

// This is a test for displaying the easing of keyframes.
// Checks easing text which is displayed as popup and
// the path which emphasises by mouseover.

const TEST_CASES = {
  "no-easing": {
    opacity: {
      expectedValues: ["linear"],
    }
  },
  "effect-easing": {
    opacity: {
      expectedValues: ["linear"],
    }
  },
  "keyframe-easing": {
    opacity: {
      expectedValues: ["steps(2)"],
    }
  },
  "both-easing": {
    opacity: {
      expectedValues: ["steps(2)"],
    }
  },
  "many-keyframes": {
    backgroundColor: {
      selector: "rect",
      expectedValues: ["steps(2)", "ease-out"],
      noEmphasisPath: true,
    },
    opacity: {
      expectedValues: ["steps(2)", "ease-in", "linear", "ease-out"],
    }
  },
  "css-animations": {
    opacity: {
      expectedValues: ["ease", "ease"],
    }
  },
};

add_task(function* () {
  yield addTab(URL_ROOT + "doc_multiple_easings.html");
  const { panel } = yield openAnimationInspector();

  const timelineComponent = panel.animationsTimelineComponent;
  const timeBlocks = getAnimationTimeBlocks(panel);
  for (let i = 0; i < timeBlocks.length; i++) {
    yield clickOnAnimation(panel, i);

    const detailComponent = timelineComponent.details;
    const detailEl = detailComponent.containerEl;
    const state = detailComponent.animation.state;

    const testcase = TEST_CASES[state.name];
    if (!testcase) {
      continue;
    }

    // Expand detail pane height to send mouseover event correct to tested path element.
    // This is because depending on the environment the target element may be out of
    // the window.
    detailEl.closest(".controlled").style.height = "100%";

    for (let testProperty in testcase) {
      const testIdentity = `"${ testProperty }" of "${ state.name }"`;
      info(`Test for ${ testIdentity }`);

      const testdata = testcase[testProperty];
      const selector = testdata.selector ? testdata.selector : `.${testProperty}`;
      const hintEls = detailEl.querySelectorAll(`${ selector }.hint`);
      const expectedValues = testdata.expectedValues;
      is(hintEls.length, expectedValues.length,
         `Length of hints for ${ testIdentity } should be ${expectedValues.length}`);

      for (let j = 0; j < hintEls.length; j++) {
        const hintEl = hintEls[j];
        const expectedValue = expectedValues[j];

        info("Test easing text");
        const gEl = hintEl.closest("g");
        ok(gEl, `<g> element for ${ testIdentity } should exists`);
        const titleEl = gEl.querySelector("title");
        ok(titleEl, `<title> element for ${ testIdentity } should exists`);
        is(titleEl.textContent, expectedValue,
           `textContent of <title> for ${ testIdentity } should be ${ expectedValue }`);

        info("Test emphasis path");
        const win = hintEl.ownerDocument.defaultView;
        // Mouse out once from hintEl.
        EventUtils.synthesizeMouse(hintEl, -1, -1, {type: "mouseout"}, win);
        is(win.getComputedStyle(hintEl).strokeOpacity, 0,
           `stroke-opacity of hintEl for ${ testIdentity } should be 0 ` +
           `while mouse is out from the element`);
        // Mouse is over the hintEl.
        EventUtils.synthesizeMouseAtCenter(hintEl, {type: "mouseover"}, win);
        if (testdata.noEmphasisPath) {
          is(win.getComputedStyle(hintEl).strokeOpacity, 0,
             `stroke-opacity of hintEl for ${ testIdentity } should be 0 ` +
             `even while mouse is over the element`);
        } else {
          is(win.getComputedStyle(hintEl).strokeOpacity, 1,
             `stroke-opacity of hintEl for ${ testIdentity } should be 1 ` +
             `while mouse is over the element`);
        }
      }
    }
  }
});
