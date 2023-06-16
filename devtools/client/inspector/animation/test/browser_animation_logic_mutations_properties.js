/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether animation was changed after altering following properties.
// * delay
// * direction
// * duration
// * easing (animationTimingFunction in case of CSS Animationns)
// * fill
// * iterations
// * endDelay (script animation only)
// * iterationStart (script animation only)
// * playbackRate (script animation only)

const SEC = 1000;
const TEST_EFFECT_TIMING = {
  delay: 20 * SEC,
  direction: "reverse",
  duration: 20 * SEC,
  easing: "steps(1)",
  endDelay: 20 * SEC,
  fill: "backwards",
  iterations: 20,
  iterationStart: 20 * SEC,
};
const TEST_PLAYBACK_RATE = 0.1;

add_task(async function () {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  await removeAnimatedElementsExcept([".animated", ".end-delay"]);
  const { animationInspector } = await openAnimationInspector();
  await setCSSAnimationProperties(animationInspector);
  await assertProperties(animationInspector.state.animations[0], false);
  await setScriptAnimationProperties(animationInspector);
  await assertProperties(animationInspector.state.animations[1], true);
});

async function setCSSAnimationProperties(animationInspector) {
  const properties = {
    animationDelay: `${TEST_EFFECT_TIMING.delay}ms`,
    animationDirection: TEST_EFFECT_TIMING.direction,
    animationDuration: `${TEST_EFFECT_TIMING.duration}ms`,
    animationFillMode: TEST_EFFECT_TIMING.fill,
    animationIterationCount: TEST_EFFECT_TIMING.iterations,
    animationTimingFunction: TEST_EFFECT_TIMING.easing,
  };

  await setStyles(animationInspector, ".animated", properties);
}

async function setScriptAnimationProperties(animationInspector) {
  await setEffectTimingAndPlayback(
    animationInspector,
    ".end-delay",
    TEST_EFFECT_TIMING,
    TEST_PLAYBACK_RATE
  );
}

async function assertProperties(animation, isScriptAnimation) {
  await waitUntil(() => animation.state.delay === TEST_EFFECT_TIMING.delay);
  ok(true, `Delay should be ${TEST_EFFECT_TIMING.delay}`);

  await waitUntil(
    () => animation.state.direction === TEST_EFFECT_TIMING.direction
  );
  ok(true, `Direction should be ${TEST_EFFECT_TIMING.direction}`);

  await waitUntil(
    () => animation.state.duration === TEST_EFFECT_TIMING.duration
  );
  ok(true, `Duration should be ${TEST_EFFECT_TIMING.duration}`);

  await waitUntil(() => animation.state.fill === TEST_EFFECT_TIMING.fill);
  ok(true, `Fill should be ${TEST_EFFECT_TIMING.fill}`);

  await waitUntil(
    () => animation.state.iterationCount === TEST_EFFECT_TIMING.iterations
  );
  ok(true, `Iterations should be ${TEST_EFFECT_TIMING.iterations}`);

  if (isScriptAnimation) {
    await waitUntil(() => animation.state.easing === TEST_EFFECT_TIMING.easing);
    ok(true, `Easing should be ${TEST_EFFECT_TIMING.easing}`);

    await waitUntil(
      () => animation.state.iterationStart === TEST_EFFECT_TIMING.iterationStart
    );
    ok(true, `IterationStart should be ${TEST_EFFECT_TIMING.iterationStart}`);

    await waitUntil(() => animation.state.playbackRate === TEST_PLAYBACK_RATE);
    ok(true, `PlaybackRate should be ${TEST_PLAYBACK_RATE}`);
  } else {
    await waitUntil(
      () =>
        animation.state.animationTimingFunction === TEST_EFFECT_TIMING.easing
    );

    ok(true, `AnimationTimingFunction should be ${TEST_EFFECT_TIMING.easing}`);
  }
}
