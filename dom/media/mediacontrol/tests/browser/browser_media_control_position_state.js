const PAGE_URL =
  "https://example.com/browser/dom/media/mediacontrol/tests/browser/file_non_autoplay.html";
const IFRAME_URL =
  "https://example.com/browser/dom/media/mediacontrol/tests/browser/file_iframe_media.html";

const testVideoId = "video";
const videoDuration = 5.589333;

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
  logPositionStateChangeEvents(tab);

  info(`apply initial position state`);
  await applyPositionState(tab, { duration: 10 });

  info(`start media`);
  const initialPositionState = isNextPositionState(tab, { duration: 10 });
  await playMedia(tab, testVideoId);
  await initialPositionState;

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
  logPositionStateChangeEvents(tab);

  info(`apply initial position state`);
  await applyPositionState(tab, { duration: 10 });

  info(`start media`);
  const initialPositionState = isNextPositionState(tab, { duration: 10 });
  await playMedia(tab, testVideoId);
  await initialPositionState;

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
 *
 * @param {boolean} withMetadata
 *                  Specifies if the tab should set metadata for the playing video
 */
async function testGuessedPositionState(withMetadata) {
  info(`open media page`);
  const tab = await createLoadedTabWrapper(PAGE_URL);
  logPositionStateChangeEvents(tab);

  if (withMetadata) {
    info(`set media metadata`);
    await setMediaMetadata(tab, { title: "A Video" });
  }

  info(`start media`);
  await emitsPositionState(() => playMedia(tab, testVideoId), tab, {
    duration: videoDuration,
    position: 0,
    playbackRate: 1.0,
  });

  info(`set playback rate to 2x`);
  await emitsPositionState(() => setPlaybackRate(tab, testVideoId, 2.0), tab, {
    duration: videoDuration,
    position: null, // ignored,
    playbackRate: 2.0,
  });

  info(`seek to 1s`);
  await emitsPositionState(() => setCurrentTime(tab, testVideoId, 1.0), tab, {
    duration: videoDuration,
    position: 1.0,
    playbackRate: 2.0,
  });

  let positionChangedNum = 0;
  const controller = tab.linkedBrowser.browsingContext.mediaController;
  controller.onpositionstatechange = () => positionChangedNum++;

  info(`pause media`);
  // shouldn't generate an event
  await pauseMedia(tab, testVideoId);

  info(`seek to 2s`);
  await emitsPositionState(() => setCurrentTime(tab, testVideoId, 2.0), tab, {
    duration: videoDuration,
    position: 2.0,
    playbackRate: 2.0,
  });

  info(`start media`);
  await emitsPositionState(() => playMedia(tab, testVideoId), tab, {
    duration: videoDuration,
    position: 2.0,
    playbackRate: 2.0,
  });

  is(
    positionChangedNum,
    2,
    `We should only receive two of position changes, because pausing is effectless`
  );

  info(`remove tab`);
  await tab.close();
}

add_task(async function testGuessedPositionStateWithMetadata() {
  testGuessedPositionState(true);
});

add_task(async function testGuessedPositionStateWithoutMetadata() {
  testGuessedPositionState(false);
});

/**
 * @typedef {{
 *   duration: number,
 *   playbackRate?: number | null,
 *   position?: number | null,
 * }} ExpectedPositionState
 */

/**
 * Checks if the next received position state matches the expected one.
 *
 * @param {tab} tab
 *        The tab that contains the media
 * @param {ExpectedPositionState} positionState
 *         The expected position state. `duration` is mandatory. `playbackRate`
 *         and `position` are optional. If they're `null`, they're ignored,
 *         otherwise if they're not present or undefined, they're expected to
 *         be the default value.
 * @returns {Promise}
 *          Resolves when the event has been received
 */
async function isNextPositionState(tab, positionState) {
  const got = await nextPositionState(tab);
  isPositionState(got, positionState);
}

/**
 * Waits for the next position state and returns it
 *
 * @param {tab} tab The tab to receive position state from
 * @returns {Promise<MediaPositionState>} The emitted position state
 */
function nextPositionState(tab) {
  const controller = tab.linkedBrowser.browsingContext.mediaController;
  return new Promise(r => {
    controller.addEventListener("positionstatechange", r, { once: true });
  });
}

/**
 * @param {MediaPositionState} got
 *        The received position state
 * @param {ExpectedPositionState} expected
 *         The expected position state. `duration` is mandatory. `playbackRate`
 *         and `position` are optional. If they're `null`, they're ignored,
 *         otherwise if they're not present or undefined, they're expected to
 *         be the default value.
 */
function isPositionState(got, expected) {
  const { duration, playbackRate, position } = expected;
  // duration is mandatory.
  isFuzzyEq(got.duration, duration, "duration");

  // Playback rate is optional, if it's not present, default should be 1.0
  if (typeof playbackRate === "number") {
    isFuzzyEq(got.playbackRate, playbackRate, "playbackRate");
  } else if (playbackRate !== null) {
    is(got.playbackRate, 1.0, `expected default playbackRate is 1.0`);
  }

  // Position is optional, if it's not present, default should be 0.0
  if (typeof position === "number") {
    isFuzzyEq(got.position, position, "position");
  } else if (position !== null) {
    is(got.position, 0.0, `expected default position is 0.0`);
  }
}

/**
 * Checks if two numbers are equal within one significant digit
 *
 * @param {number} got
 *        The value received while testing
 * @param {number} expected
 *        The expected value
 * @param {string} role
 *        The role of the check (used for formatting)
 */
function isFuzzyEq(got, expected, role) {
  expected = expected.toFixed(1);
  got = got.toFixed(1);
  is(got, expected, `expected ${role} ${got} to equal ${expected}`);
}

/**
 * Test if `cb` emits a position state event.
 *
 * @param {() => (void | Promise<void>)} cb
 *        A callback that is expected to generate a position state event
 * @param {tab} tab
 *        The tab that contains the media
 * @param {ExpectedPositionState} positionState
 *        The expected position state to be generated.
 */
async function emitsPositionState(cb, tab, positionState) {
  const positionStateChanged = isNextPositionState(tab, positionState);
  await cb();
  await positionStateChanged;
}

/**
 * The following are helper functions.
 */
async function setPositionState(tab, positionState) {
  await emitsPositionState(
    () => applyPositionState(tab, positionState),
    tab,
    positionState
  );
}

async function applyPositionState(tab, positionState) {
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [positionState],
    positionState => {
      content.navigator.mediaSession.setPositionState(positionState);
    }
  );
}

async function setMediaMetadata(tab, metadata) {
  await SpecialPowers.spawn(tab.linkedBrowser, [metadata], data => {
    content.navigator.mediaSession.metadata = new content.MediaMetadata(data);
  });
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
