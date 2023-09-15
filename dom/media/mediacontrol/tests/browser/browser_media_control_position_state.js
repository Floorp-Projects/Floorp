const PAGE_URL =
  "https://example.com/browser/dom/media/mediacontrol/tests/browser/file_non_autoplay.html";
const IFRAME_URL =
  "https://example.com/browser/dom/media/mediacontrol/tests/browser/file_iframe_media.html";

const testVideoId = "video";

add_task(async function setupTestingPref() {
  await SpecialPowers.pushPrefEnv({
    set: [["media.mediacontrol.testingevents.enabled", true]],
  });
});

/**
 * This test is used to check if we can receive correct position state change,
 * when we set the position state to the media session.
 */
add_task(async function testSetPositionState() {
  info(`open media page`);
  const tab = await createLoadedTabWrapper(PAGE_URL);

  info(`start media`);
  await playMedia(tab, testVideoId);

  info(`set duration only`);
  await setPositionState(tab, {
    duration: 60,
  });

  info(`set duration and playback rate`);
  await setPositionState(tab, {
    duration: 50,
    playbackRate: 2.0,
  });

  info(`set duration, playback rate and position`);
  await setPositionState(tab, {
    duration: 40,
    playbackRate: 3.0,
    position: 10,
  });

  info(`remove tab`);
  await tab.close();
});

add_task(async function testSetPositionStateFromInactiveMediaSession() {
  info(`open media page`);
  const tab = await createLoadedTabWrapper(PAGE_URL);

  info(`start media`);
  await playMedia(tab, testVideoId);

  info(
    `add an event listener to measure how many times the position state changes`
  );
  let positionChangedNum = 0;
  const controller = tab.linkedBrowser.browsingContext.mediaController;
  controller.onpositionstatechange = () => positionChangedNum++;

  info(`set position state on the main page which has an active media session`);
  await setPositionState(tab, {
    duration: 60,
  });

  info(`set position state on the iframe which has an inactive media session`);
  await setPositionStateOnInactiveMediaSession(tab);

  info(`set position state on the main page again`);
  await setPositionState(tab, {
    duration: 60,
  });
  is(
    positionChangedNum,
    2,
    `We should only receive two times of position change, because ` +
      `the second one which performs on inactive media session is effectless`
  );

  info(`remove tab`);
  await tab.close();
});

/**
 * The following are helper functions.
 */
async function setPositionState(tab, positionState) {
  const controller = tab.linkedBrowser.browsingContext.mediaController;
  const positionStateChanged = new Promise(r => {
    controller.addEventListener(
      "positionstatechange",
      event => {
        const { duration, playbackRate, position } = positionState;
        // duration is mandatory.
        is(
          event.duration,
          duration,
          `expected duration ${event.duration} is equal to ${duration}`
        );

        // Playback rate is optional, if it's not present, default should be 1.0
        if (playbackRate) {
          is(
            event.playbackRate,
            playbackRate,
            `expected playbackRate ${event.playbackRate} is equal to ${playbackRate}`
          );
        } else {
          is(event.playbackRate, 1.0, `expected default playbackRate is 1.0`);
        }

        // Position state is optional, if it's not present, default should be 0.0
        if (position) {
          is(
            event.position,
            position,
            `expected position ${event.position} is equal to ${position}`
          );
        } else {
          is(event.position, 0.0, `expected default position is 0.0`);
        }
        r();
      },
      { once: true }
    );
  });
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [positionState],
    positionState => {
      content.navigator.mediaSession.setPositionState(positionState);
    }
  );
  await positionStateChanged;
}

async function setPositionStateOnInactiveMediaSession(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [IFRAME_URL], async url => {
    info(`create iframe and wait until it finishes loading`);
    const iframe = content.document.getElementById("iframe");
    iframe.src = url;
    await new Promise(r => (iframe.onload = r));

    info(`trigger media in iframe entering into fullscreen`);
    iframe.contentWindow.postMessage("setPositionState", "*");
  });
}
