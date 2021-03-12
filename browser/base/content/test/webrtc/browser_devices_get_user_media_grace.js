/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const permissionError =
  "error: NotAllowedError: The request is not allowed " +
  "by the user agent or the platform in the current context.";

const SAME_ORIGIN = "https://example.com";
const CROSS_ORIGIN = "https://example.org";

const PATH = "/browser/browser/base/content/test/webrtc/get_user_media.html";
const PATH2 = "/browser/browser/base/content/test/webrtc/get_user_media2.html";

const GRACE_PERIOD_MS = 1000;
const WAIT_PERIOD_MS = GRACE_PERIOD_MS + 500;

// We're inherently testing timeouts (grace periods)
/* eslint-disable mozilla/no-arbitrary-setTimeout */
const wait = ms => new Promise(resolve => setTimeout(resolve, ms));
const perms = SitePermissions;

// These tests focus on camera and microphone, so we define some helpers.

async function prompt(audio, video) {
  let observerPromise = expectObserverCalled("getUserMedia:request");
  let promise = promisePopupNotificationShown("webRTC-shareDevices");
  await promiseRequestDevice(audio, video);
  await promise;
  await observerPromise;
  checkDeviceSelectors(audio, video);
}

async function allow(audio, video) {
  let indicator = promiseIndicatorWindow();
  let observerPromise1 = expectObserverCalled("getUserMedia:response:allow");
  let observerPromise2 = expectObserverCalled("recording-device-events");
  await promiseMessage("ok", () => {
    PopupNotifications.panel.firstElementChild.button.click();
  });
  await observerPromise1;
  await observerPromise2;
  Assert.deepEqual(
    Object.assign({ audio: false, video: false }, await getMediaCaptureState()),
    { audio, video },
    `expected ${video ? "camera " : ""} ${audio ? "microphone " : ""}shared`
  );
  await indicator;
  await checkSharingUI({ audio, video });
}

async function deny() {
  let observerPromise1 = expectObserverCalled("getUserMedia:response:deny");
  let observerPromise2 = expectObserverCalled("recording-window-ended");
  await promiseMessage(permissionError, () => {
    activateSecondaryAction(kActionDeny);
  });
  await observerPromise1;
  await observerPromise2;
  await checkNotSharing();
}

async function noPrompt(audio, video) {
  let observerPromises = [
    expectObserverCalled("getUserMedia:request"),
    expectObserverCalled("getUserMedia:response:allow"),
    expectObserverCalled("recording-device-events"),
  ];
  let promise = promiseMessage("ok");
  await promiseRequestDevice(audio, video);
  await promise;
  await Promise.all(observerPromises);
  await promiseNoPopupNotification("webRTC-shareDevices");
  Assert.deepEqual(
    Object.assign({ audio: false, video: false }, await getMediaCaptureState()),
    { audio, video },
    `expected ${video ? "camera " : ""} ${audio ? "microphone " : ""}shared`
  );
  await checkSharingUI({ audio, video });
}

async function navigate(browser, url) {
  let loaded = BrowserTestUtils.browserLoaded(browser, false, url);
  await SpecialPowers.spawn(
    browser,
    [url],
    u => (content.document.location = u)
  );
  await loaded;
}

