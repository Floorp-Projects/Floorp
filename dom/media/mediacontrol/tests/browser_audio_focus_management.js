/* eslint-disable no-undef */
"use strict";

const PAGE_AUDIBLE =
  "https://example.com/browser/dom/media/mediacontrol/tests/file_autoplay.html";
const PAGE_INAUDIBLE =
  "https://example.com/browser/dom/media/mediacontrol/tests/file_muted_autoplay.html";

/**
 * These tests are used to ensure that the audio focus management works correctly
 * amongs different tabs no matter the pref is on or off. If the pref is on,
 * there is only one tab which is allowed to play audio at a time, the last tab
 * starting audio will immediately stop other tabs which own audio focus. But
 * notice that playing inaudible media won't gain audio focus. If the pref is
 * off, all audible tabs can own audio focus at the same time without
 * interfering each others.
 */
add_task(async function testDisableAudioFocusManagement() {
  await switchAudioFocusManagerment(false);

  info(`open audible autoplay media in tab1`);
  const tab1 = await createTabAndLoad(PAGE_AUDIBLE);
  await checkOrWaitUntilMediaStartedPlaying(tab1);

  info(`open same page on another tab, which shouldn't cause audio competing`);
  const tab2 = await createTabAndLoad(PAGE_AUDIBLE);
  await checkOrWaitUntilMediaStartedPlaying(tab2);

  info(`media in tab1 should be playing still`);
  await checkOrWaitUntilMediaStartedPlaying(tab1);

  info(`remove tabs`);
  await clearTabsAndResetPref([tab1, tab2]);
});

add_task(async function testEnableAudioFocusManagement() {
  await switchAudioFocusManagerment(true);

  info(`open audible autoplay media in tab1`);
  const tab1 = await createTabAndLoad(PAGE_AUDIBLE);
  await checkOrWaitUntilMediaStartedPlaying(tab1);

  info(`open same page on another tab, which should cause audio competing`);
  const tab2 = await createTabAndLoad(PAGE_AUDIBLE);
  await checkOrWaitUntilMediaStartedPlaying(tab2);

  info(`media in tab1 should be stopped`);
  await checkOrWaitUntilMediaStoppedPlaying(tab1);

  info(`remove tabs`);
  await clearTabsAndResetPref([tab1, tab2]);
});

add_task(async function testMutedMediaWontInvolveAudioCompeting() {
  await switchAudioFocusManagerment(true);

  info(`open audible autoplay media in tab1`);
  const tab1 = await createTabAndLoad(PAGE_AUDIBLE);
  await checkOrWaitUntilMediaStartedPlaying(tab1);

  info(
    `open inaudible media page on another tab, which shouldn't cause audio competing`
  );
  const tab2 = await createTabAndLoad(PAGE_INAUDIBLE);
  await checkOrWaitUntilMediaStartedPlaying(tab2);

  info(`media in tab1 should be playing still`);
  await checkOrWaitUntilMediaStartedPlaying(tab1);

  info(
    `open audible media page on the third tab, which should cause audio competing`
  );
  const tab3 = await createTabAndLoad(PAGE_AUDIBLE);
  await checkOrWaitUntilMediaStartedPlaying(tab3);

  info(`media in tab1 should be stopped`);
  await checkOrWaitUntilMediaStoppedPlaying(tab1);

  info(`media in tab2 should not be affected because it's inaudible.`);
  await checkOrWaitUntilMediaStartedPlaying(tab2);

  info(`remove tabs`);
  await clearTabsAndResetPref([tab1, tab2, tab3]);
});

add_task(async function testStopMultipleTabsWhenSwitchingPrefDynamically() {
  await switchAudioFocusManagerment(false);

  info(`open audible autoplay media in tab1`);
  const tab1 = await createTabAndLoad(PAGE_AUDIBLE);
  await checkOrWaitUntilMediaStartedPlaying(tab1);

  info(`open same page on another tab, which shouldn't cause audio competing`);
  const tab2 = await createTabAndLoad(PAGE_AUDIBLE);
  await checkOrWaitUntilMediaStartedPlaying(tab2);

  await switchAudioFocusManagerment(true);

  info(`open same page on the third tab, which should cause audio competing`);
  const tab3 = await createTabAndLoad(PAGE_AUDIBLE);
  await checkOrWaitUntilMediaStartedPlaying(tab3);

  info(`media in tab1 and tab2 should be stopped`);
  await checkOrWaitUntilMediaStoppedPlaying(tab1);
  await checkOrWaitUntilMediaStoppedPlaying(tab2);

  info(`remove tabs`);
  await clearTabsAndResetPref([tab1, tab2, tab3]);
});

/**
 * The following are helper funcions.
 */
async function switchAudioFocusManagerment(enable) {
  const state = enable ? "Enable" : "Disable";
  info(`${state} audio focus management`);
  await SpecialPowers.pushPrefEnv({
    set: [["media.audioFocus.management", enable]],
  });
}
async function createTabAndLoad(url) {
  let tab = await BrowserTestUtils.openNewForegroundTab(window.gBrowser, url);
  return tab;
}

async function checkOrWaitUntilMediaStartedPlaying(tab) {
  await ContentTask.spawn(tab.linkedBrowser, null, () => {
    return new Promise(resolve => {
      const video = content.document.getElementById("autoplay");
      if (!video) {
        ok(false, `can't get the media element!`);
      }
      if (!video.paused) {
        ok(true, `media started playing`);
        resolve();
      } else {
        info(`wait until media starts playing`);
        video.onplaying = () => {
          video.onplaying = null;
          ok(true, `media started playing`);
          resolve();
        };
      }
    });
  });
}

async function checkOrWaitUntilMediaStoppedPlaying(tab) {
  await ContentTask.spawn(tab.linkedBrowser, null, () => {
    return new Promise(resolve => {
      const video = content.document.getElementById("autoplay");
      if (!video) {
        ok(false, `can't get the media element!`);
      }
      if (video.paused) {
        ok(true, `media stopped playing`);
        resolve();
      } else {
        info(`wait until media stops playing`);
        video.onpause = () => {
          video.onpause = null;
          ok(true, `media stopped playing`);
          resolve();
        };
      }
    });
  });
}

async function clearTabsAndResetPref(tabs) {
  info(`clear tabs and reset pref`);
  for (let tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }
  await switchAudioFocusManagerment(false);
}
