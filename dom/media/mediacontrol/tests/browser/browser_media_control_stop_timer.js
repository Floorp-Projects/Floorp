// Import this in order to use `triggerPictureInPicture()`.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/pictureinpicture/tests/head.js",
  this
);

const PAGE_NON_AUTOPLAY =
  "https://example.com/browser/dom/media/mediacontrol/tests/browser/file_non_autoplay.html";

const testVideoId = "video";

add_task(async function setupTestingPref() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.mediacontrol.testingevents.enabled", true],
      ["media.mediacontrol.stopcontrol.timer", true],
      ["media.mediacontrol.stopcontrol.timer.ms", 0],
    ],
  });
});

/**
 * This test is used to check the stop timer for media element, which would stop
 * media control for the specific element when the element has been paused over
 * certain length of time. (That is controlled by the value of the pref
 * `media.mediacontrol.stopcontrol.timer.ms`) In this test, we set the pref to 0
 * which means the stop timer would be triggered after the media is paused.
 * However, if the media is being used in PIP mode, we won't start the stop
 * timer for it.
 */
add_task(async function testStopMediaControlAfterPausingMedia() {
  info(`open media page`);
  const tab = await createLoadedTabWrapper(PAGE_NON_AUTOPLAY);

  info(`start media`);
  await playMedia(tab, testVideoId);

  info(`pause media and the stop timer would stop media control`);
  await pauseMediaAndMediaControlShouldBeStopped(tab, testVideoId);

  info(`remove tab`);
  await tab.close();
});

add_task(async function testNotToStopMediaControlForPIPVideo() {
  info(`open media page`);
  const tab = await createLoadedTabWrapper(PAGE_NON_AUTOPLAY);

  info(`start media`);
  await playMedia(tab, testVideoId);

  info(`trigger PIP mode`);
  const winPIP = await triggerPictureInPicture(tab.linkedBrowser, testVideoId);

  info(`pause media and the stop timer would not stop media control`);
  await pauseMedia(tab, testVideoId);

  info(`pressing 'play' key should start PIP video again`);
  await generateMediaControlKeyEvent("play");
  await checkOrWaitUntilMediaStartedPlaying(tab, testVideoId);

  info(`remove tab`);
  await BrowserTestUtils.closeWindow(winPIP);
  await tab.close();
});

/**
 * The following is helper function.
 */
function pauseMediaAndMediaControlShouldBeStopped(tab, elementId) {
  // After pausing media, the stop timer would be triggered and stop the media
  // control.
  return Promise.all([
    new Promise(r => (tab.controller.ondeactivated = r)),
    SpecialPowers.spawn(tab.linkedBrowser, [elementId], Id => {
      content.document.getElementById(Id).pause();
    }),
  ]);
}