var gTests = [
  {
    desc: "getUserMedia camera+mic survives track.stop but not past grace",
    run: async function checkAudioVideoGracePastStop() {
      await prompt(true, true);
      await allow(true, true);

      info(
        "After closing all streams, gUM(camera+mic) returns a stream " +
          "without prompting within grace period."
      );
      await closeStream();
      await noPrompt(true, true);

      info(
        "After closing all streams, gUM(mic) returns a stream " +
          "without prompting within grace period."
      );
      await closeStream();
      await noPrompt(true, false);

      info(
        "After closing all streams, gUM(camera) returns a stream " +
          "without prompting within grace period."
      );
      await closeStream();
      await noPrompt(false, true);

      info("gUM(screen) still causes a prompt.");
      let observerPromise = expectObserverCalled("getUserMedia:request");
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true, null, "screen");
      await promise;
      await observerPromise;

      is(
        PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
        "webRTC-shareScreen-notification-icon",
        "anchored to device icon"
      );
      checkDeviceSelectors(false, false, true);

      observerPromise = expectObserverCalled("getUserMedia:response:deny");
      await promiseMessage(permissionError, () => {
        activateSecondaryAction(kActionDeny);
      });
      await observerPromise;
      perms.removeFromPrincipal(null, "screen", gBrowser.selectedBrowser);

      await closeStream();
      info("Closed stream. Waiting past grace period.");
      await wait(WAIT_PERIOD_MS);

      info("After grace period expires, gUM(camera) causes a prompt.");
      await prompt(false, true);
      await deny();
      perms.removeFromPrincipal(null, "camera", gBrowser.selectedBrowser);

      info("After grace period expires, gUM(mic) causes a prompt.");
      await prompt(true, false);
      await deny();
      perms.removeFromPrincipal(null, "microphone", gBrowser.selectedBrowser);
    },
  },

  {
    desc: "getUserMedia camera+mic survives page reload but not past grace",
    run: async function checkAudioVideoGracePastReload(browser) {
      await prompt(true, true);
      await allow(true, true);
      await closeStream();

      info("Reload through the page");
      let reloaded = BrowserTestUtils.browserLoaded(browser);
      await SpecialPowers.spawn(browser, [], () =>
        content.document.location.reload()
      );
      await reloaded;

      info(
        "After page reload, gUM(camera+mic) returns a stream " +
          "without prompting within grace period."
      );
      await noPrompt(true, true);
      await closeStream();

      info("Reload as a user");
      let reloadButton = document.getElementById("reload-button");
      await TestUtils.waitForCondition(() => {
        return !reloadButton.disabled;
      });
      reloaded = BrowserTestUtils.browserLoaded(browser);
      EventUtils.synthesizeMouseAtCenter(reloadButton, {});
      await reloaded;

      info(
        "After user page reload, gUM(camera+mic) returns a stream " +
          "without prompting within grace period."
      );
      await noPrompt(true, true);

      info("gUM(screen) still causes a prompt.");
      let observerPromise = expectObserverCalled("getUserMedia:request");
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true, null, "screen");
      await promise;
      await observerPromise;

      is(
        PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
        "webRTC-shareScreen-notification-icon",
        "anchored to device icon"
      );
      checkDeviceSelectors(false, false, true);

      observerPromise = expectObserverCalled("getUserMedia:response:deny");
      await promiseMessage(permissionError, () => {
        activateSecondaryAction(kActionDeny);
      });
      await observerPromise;
      perms.removeFromPrincipal(null, "screen", gBrowser.selectedBrowser);

      await closeStream();
      info("Closed stream. Waiting past grace period.");
      await wait(WAIT_PERIOD_MS);

      info("After grace period expires, gUM(camera) causes a prompt.");
      await prompt(false, true);
      await deny();
      perms.removeFromPrincipal(null, "camera", gBrowser.selectedBrowser);

      info("After grace period expires, gUM(mic) causes a prompt.");
      await prompt(true, false);
      await deny();
      perms.removeFromPrincipal(null, "microphone", gBrowser.selectedBrowser);
    },
  },

  {
    desc: "getUserMedia camera+mic survives navigation but not past grace",
    run: async function checkAudioVideoGracePastNavigation(browser) {
      await prompt(true, true);
      await allow(true, true);
      await closeStream();

      info("Navigate to a second same-origin page");
      await navigate(browser, SAME_ORIGIN + PATH2);
      info(
        "After navigating to second same-origin page, gUM(camera+mic) " +
          "returns a stream without prompting within grace period."
      );
      await noPrompt(true, true);
      await closeStream();

      info("Closed stream. Waiting past grace period.");
      await wait(WAIT_PERIOD_MS);

      info("After grace period expires, gUM(camera+mic) causes a prompt.");
      await prompt(true, true);
      await allow(true, true);

      info("Navigate to a different-origin page");
      await navigate(browser, CROSS_ORIGIN + PATH2);
      info(
        "After navigating to a different-origin page, gUM(camera+mic) " +
          "causes a prompt."
      );
      await prompt(true, true);
      await deny();
      perms.removeFromPrincipal(null, "camera", gBrowser.selectedBrowser);
      perms.removeFromPrincipal(null, "microphone", gBrowser.selectedBrowser);

      info("Navigate back to the first page");
      await navigate(browser, SAME_ORIGIN + PATH);
      info(
        "After navigating back to the first page, gUM(camera+mic) " +
          "returns a stream without prompting within grace period."
      );
      await noPrompt(true, true);
      await closeStream();
      info("Closed stream. Waiting past grace period.");
      await wait(WAIT_PERIOD_MS);

      info("After grace period expires, gUM(camera+mic) causes a prompt.");
      await prompt(true, true);
      await deny();
      perms.removeFromPrincipal(null, "camera", gBrowser.selectedBrowser);
      perms.removeFromPrincipal(null, "microphone", gBrowser.selectedBrowser);
    },
  },

  {
    desc: "getUserMedia camera+mic grace period does not carry over to new tab",
    run: async function checkAudioVideoGraceEndsNewTab() {
      await prompt(true, true);
      await allow(true, true);

      info("Open same page in a new tab");
      await BrowserTestUtils.withNewTab(SAME_ORIGIN + PATH, async browser => {
        info("In new tab, gUM(camera+mic) causes a prompt.");
        await prompt(true, true);
      });
      info("Closed tab");
      await closeStream();
      info("Closed stream. Waiting past grace period.");
      await wait(WAIT_PERIOD_MS);

      info("After grace period expires, gUM(camera+mic) causes a prompt.");
      await prompt(true, true);
      await deny();
      perms.removeFromPrincipal(null, "camera", gBrowser.selectedBrowser);
      perms.removeFromPrincipal(null, "microphone", gBrowser.selectedBrowser);
    },
  },
];

add_task(async function test() {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.webrtc.deviceGracePeriodTimeoutMs", GRACE_PERIOD_MS]],
  });
  await runTests(gTests);
});
