"use strict";

// Disabling undef warning because in `run()` we use functions from head.js
/* eslint-disable no-undef */

/**
 * This test is used to ensure that Glean probe 'first_frame_loaded' can be
 * recorded correctly in different situations.
 */

const testCases = [
  {
    expected: {
      playback_type: "Non-MSE playback",
      video_codec: "video/avc",
      resolution: "AV,240<h<=480",
      key_system: undefined,
    },
    async run(tab) {
      await loadVideo(tab, "mozfirstframeloadedprobe");
    },
  },
  {
    expected: {
      playback_type: "MSE playback",
      video_codec: "video/avc",
      resolution: "AV,240<h<=480",
      key_system: undefined,
    },
    async run(tab) {
      await loadMseVideo(tab, "mozfirstframeloadedprobe");
    },
  },
  {
    expected: {
      playback_type: "EME playback",
      video_codec: "video/vp9",
      resolution: "V,240<h<=480",
      key_system: "org.w3.clearkey",
    },
    async run(tab) {
      await loadEmeVideo(tab, "mozfirstframeloadedprobe");
    },
  },
];

add_task(async function setTestPref() {
  await SpecialPowers.pushPrefEnv({
    set: [["media.testing-only-events", true]],
  });
});

add_task(async function testGleanMediaPlayackFirstFrameLoaded() {
  for (let test of testCases) {
    Services.fog.testResetFOG();

    const expected = test.expected;
    info(`running test for '${expected.playback_type}'`);
    const tab = await openTab();
    await test.run(tab);

    info(`waiting until glean probe is ready on the parent process`);
    await Services.fog.testFlushAllChildren();

    info("checking the collected results");
    const extra = Glean.mediaPlayback.firstFrameLoaded.testGetValue()[0].extra;
    Assert.greater(
      parseInt(extra.first_frame_loaded_time),
      0,
      `${extra.first_frame_loaded_time} is correct`
    );
    is(
      extra.playback_type,
      expected.playback_type,
      `${extra.playback_type} is correct`
    );
    is(
      extra.video_codec,
      expected.video_codec,
      `${extra.video_codec} is correct`
    );
    is(extra.resolution, expected.resolution, `${extra.resolution} is correct`);
    is(extra.key_system, expected.key_system, `${extra.key_system} is correct`);

    BrowserTestUtils.removeTab(tab);
  }
});
