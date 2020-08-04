/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "https://example.com/"
);
const TEST_PAGE = TEST_ROOT + "get_user_media.html";

/**
 * Given a browser from a tab in this window, chooses to share
 * some combination of camera, mic or screen.
 *
 * @param {<xul:browser} browser - The browser to share devices with.
 * @param {boolean} camera - True to share a camera device.
 * @param {boolean} mic - True to share a microphone device.
 * @param {boolean} screen - True to share a display device.
 * @return {Promise}
 * @resolves {undefined} - Once the sharing is complete.
 */
async function shareDevices(browser, camera, mic, screen) {
  if (camera || mic) {
    let promise = promisePopupNotificationShown(
      "webRTC-shareDevices",
      null,
      window
    );

    await promiseRequestDevice(mic, camera, null, null, browser);
    await promise;

    checkDeviceSelectors(mic, camera);
    let observerPromise1 = expectObserverCalled("getUserMedia:response:allow");
    let observerPromise2 = expectObserverCalled("recording-device-events");
    promise = promiseMessage("ok", () => {
      PopupNotifications.panel.firstElementChild.button.click();
    });

    await observerPromise1;
    await observerPromise2;
    await promise;
  }

  if (screen) {
    let promise = promisePopupNotificationShown(
      "webRTC-shareDevices",
      null,
      window
    );

    await promiseRequestDevice(false, true, null, "screen", browser);
    await promise;

    checkDeviceSelectors(false, false, true, window);

    let document = window.document;

    // Select one of the windows / screens. It doesn't really matter which.
    let menulist = document.getElementById("webRTC-selectWindow-menulist");
    menulist.getItemAtIndex(menulist.itemCount - 1).doCommand();
    let notification = window.PopupNotifications.panel.firstElementChild;

    let observerPromise1 = expectObserverCalled("getUserMedia:response:allow");
    let observerPromise2 = expectObserverCalled("recording-device-events");
    await promiseMessage(
      "ok",
      () => {
        notification.button.click();
      },
      1,
      browser
    );
    await observerPromise1;
    await observerPromise2;
  }
}

/**
 * Tests that if the indicator is closed somehow by the user when streams
 * still ongoing, that all of those streams are stopped, and the most recent
 * tab that a stream was shared with is selected.
 */
add_task(async function test_close_indicator() {
  let prefs = [
    [PREF_PERMISSION_FAKE, true],
    [PREF_AUDIO_LOOPBACK, ""],
    [PREF_VIDEO_LOOPBACK, ""],
    [PREF_FAKE_STREAMS, true],
    [PREF_FOCUS_SOURCE, false],
  ];
  await SpecialPowers.pushPrefEnv({ set: prefs });

  let indicatorPromise = promiseIndicatorWindow();

  info("Opening first tab");
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE);
  info("Sharing camera, microphone and screen");
  await shareDevices(tab1.linkedBrowser, true, true, true);

  info("Opening second tab");
  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE);
  info("Sharing camera and screen");
  await shareDevices(tab2.linkedBrowser, true, false, true);

  info("Opening third tab");
  let tab3 = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE);
  info("Sharing screen");
  await shareDevices(tab3.linkedBrowser, false, false, true);

  info("Opening fourth tab");
  let tab4 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com"
  );

  Assert.equal(
    gBrowser.selectedTab,
    tab4,
    "Most recently opened tab is selected"
  );

  let indicator = await indicatorPromise;

  indicator.close();

  await checkNotSharing();

  Assert.equal(
    webrtcUI.activePerms.size,
    0,
    "There shouldn't be any active stream permissions."
  );

  Assert.equal(
    gBrowser.selectedTab,
    tab3,
    "Most recently tab that streams were shared with is selected"
  );
  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab3);
  BrowserTestUtils.removeTab(tab4);
});
