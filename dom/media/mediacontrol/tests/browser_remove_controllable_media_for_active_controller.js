/* eslint-disable no-undef */
const PAGE_URL =
  "https://example.com/browser/dom/media/mediacontrol/tests/file_non_autoplay.html";

const testVideoId = "video";

add_task(async function setupTestingPref() {
  await SpecialPowers.pushPrefEnv({
    set: [["media.mediacontrol.testingevents.enabled", true]],
  });
});

/**
 * If an active controller has an active media session, then it can still be
 * controlled via media key even if there is no controllable media presents.
 * As active media session could still create other controllable media in its
 * action handler, it should still receive media key. However, if a controller
 * doesn't have an active media session, then it won't be controlled via media
 * key when no controllable media presents.
 */
add_task(
  async function testControllerWithActiveMediaSessionShouldStillBeActiveWhenNoControllableMediaPresents() {
    info(`open media page`);
    const tab = await createTabAndLoad(PAGE_URL);

    info(`play media would activate controller and media session`);
    await setupMediaSession(tab);
    await playMedia(tab, testVideoId);
    await checkOrWaitControllerBecomesActive(tab);

    info(`remove playing media so we don't have any controllable media now`);
    await removePlayingMedia(tab);

    info(`despite that, controller should still be active`);
    await checkOrWaitControllerBecomesActive(tab);

    info(`active media session can still receive media key`);
    await ensureActiveMediaSessionReceivedMediaKey(tab);

    info(`remove tab`);
    await BrowserTestUtils.removeTab(tab);
  }
);

add_task(
  async function testControllerWithoutActiveMediaSessionShouldBecomeInactiveWhenNoControllableMediaPresents() {
    info(`open media page`);
    const tab = await createTabAndLoad(PAGE_URL);

    info(`play media would activate controller`);
    await playMedia(tab, testVideoId);
    await checkOrWaitControllerBecomesActive(tab);

    info(`remove playing media so we don't have any controllable media now`);
    await removePlayingMedia(tab);

    info(`without having media session, controller should be deactivated`);
    await checkOrWaitControllerBecomesInactive(tab);

    info(`remove tab`);
    await BrowserTestUtils.removeTab(tab);
  }
);

/**
 * The following are helper functions.
 */
function setupMediaSession(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [], _ => {
    // except `play/pause/stop`, set an action handler for arbitrary key in
    // order to later verify if the session receives that media key by listening
    // to session's `onpositionstatechange`.
    content.navigator.mediaSession.setActionHandler("seekforward", _ => {
      content.navigator.mediaSession.setPositionState({
        duration: 60,
      });
    });
  });
}

async function ensureActiveMediaSessionReceivedMediaKey(tab) {
  const controller = tab.linkedBrowser.browsingContext.mediaController;
  const positionChangePromise = new Promise(
    r => (controller.onpositionstatechange = r)
  );
  MediaControlService.generateMediaControlKey("seekforward");
  await positionChangePromise;
  ok(true, "active media session received media key");
}

function removePlayingMedia(tab) {
  const controller = tab.linkedBrowser.browsingContext.mediaController;
  return Promise.all([
    new Promise(r => (controller.onplaybackstatechange = r)),
    SpecialPowers.spawn(tab.linkedBrowser, [testVideoId], Id => {
      content.document.getElementById(Id).remove();
    }),
  ]);
}

async function checkOrWaitControllerBecomesActive(tab) {
  const controller = tab.linkedBrowser.browsingContext.mediaController;
  if (!controller.isActive) {
    await new Promise(r => (controller.onactivated = r));
  }
  ok(controller.isActive, `controller is active`);
}

async function checkOrWaitControllerBecomesInactive(tab) {
  const controller = tab.linkedBrowser.browsingContext.mediaController;
  if (controller.isActive) {
    await new Promise(r => (controller.ondeactivated = r));
  }
  ok(!controller.isActive, `controller is inacitve`);
}
