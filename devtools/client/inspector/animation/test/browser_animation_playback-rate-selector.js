/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following PlaybackRateSelector component:
// * element existence
// * make playback rate of animations by the selector
// * in case of animations have mixed playback rate
// * in case of animations have playback rate which is not default selectable value

add_task(async function () {
  await addTab(URL_ROOT + "doc_custom_playback_rate.html");
  const { animationInspector, inspector, panel } =
    await openAnimationInspector();

  info("Checking playback rate selector existence");
  const selectEl = panel.querySelector(".playback-rate-selector");
  ok(selectEl, "scrubber controller should exist");

  info(
    "Checking playback rate existence which includes custom rate of animations"
  );
  const expectedPlaybackRates = [0.1, 0.25, 0.5, 1, 1.5, 2, 5, 10];
  await assertPlaybackRateOptions(selectEl, expectedPlaybackRates);

  info("Checking selected playback rate");
  is(Number(selectEl.value), 1.5, "Selected option should be 1.5");

  info("Checking playback rate of animations");
  await changePlaybackRateSelector(animationInspector, panel, 0.5);
  await assertPlaybackRate(animationInspector, 0.5);

  info("Checking mixed playback rate");
  await selectNode("div", inspector);
  await waitUntil(() => panel.querySelectorAll(".animation-item").length === 1);
  await changePlaybackRateSelector(animationInspector, panel, 2);
  await assertPlaybackRate(animationInspector, 2);
  await selectNode("body", inspector);
  await waitUntil(() => panel.querySelectorAll(".animation-item").length === 2);
  await waitUntil(() => selectEl.value === "");
  ok(true, "Selected option should be empty");

  info("Checking playback rate after re-setting");
  await changePlaybackRateSelector(animationInspector, panel, 1);
  await assertPlaybackRate(animationInspector, 1);

  info(
    "Checking whether custom playback rate exist " +
      "after selecting another playback rate"
  );
  await assertPlaybackRateOptions(selectEl, expectedPlaybackRates);
});

async function assertPlaybackRate(animationInspector, rate) {
  await waitUntil(() =>
    animationInspector.state?.animations.every(
      ({ state }) => state.playbackRate === rate
    )
  );
  ok(true, `Playback rate of animations should be ${rate}`);
}

async function assertPlaybackRateOptions(selectEl, expectedPlaybackRates) {
  await waitUntil(() => {
    if (selectEl.options.length !== expectedPlaybackRates.length) {
      return false;
    }

    for (let i = 0; i < selectEl.options.length; i++) {
      const optionEl = selectEl.options[i];
      const expectedPlaybackRate = expectedPlaybackRates[i];
      if (Number(optionEl.value) !== expectedPlaybackRate) {
        return false;
      }
    }

    return true;
  });
  ok(true, "Content of playback rate options are correct");
}
