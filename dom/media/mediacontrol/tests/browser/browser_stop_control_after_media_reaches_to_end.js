const PAGE_URL =
  "https://example.com/browser/dom/media/mediacontrol/tests/browser/file_non_looping_media.html";

add_task(async function setupTestingPref() {
  await SpecialPowers.pushPrefEnv({
    set: [["media.mediacontrol.stopcontrol.aftermediaends", true]],
  });
});

/**
 * This test is used to ensure that we would stop controlling media after it
 * reaches to the end when a controller doesn't have an active media session.
 * If a controller has an active media session, it would keep active despite
 * media reaches to the end.
 */
add_task(async function testControllerShouldStopAfterMediaReachesToTheEnd() {
  info(`open media page and play media until the end`);
  const tab = await createLoadedTabWrapper(PAGE_URL);
  await Promise.all([
    checkIfMediaControllerBecomeInactiveAfterMediaEnds(tab),
    playMediaUntilItReachesToTheEnd(tab),
  ]);

  info(`remove tab`);
  await tab.close();
});

add_task(async function testControllerWontStopAfterMediaReachesToTheEnd() {
  info(`open media page and create media session`);
  const tab = await createLoadedTabWrapper(PAGE_URL);
  await createMediaSession(tab);

  info(`play media until the end`);
  await playMediaUntilItReachesToTheEnd(tab);

  info(`controller is still active because of having active media session`);
  await checkControllerIsActive(tab);

  info(`remove tab`);
  await tab.close();
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
      // We activate controller when it becomes playing, which doesn't guarantee
      // it's already audible, so we won't check audible state here.
      ok(controller.isActive, "controller should be active");
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

function createMediaSession(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [], _ => {
    // simply create a media session, which would become the active media session later.
    content.navigator.mediaSession;
  });
}

function checkControllerIsActive(tab) {
  const controller = tab.linkedBrowser.browsingContext.mediaController;
  ok(controller.isActive, `controller is active`);
}
