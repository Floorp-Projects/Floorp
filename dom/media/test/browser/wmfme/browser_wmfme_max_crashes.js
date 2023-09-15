"use strict";

/**
 * This test aims to ensure that the MFCDM process won't be recovered once the
 * amount of crashes has exceeded the amount of value which we tolerate.
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
  const maxCrashes = Services.prefs.getIntPref(
    "media.wmf.media-engine.max-crashes"
  );
  info(`The amount of tolerable crashes=${maxCrashes}`);

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

  let pidBeforeCrash, pidAfterCrash;
  for (let idx = 0; idx < maxCrashes; idx++) {
    pidBeforeCrash = await getMFCDMProcessId();
    await crashUtilityProcess(pidBeforeCrash);

    info("The CDM process should be recreated which makes media keep playing");
    await assertRunningProcessAndDecoderName(tab, {
      expectedProcess: "Utility MF Media Engine CDM",
      expectedDecoder: "media engine video stream",
    });

    pidAfterCrash = await getMFCDMProcessId();
    isnot(
      pidBeforeCrash,
      pidAfterCrash,
      `new process ${pidAfterCrash} is not previous crashed one ${pidBeforeCrash}`
    );
  }

  info("This crash should result in not spawning MFCDM process again");
  pidBeforeCrash = await getMFCDMProcessId();
  await crashUtilityProcess(pidBeforeCrash);

  await assertNotEqualRunningProcessAndDecoderName(tab, {
    givenProcess: "Utility MF Media Engine CDM",
    givenDecoder: "media engine video stream",
  });

  BrowserTestUtils.removeTab(tab);
});
