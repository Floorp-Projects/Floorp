const PAGE_URL =
  "https://example.com/browser/dom/media/mediacontrol/tests/browser/file_empty_title.html";

/**
 * This test is used to ensure that real-time media won't be affected by the
 * media control. Only non-real-time media would.
 */
add_task(async function setupTestingPref() {
  await SpecialPowers.pushPrefEnv({
    set: [["media.mediacontrol.testingevents.enabled", true]],
  });
});

add_task(async function testOnlyControlNonRealTimeMedia() {
  const tab = await createLoadedTabWrapper(PAGE_URL);
  const controller = tab.linkedBrowser.browsingContext.mediaController;
  await StartRealTimeMedia(tab);
  ok(
    !controller.isActive,
    "starting a real-time media won't acivate controller"
  );

  info(`playing a non-real-time media would activate controller`);
  await Promise.all([
    new Promise(r => (controller.onactivated = r)),
    startNonRealTimeMedia(tab),
  ]);

  info(`'pause' action should only pause non-real-time media`);
  MediaControlService.generateMediaControlKey("pause");
  await new Promise(r => (controller.onplaybackstatechange = r));
  await checkIfMediaAreAffectedByMediaControl(tab);

  info(`remove tab`);
  await tab.close();
});

async function startNonRealTimeMedia(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [], async _ => {
    let video = content.document.getElementById("video");
    if (!video) {
      ok(false, `can not get the video element!`);
      return;
    }
    await video.play();
  });
}

async function StartRealTimeMedia(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [], async _ => {
    let videoRealTime = content.document.createElement("video");
    content.document.body.appendChild(videoRealTime);
    videoRealTime.srcObject = await content.navigator.mediaDevices.getUserMedia(
      { audio: true, fake: true }
    );
    // We want to ensure that the checking of should the media be controlled by
    // media control would be performed after the element finishes loading the
    // media stream. Using `autoplay` would trigger the play invocation only
    // after the element get enough data.
    videoRealTime.autoplay = true;
    await new Promise(r => (videoRealTime.onplaying = r));
  });
}

async function checkIfMediaAreAffectedByMediaControl(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [], async _ => {
    const vids = content.document.getElementsByTagName("video");
    for (let vid of vids) {
      if (!vid.srcObject) {
        ok(vid.paused, "non-real-time media should be paused");
      } else {
        ok(!vid.paused, "real-time media should not be affected");
      }
    }
  });
}
