/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether the layout of graphs were broken by seek and resume.

add_task(async function() {
  await addTab(URL_ROOT + "doc_multi_timings.html");
  await removeAnimatedElementsExcept([".delay-positive", ".delay-negative",
                                      ".enddelay-positive", ".enddelay-negative"]);
  const { animationInspector, panel } = await openAnimationInspector();

  info("Get initial coordinates result as test data");
  const initialCoordinatesResult = [];

  for (const itemEl of panel.querySelectorAll(".animation-item")) {
    const svgEl = itemEl.querySelector("svg");
    const svgViewBoxX = svgEl.viewBox.baseVal.x;
    const svgViewBoxWidth = svgEl.viewBox.baseVal.width;

    const pathEl = svgEl.querySelector(".animation-computed-timing-path");
    const pathX = pathEl.transform.baseVal[0].matrix.e;

    const delayEl = itemEl.querySelector(".animation-delay-sign");
    let delayX = null;
    let delayWidth = null;

    if (delayEl) {
      const computedStyle = delayEl.ownerGlobal.getComputedStyle(delayEl);
      delayX = computedStyle.left;
      delayWidth = computedStyle.width;
    }

    const endDelayEl = itemEl.querySelector(".animation-end-delay-sign");
    let endDelayX = null;
    let endDelayWidth = null;

    if (endDelayEl) {
      const computedStyle = endDelayEl.ownerGlobal.getComputedStyle(endDelayEl);
      endDelayX = computedStyle.left;
      endDelayWidth = computedStyle.width;
    }

    const coordinates = { svgViewBoxX, svgViewBoxWidth,
                          pathX, delayX, delayWidth, endDelayX, endDelayWidth };
    initialCoordinatesResult.push(coordinates);
  }

  info("Set currentTime to rear of the end of animation of .delay-negative.");
  await clickOnCurrentTimeScrubberController(animationInspector, panel, 0.75);
  info("Resume animations");
  await clickOnPauseResumeButton(animationInspector, panel);

  info("Check the layout");
  const itemEls = panel.querySelectorAll(".animation-item");
  is(itemEls.length, initialCoordinatesResult.length,
    "Count of animation item should be same to initial items");

  info("Check the coordinates");
  checkExpectedCoordinates(itemEls, initialCoordinatesResult);
});

function checkExpectedCoordinates(itemEls, initialCoordinatesResult) {
  for (let i = 0; i < itemEls.length; i++) {
    const expectedCoordinates = initialCoordinatesResult[i];
    const itemEl = itemEls[i];
    const svgEl = itemEl.querySelector("svg");
    is(svgEl.viewBox.baseVal.x, expectedCoordinates.svgViewBoxX,
      "X of viewBox of svg should be same");
    is(svgEl.viewBox.baseVal.width, expectedCoordinates.svgViewBoxWidth,
      "Width of viewBox of svg should be same");

    const pathEl = svgEl.querySelector(".animation-computed-timing-path");
    is(pathEl.transform.baseVal[0].matrix.e, expectedCoordinates.pathX,
      "X of tansform of path element should be same");

    const delayEl = itemEl.querySelector(".animation-delay-sign");

    if (delayEl) {
      const computedStyle = delayEl.ownerGlobal.getComputedStyle(delayEl);
      is(computedStyle.left, expectedCoordinates.delayX,
        "X of delay sign should be same");
      is(computedStyle.width, expectedCoordinates.delayWidth,
        "Width of delay sign should be same");
    } else {
      ok(!expectedCoordinates.delayX, "X of delay sign should exist");
      ok(!expectedCoordinates.delayWidth, "Width of delay sign should exist");
    }

    const endDelayEl = itemEl.querySelector(".animation-end-delay-sign");

    if (endDelayEl) {
      const computedStyle = endDelayEl.ownerGlobal.getComputedStyle(endDelayEl);
      is(computedStyle.left, expectedCoordinates.endDelayX,
        "X of endDelay sign should be same");
      is(computedStyle.width, expectedCoordinates.endDelayWidth,
        "Width of endDelay sign should be same");
    } else {
      ok(!expectedCoordinates.endDelayX, "X of endDelay sign should exist");
      ok(!expectedCoordinates.endDelayWidth, "Width of endDelay sign should exist");
    }
  }
}
