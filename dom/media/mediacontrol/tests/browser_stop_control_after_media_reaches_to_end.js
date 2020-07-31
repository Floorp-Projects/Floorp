/* eslint-disable no-undef */
const PAGE_URL =
  "https://example.com/browser/dom/media/mediacontrol/tests/file_non_looping_media.html";

/**
 * This test is used to ensure that we would stop controlling media after it
 * reaches to the end.
 */
add_task(async function testControlShouldStopAfterMediaReachesToTheEnd() {
  info(`open media page and play media until the end`);
  const tab = await createTabAndLoad(PAGE_URL);
  await Promise.all([
    checkIfMediaControllerBecomeInactiveAfterMediaEnds(tab),
    playMediaUntilItReachesToTheEnd(tab),
  ]);

  info(`remove tab`);
  await BrowserTestUtils.removeTab(tab);
});

/**
 * The following are helper functions.
 */
function checkIfMediaControllerBecomeInactiveAfterMediaEnds(tab) {
  return new Promise(r => {
    let activeChangedNums = 0;
    const controller = tab.linkedBrowser.browsingContext.mediaController;
    controller.onactivated = () => {
      is(
        ++activeChangedNums,
        1,
        `Receive ${activeChangedNums} times 'onactivechange'`
      );
      ok(controller.isActive, "controller should be active");
      ok(controller.isAudible, "controller should be audible");
      ok(controller.isPlaying, "controller should be playing");
    };
    controller.ondeactivated = () => {
      is(
        ++activeChangedNums,
        2,
        `Receive ${activeChangedNums} times 'onactivechange'`
      );
      ok(!controller.isActive, "controller should be inactive");
      ok(!controller.isAudible, "controller should be inaudible");
      ok(!controller.isPlaying, "controller should be paused");
      r();
    };
  });
}

function playMediaUntilItReachesToTheEnd(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    const video = content.document.getElementById("video");
    if (!video) {
      ok(false, "can't get video");
    }

    if (video.readyState < video.HAVE_METADATA) {
      info(`load media to get its duration`);
      video.load();
      await new Promise(r => (video.loadedmetadata = r));
    }

    info(`adjust the start position to faster reach to the end`);
    ok(video.duration > 1.0, "video's duration is larger than 1.0s");
    video.currentTime = video.duration - 1.0;

    info(`play ${video.id} video`);
    video.play();
    await new Promise(r => (video.onended = r));
  });
}
