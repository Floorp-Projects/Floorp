/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following PlaybackRateSelector component:
// * element existence
// * make playback rate of animations by the selector
// * in case of animations have mixed playback rate
// * in case of animations have playback rate which is not default selectable value

add_task(async function() {
  await addTab(URL_ROOT + "doc_custom_playback_rate.html");
  const {
    animationInspector,
    inspector,
    panel,
  } = await openAnimationInspector();

  info("Checking playback rate selector existence");
  const selectEl = panel.querySelector(".playback-rate-selector");
  ok(selectEl, "scrubber controller should exist");

  info(
    "Checking playback rate existence which includes custom rate of animations"
  );
  const expectedPlaybackRates = [0.1, 0.25, 0.5, 1, 1.5, 2, 5, 10];
  assertPlaybackRateOptions(selectEl, expectedPlaybackRates);

  info("Checking selected playback rate");
  is(Number(selectEl.value), 1.5, "Selected option should be 1.5");

  info("Checking playback rate of animations");
  await clickOnPlaybackRateSelector(animationInspector, panel, 0.5);
  assertPlaybackRate(animationInspector, 0.5);

  info("Checking mixed playback rate");
  await selectNodeAndWaitForAnimations("div", inspector);
  await clickOnPlaybackRateSelector(animationInspector, panel, 2);
  assertPlaybackRate(animationInspector, 2);
  await selectNodeAndWaitForAnimations("body", inspector);
  is(selectEl.value, "", "Selected option should be empty");

  info("Checking playback rate after re-setting");
  await clickOnPlaybackRateSelector(animationInspector, panel, 1);
  assertPlaybackRate(animationInspector, 1);

  info(
    "Checking whether custom playback rate exist " +
      "after selecting another playback rate"
  );
  assertPlaybackRateOptions(selectEl, expectedPlaybackRates);
});

async function assertPlaybackRate(animationInspector, rate) {
  const isRateEqual = animationInspector.state.animations.every(
    ({ state }) => state.playbackRate === rate
  );
  ok(isRateEqual, `Playback rate of animations should be ${rate}`);
}

function assertPlaybackRateOptions(selectEl, expectedPlaybackRates) {
  is(
    selectEl.options.length,
    expectedPlaybackRates.length,
    `Length of options should be ${expectedPlaybackRates.length}`
  );

  for (let i = 0; i < selectEl.options.length; i++) {
    const optionEl = selectEl.options[i];
    const expectedPlaybackRate = expectedPlaybackRates[i];
    is(
      Number(optionEl.value),
      expectedPlaybackRate,
      `Option of index[${i}] should be ${expectedPlaybackRate}`
    );
  }
}
