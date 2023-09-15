const PAGE_NON_AUTOPLAY =
  "https://example.com/browser/dom/media/mediacontrol/tests/browser/file_non_autoplay.html";

const testVideoId = "video";
const sDefaultSupportedKeys = ["focus", "play", "pause", "playpause", "stop"];

add_task(async function setupTestingPref() {
  await SpecialPowers.pushPrefEnv({
    set: [["media.mediacontrol.testingevents.enabled", true]],
  });
});

/**
 * Supported media keys are used for indicating what UI button should be shown
 * on the virtual control interface. All supported media keys are listed in
 * `MediaKey` in `MediaController.webidl`. Some media keys are defined as
 * default media keys which are always supported. Otherwise, other media keys
 * have to have corresponding action handler on the active media session in
 * order to be added to the supported keys.
 */
add_task(async function testDefaultSupportedKeys() {
  info(`open media page`);
  const tab = await createLoadedTabWrapper(PAGE_NON_AUTOPLAY);

  info(`start media`);
  await playMedia(tab, testVideoId);

  info(`should use default supported keys`);
  await supportedKeysShouldEqualTo(tab, sDefaultSupportedKeys);

  info(`remove tab`);
  await tab.close();
});

add_task(async function testNoActionHandlerBeingSet() {
  info(`open media page`);
  const tab = await createLoadedTabWrapper(PAGE_NON_AUTOPLAY);

  info(`start media`);
  await playMedia(tab, testVideoId);

  info(`create media session but not set any action handler`);
  await setMediaSessionSupportedAction(tab, []);

  info(
    `should use default supported keys even if ` +
      `media session doesn't have any action handler`
  );
  await supportedKeysShouldEqualTo(tab, sDefaultSupportedKeys);

  info(`remove tab`);
  await tab.close();
});

add_task(async function testSettingActionsWhichAreAlreadyDefaultKeys() {
  info(`open media page`);
  const tab = await createLoadedTabWrapper(PAGE_NON_AUTOPLAY);

  info(`start media`);
  await playMedia(tab, testVideoId);

  info(`create media session but not set any action handler`);
  await setMediaSessionSupportedAction(tab, ["play", "pause", "stop"]);

  info(
    `those actions has already been included in default supported keys, so ` +
      `the result should still be default supported keys`
  );
  await supportedKeysShouldEqualTo(tab, sDefaultSupportedKeys);

  info(`remove tab`);
  await tab.close();
});

add_task(async function testSettingActionsWhichAreNotDefaultKeys() {
  info(`open media page`);
  const tab = await createLoadedTabWrapper(PAGE_NON_AUTOPLAY);

  info(`start media`);
  await playMedia(tab, testVideoId);

  info(`create media session but not set any action handler`);
  let nonDefaultActions = [
    "seekbackward",
    "seekforward",
    "previoustrack",
    "nexttrack",
  ];
  await setMediaSessionSupportedAction(tab, nonDefaultActions);

  info(
    `supported keys should include those actions which are not default supported keys`
  );
  let expectedKeys = sDefaultSupportedKeys.concat(nonDefaultActions);
  await supportedKeysShouldEqualTo(tab, expectedKeys);

  info(`remove tab`);
  await tab.close();
});

/**
 * The following are helper functions.
 */
async function supportedKeysShouldEqualTo(tab, expectedKeys) {
  const controller = tab.linkedBrowser.browsingContext.mediaController;
  const supportedKeys = controller.supportedKeys;
  while (JSON.stringify(expectedKeys) != JSON.stringify(supportedKeys)) {
    await new Promise(r => (controller.onsupportedkeyschange = r));
  }
  for (let idx = 0; idx < expectedKeys.length; idx++) {
    is(
      supportedKeys[idx],
      expectedKeys[idx],
      `'${supportedKeys[idx]}' should equal to '${expectedKeys[idx]}'`
    );
  }
}

function setMediaSessionSupportedAction(tab, actions) {
  return SpecialPowers.spawn(tab.linkedBrowser, [actions], actionArr => {
    for (let action of actionArr) {
      content.navigator.mediaSession.setActionHandler(action, () => {
        info(`set '${action}' action handler`);
      });
    }
  });
}
