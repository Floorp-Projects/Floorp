/* eslint-disable no-undef */
"use strict";

const PAGE =
  "https://example.com/browser/dom/media/mediasession/test/file_media_session.html";

const ACTION = "previoustrack";

add_task(async function setupTestingPref() {
  await SpecialPowers.pushPrefEnv({
    set: [["media.mediacontrol.testingevents.enabled", true]],
  });
});

/**
 * When multiple tabs are all having media session, the latest created one would
 * become an active session. When the active media session is destroyed via
 * closing the tab, the previous active session would become current active
 * session again.
 */
add_task(async function testActiveSessionWhenClosingTab() {
  info(`open tab1 and load media session test page`);
  const tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);
  await startMediaPlaybackAndWaitMedisSessionBecomeActiveSession(tab1);

  info(`pressing '${ACTION}' key`);
  MediaControlService.generateMediaControlKey(ACTION);

  info(`session in tab1 should become active session`);
  await checkIfActionReceived(tab1, ACTION);

  info(`open tab2 and load media session test page`);
  const tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);
  await startMediaPlaybackAndWaitMedisSessionBecomeActiveSession(tab2);

  info(`pressing '${ACTION}' key`);
  MediaControlService.generateMediaControlKey(ACTION);

  info(`session in tab2 should become active session`);
  await checkIfActionReceived(tab2, ACTION);
  await checkIfActionNotReceived(tab1, ACTION);

  info(`remove tab2`);
  const controllerChanged = waitUntilMainMediaControllerChanged();
  BrowserTestUtils.removeTab(tab2);
  await controllerChanged;

  info(`pressing '${ACTION}' key`);
  MediaControlService.generateMediaControlKey(ACTION);

  info(`session in tab1 should become active session again`);
  await checkIfActionReceived(tab1, ACTION);

  info(`remove tab1`);
  BrowserTestUtils.removeTab(tab1);
});

/**
 * This test is similar with `testActiveSessionWhenClosingTab`, the difference
 * is that the way we use to destroy active session is via naviagation, not
 * closing tab.
 */
add_task(async function testActiveSessionWhenNavigatingTab() {
  info(`open tab1 and load media session test page`);
  const tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);
  await startMediaPlaybackAndWaitMedisSessionBecomeActiveSession(tab1);

  info(`pressing '${ACTION}' key`);
  MediaControlService.generateMediaControlKey(ACTION);

  info(`session in tab1 should become active session`);
  await checkIfActionReceived(tab1, ACTION);

  info(`open tab2 and load media session test page`);
  const tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);
  await startMediaPlaybackAndWaitMedisSessionBecomeActiveSession(tab2);

  info(`pressing '${ACTION}' key`);
  MediaControlService.generateMediaControlKey(ACTION);

  info(`session in tab2 should become active session`);
  await checkIfActionReceived(tab2, ACTION);
  await checkIfActionNotReceived(tab1, ACTION);

  info(`navigate tab2 to blank page`);
  const controllerChanged = waitUntilMainMediaControllerChanged();
  BrowserTestUtils.startLoadingURIString(tab2.linkedBrowser, "about:blank");
  await controllerChanged;

  info(`pressing '${ACTION}' key`);
  MediaControlService.generateMediaControlKey(ACTION);

  info(`session in tab1 should become active session`);
  await checkIfActionReceived(tab1, ACTION);

  info(`remove tabs`);
  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});

/**
 * If we create a media session in a tab where no any playing media exists, then
 * that session would not involve in global active media session selection. The
 * current active media session would remain unchanged.
 */
add_task(async function testCreatingSessionWithoutPlayingMedia() {
  info(`open tab1 and load media session test page`);
  const tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);
  await startMediaPlaybackAndWaitMedisSessionBecomeActiveSession(tab1);

  info(`pressing '${ACTION}' key`);
  MediaControlService.generateMediaControlKey(ACTION);

  info(`session in tab1 should become active session`);
  await checkIfActionReceived(tab1, ACTION);

  info(`open tab2 and load media session test page`);
  const tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);

  info(`pressing '${ACTION}' key`);
  MediaControlService.generateMediaControlKey(ACTION);

  info(
    `session in tab1 is still an active session because there is no media playing in tab2`
  );
  await checkIfActionReceived(tab1, ACTION);
  await checkIfActionNotReceived(tab2, ACTION);

  info(`remove tabs`);
  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});

/**
 * The following are helper functions
 */
async function startMediaPlaybackAndWaitMedisSessionBecomeActiveSession(tab) {
  await Promise.all([
    BrowserUtils.promiseObserved("active-media-session-changed"),
    SpecialPowers.spawn(tab.linkedBrowser, [], () => {
      const video = content.document.getElementById("testVideo");
      if (!video) {
        ok(false, `can't get the media element!`);
      }
      video.play();
    }),
  ]);
}

async function checkIfActionReceived(tab, action) {
  await SpecialPowers.spawn(tab.linkedBrowser, [action], expectedAction => {
    return new Promise(resolve => {
      const result = content.document.getElementById("result");
      if (!result) {
        ok(false, `can't get the element for showing result!`);
      }

      function checkAction() {
        is(
          result.innerHTML,
          expectedAction,
          `received '${expectedAction}' correctly`
        );
        // Reset the result after finishing checking result, then we can dispatch
        // same action again without worrying about previous result.
        result.innerHTML = "";
        resolve();
      }

      if (result.innerHTML == "") {
        info(`wait until receiving action`);
        result.addEventListener("actionChanged", () => checkAction(), {
          once: true,
        });
      } else {
        checkAction();
      }
    });
  });
}

async function checkIfActionNotReceived(tab, action) {
  await SpecialPowers.spawn(tab.linkedBrowser, [action], expectedAction => {
    return new Promise(resolve => {
      const result = content.document.getElementById("result");
      if (!result) {
        ok(false, `can't get the element for showing result!`);
      }
      is(result.innerHTML, "", `should not receive any action`);
      ok(result.innerHTML != expectedAction, `not receive '${expectedAction}'`);
      resolve();
    });
  });
}

function waitUntilMainMediaControllerChanged() {
  return BrowserUtils.promiseObserved("main-media-controller-changed");
}
