/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Animation.currentTime ignores neagtive delay and positive/negative endDelay
// during fill-mode, even if they are set.
// For example, when the animation timing is
// { duration: 1000, iterations: 1, endDelay: -500, easing: linear },
// the animation progress is 0.5 at 700ms because the progress stops as 0.5 at
// 500ms in original animation. However, if you set as
// animation.currentTime = 700 manually, the progress will be 0.7.
// So we modify setCurrentTime method since
// AnimationInspector should re-produce same as original animation.
// In these tests,
// we confirm the behavior of setCurrentTime by delay and endDelay.

add_task(async function() {
  await addTab(URL_ROOT + "doc_timing_combination_animation.html");
  const { panel, controller } = await openAnimationInspector();

  await clickTimelinePlayPauseButton(panel);

  const timeBlockComponents = getAnimationTimeBlocks(panel);

  // Test -5000ms.
  let time = -5000;
  await controller.setCurrentTimeAll(time, true);
  for (let i = 0; i < timeBlockComponents.length; i++) {
    await timeBlockComponents[i].animation.refreshState();
    const state = await timeBlockComponents[i].animation.state;
    info(`Check the state at ${ time }ms with `
         + `delay:${ state.delay } and endDelay:${ state.endDelay }`);
    is(state.currentTime, 0,
       `The currentTime should be 0 at setCurrentTime(${ time })`);
  }

  // Test 10000ms.
  time = 10000;
  await controller.setCurrentTimeAll(time, true);
  for (let i = 0; i < timeBlockComponents.length; i++) {
    await timeBlockComponents[i].animation.refreshState();
    const state = await timeBlockComponents[i].animation.state;
    info(`Check the state at ${ time }ms with `
         + `delay:${ state.delay } and endDelay:${ state.endDelay }`);
    const expected = state.delay < 0 ? 0 : time;
    is(state.currentTime, expected,
       `The currentTime should be ${ expected } at setCurrentTime(${ time }).`
       + ` delay: ${ state.delay } and endDelay: ${ state.endDelay }`);
  }

  // Test 60000ms.
  time = 60000;
  await controller.setCurrentTimeAll(time, true);
  for (let i = 0; i < timeBlockComponents.length; i++) {
    await timeBlockComponents[i].animation.refreshState();
    const state = await timeBlockComponents[i].animation.state;
    info(`Check the state at ${ time }ms with `
         + `delay:${ state.delay } and endDelay:${ state.endDelay }`);
    const expected = state.delay < 0 ? time + state.delay : time;
    is(state.currentTime, expected,
       `The currentTime should be ${ expected } at setCurrentTime(${ time }).`
       + ` delay: ${ state.delay } and endDelay: ${ state.endDelay }`);
  }

  // Test 150000ms.
  time = 150000;
  await controller.setCurrentTimeAll(time, true);
  for (let i = 0; i < timeBlockComponents.length; i++) {
    await timeBlockComponents[i].animation.refreshState();
    const state = await timeBlockComponents[i].animation.state;
    info(`Check the state at ${ time }ms with `
         + `delay:${ state.delay } and endDelay:${ state.endDelay }`);
    const currentTime = state.delay < 0 ? time + state.delay : time;
    const endTime =
      state.delay + state.iterationCount * state.duration + state.endDelay;
    const expected =
      state.endDelay < 0 && state.fill === "both" && currentTime > endTime
      ? endTime : currentTime;
    is(state.currentTime, expected,
       `The currentTime should be ${ expected } at setCurrentTime(${ time }).`
       + ` delay: ${ state.delay } and endDelay: ${ state.endDelay }`);
  }
});
