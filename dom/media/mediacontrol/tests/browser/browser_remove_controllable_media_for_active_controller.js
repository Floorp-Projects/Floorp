const PAGE_URL =
  "https://example.com/browser/dom/media/mediacontrol/tests/browser/file_non_autoplay.html";

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
    const tab = await createLoadedTabWrapper(PAGE_URL);

    info(`play media would activate controller and media session`);
    await setupMediaSession(tab);
    await playMedia(tab, testVideoId);
    await checkOrWaitControllerBecomesActive(tab);

    info(`remove playing media so we don't have any controllable media now`);
    await Promise.all([
      new Promise(r => (tab.controller.onplaybackstatechange = r)),
      removePlayingMedia(tab),
    ]);

    info(`despite that, controller should still be active`);
    await checkOrWaitControllerBecomesActive(tab);

    info(`active media session can still receive media key`);
    await ensureActiveMediaSessionReceivedMediaKey(tab);

    info(`remove tab`);
    await tab.close();
  }
);

add_task(
  async function testControllerWithoutActiveMediaSessionShouldBecomeInactiveWhenNoControllableMediaPresents() {
    info(`open media page`);
    const tab = await createLoadedTabWrapper(PAGE_URL);

    info(`play media would activate controller`);
    await playMedia(tab, testVideoId);
    await checkOrWaitControllerBecomesActive(tab);

    info(
      `remove playing media so we don't have any controllable media, which would deactivate controller`
    );
    await Promise.all([
      new Promise(r => (tab.controller.ondeactivated = r)),
      removePlayingMedia(tab),
    ]);

    info(`remove tab`);
    await tab.close();
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
  return SpecialPowers.spawn(tab.linkedBrowser, [testVideoId], Id => {
    content.document.getElementById(Id).remove();
  });
}

async function checkOrWaitControllerBecomesActive(tab) {
  const controller = tab.linkedBrowser.browsingContext.mediaController;
  if (!controller.isActive) {
    info(`wait until controller gets activated`);
    await new Promise(r => (controller.onactivated = r));
  }
  ok(controller.isActive, `controller is active`);
}
