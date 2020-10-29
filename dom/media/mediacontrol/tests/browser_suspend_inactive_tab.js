/* eslint-disable no-undef */
const PAGE_NON_AUTOPLAY =
  "https://example.com/browser/dom/media/mediacontrol/tests/file_non_autoplay.html";
const VIDEO_ID = "video";

add_task(async function setupTestingPref() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.mediacontrol.testingevents.enabled", true],
      ["dom.suspend_inactive.enabled", true],
      ["dom.audiocontext.testing", true],
    ],
  });
});

/**
 * This test to used to test the feature that would suspend the inactive tab,
 * which currently is only used on Android.
 *
 * Normally when tab becomes inactive, we would suspend it and stop its script
 * from running. However, if a tab has a main controller, which indicates it
 * might have playng media, or waiting media keys to control media, then it
 * would not be suspended event if it's inactive.
 *
 * In addition, Note that, on Android, audio focus management is enabled by
 * default, so there is only one tab being able to play at a time, which means
 * the tab playing media always has main controller.
 */
add_task(async function testInactiveTabWouldBeSuspended() {
  info(`open a tab`);
  const tab = await createTab(PAGE_NON_AUTOPLAY);
  await shouldTabStateEqualTo(tab, "running");

  info(`tab should be suspended when it becomes inactive`);
  setTabActive(tab, false);
  await shouldTabStateEqualTo(tab, "suspended");

  info(`remove tab`);
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function testInactiveTabEverStartPlayingWontBeSuspended() {
  info(`open tab1 and play media`);
  const tab1 = await createTab(PAGE_NON_AUTOPLAY);
  await shouldTabStateEqualTo(tab1, "running");
  await playMedia(tab1, VIDEO_ID);

  info(`tab with playing media won't be suspended when it becomes inactive`);
  setTabActive(tab1, false);
  await shouldTabStateEqualTo(tab1, "running");

  info(
    `even if media is paused, keep tab running so that it could listen to media keys to control media in the future`
  );
  await pauseMedia(tab1, VIDEO_ID);
  await shouldTabStateEqualTo(tab1, "running");

  info(`open tab2 and play media`);
  const tab2 = await createTab(PAGE_NON_AUTOPLAY);
  await shouldTabStateEqualTo(tab2, "running");
  await playMedia(tab2, VIDEO_ID);

  info(
    `as inactive tab1 doesn't own main controller, it should be suspended again`
  );
  await shouldTabStateEqualTo(tab1, "suspended");

  info(`remove tabs`);
  await BrowserTestUtils.removeTab(tab1);
  await BrowserTestUtils.removeTab(tab2);
});

/**
 * The following are helper functions.
 */
async function createTab(url) {
  const tab = await createTabAndLoad(url);
  await createStateObserver(tab);
  return tab;
}

function createStateObserver(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [], _ => {
    /**
     * Currently there is no API allowing us to observe tab's suspend state
     * directly, so we use a tricky way to observe that by checking AudioContext
     * state. Because AudioContext would change its state when tab is being
     * suspended or resumed.
     */
    class StateObserver {
      constructor() {
        this.ac = new content.AudioContext();
      }
      getState() {
        return this.ac.state;
      }
      async waitUntilStateEqualTo(expectedState) {
        while (this.ac.state != expectedState) {
          info(`wait until tab state changes to '${expectedState}'`);
          await new Promise(r => (this.ac.onstatechange = r));
        }
      }
    }
    content.obs = new StateObserver();
  });
}

function setTabActive(tab, isActive) {
  tab.linkedBrowser.docShellIsActive = isActive;
}

function shouldTabStateEqualTo(tab, state) {
  return SpecialPowers.spawn(tab.linkedBrowser, [state], async state => {
    await content.obs.waitUntilStateEqualTo(state);
    ok(content.obs.getState() == state, `correct tab state '${state}'`);
  });
}
