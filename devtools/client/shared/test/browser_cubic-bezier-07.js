/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that changing the cubic-bezier curve in the widget does change the dot animation
// preview too.

const {CubicBezierWidget} = require("devtools/client/shared/widgets/CubicBezierWidget");
const {PREDEFINED} = require("devtools/client/shared/widgets/CubicBezierPresets");

const TEST_URI = CHROME_URL_ROOT + "doc_cubic-bezier-01.html";

add_task(async function() {
  const [host,, doc] = await createHost("bottom", TEST_URI);

  const container = doc.querySelector("#cubic-bezier-container");
  const w = new CubicBezierWidget(container, PREDEFINED.linear);

  await previewDotReactsToChanges(w, [0.6, -0.28, 0.74, 0.05]);
  await previewDotReactsToChanges(w, [0.9, 0.03, 0.69, 0.22]);
  await previewDotReactsToChanges(w, [0.68, -0.55, 0.27, 1.55]);
  await previewDotReactsToChanges(w, PREDEFINED.ease, "ease");
  await previewDotReactsToChanges(w, PREDEFINED["ease-in-out"], "ease-in-out");

  w.destroy();
  host.destroy();
});

async function previewDotReactsToChanges(widget, coords, expectedEasing) {
  const onUpdated = widget.once("updated");
  widget.coordinates = coords;
  await onUpdated;

  const animatedDot = widget.timingPreview.dot;
  const animations = animatedDot.getAnimations();

  if (!expectedEasing) {
    expectedEasing =
      `cubic-bezier(${coords[0]}, ${coords[1]}, ${coords[2]}, ${coords[3]})`;
  }

  is(animations.length, 1, "The dot is animated");

  const goingToRight = animations[0].effect.getKeyframes()[2];
  is(goingToRight.easing, expectedEasing,
     `The easing when going to the right was set correctly to ${coords}`);

  const goingToLeft = animations[0].effect.getKeyframes()[6];
  is(goingToLeft.easing, expectedEasing,
     `The easing when going to the left was set correctly to ${coords}`);
}
