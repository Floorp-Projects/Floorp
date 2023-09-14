"use strict";

/**
 * This test aims to ensure that the media engine playback will recover from a
 * crash and keep playing without any problem.
 */
add_task(async function setupTestingPref() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.wmf.media-engine.enabled", 1],
      ["media.wmf.media-engine.channel-decoder.enabled", true],
    ],
  });
});

const VIDEO_PAGE = GetTestWebBasedURL("file_video.html");

add_task(async function testPlaybackRecoveryFromCrash() {
  info(`Create a tab and load test page`);
  let tab = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    "about:blank"
  );
  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, VIDEO_PAGE);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  await playVideo(tab);

  info("Ensure video is running via the media engine framework");
  await assertRunningProcessAndDecoderName(tab, {
    expectedProcess: "Utility MF Media Engine CDM",
    expectedDecoder: "media engine video stream",
  });

  const pidBeforeCrash = await getMFCDMProcessId();
  await crashUtilityProcess(pidBeforeCrash);

  info("The CDM process should be recreated which makes media keep playing");
  await assertRunningProcessAndDecoderName(tab, {
    expectedProcess: "Utility MF Media Engine CDM",
    expectedDecoder: "media engine video stream",
  });

  const pidAfterCrash = await getMFCDMProcessId();
  isnot(
    pidBeforeCrash,
    pidAfterCrash,
    `new process ${pidAfterCrash} is not previous crashed one ${pidBeforeCrash}`
  );

  BrowserTestUtils.removeTab(tab);
});
