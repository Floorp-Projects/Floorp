/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "https://example.com/"
);
const TEST_PAGE = TEST_ROOT + "get_user_media.html";

/**
 * Tests that the screen / window sharing permission popup offers the ability
 * for users to silence DOM notifications while sharing.
 */

/**
 * Helper function that exercises a specific browser to test whether or not the
 * user can silence notifications via the display sharing permission panel.
 *
 * First, we ensure that notification silencing is disabled by default. Then, we
 * request screen sharing from the browser, and check the checkbox that
 * silences notifications. Once screen sharing is established, then we ensure
 * that notification silencing is enabled. Then we stop sharing, and ensure that
 * notification silencing is disabled again.
 *
 * @param {<xul:browser>} aBrowser - The window to run the test on. This browser
 *   should have TEST_PAGE loaded.
 * @return Promise
 * @resolves undefined - When the test on the browser is complete.
 */
async function testNotificationSilencing(aBrowser) {
  let hasIndicator = Services.wm
    .getEnumerator("Browser:WebRTCGlobalIndicator")
    .hasMoreElements();

  let window = aBrowser.ownerGlobal;

  let alertsService = Cc["@mozilla.org/alerts-service;1"]
    .getService(Ci.nsIAlertsService)
    .QueryInterface(Ci.nsIAlertsDoNotDisturb);
  Assert.ok(alertsService, "Alerts Service implements nsIAlertsDoNotDisturb");
  Assert.ok(
    !alertsService.suppressForScreenSharing,
    "Should not be silencing notifications to start."
  );

  let observerPromise = expectObserverCalled(
    "getUserMedia:request",
    1,
    aBrowser
  );
  let promise = promisePopupNotificationShown(
    "webRTC-shareDevices",
    null,
    window
  );
  let indicatorPromise = hasIndicator
    ? Promise.resolve()
    : promiseIndicatorWindow();
  await promiseRequestDevice(false, true, null, "screen", aBrowser);
  await promise;
  await observerPromise;

  checkDeviceSelectors(false, false, true, window);

  let document = window.document;

  // Select one of the windows / screens. It doesn't really matter which.
  let menulist = document.getElementById("webRTC-selectWindow-menulist");
  menulist.getItemAtIndex(menulist.itemCount - 1).doCommand();

  let notification = window.PopupNotifications.panel.firstElementChild;
  Assert.ok(
    notification.hasAttribute("warninghidden"),
    "Notification silencing warning message is hidden by default"
  );

  let checkbox = notification.checkbox;
  Assert.ok(!!checkbox, "Notification silencing checkbox is present");
  Assert.ok(!checkbox.checked, "checkbox is not checked by default");
  checkbox.click();
  Assert.ok(checkbox.checked, "checkbox now checked");
  // The orginal behaviour of the checkbox disabled the Allow button. Let's
  // make sure we're not still doing that.
  Assert.ok(!notification.button.disabled, "Allow button is not disabled");
  Assert.ok(
    notification.hasAttribute("warninghidden"),
    "No warning message is shown"
  );

  let observerPromise1 = expectObserverCalled(
    "getUserMedia:response:allow",
    1,
    aBrowser
  );
  let observerPromise2 = expectObserverCalled(
    "recording-device-events",
    1,
    aBrowser
  );
  await promiseMessage(
    "ok",
    () => {
      notification.button.click();
    },
    1,
    aBrowser
  );
  await observerPromise1;
  await observerPromise2;
  let indicator = await indicatorPromise;

  Assert.ok(
    alertsService.suppressForScreenSharing,
    "Should now be silencing notifications"
  );

  let indicatorClosedPromise = hasIndicator
    ? Promise.resolve()
    : BrowserTestUtils.domWindowClosed(indicator);

  await stopSharing("screen", true, aBrowser, window);
  await indicatorClosedPromise;

  Assert.ok(
    !alertsService.suppressForScreenSharing,
    "Should no longer be silencing notifications"
  );
}

add_task(async function setup() {
  // Set prefs so that permissions prompts are shown and loopback devices
  // are not used. To test the chrome we want prompts to be shown, and
  // these tests are flakey when using loopback devices (though it would
  // be desirable to make them work with loopback in future). See bug 1643711.
  let prefs = [
    [PREF_PERMISSION_FAKE, true],
    [PREF_AUDIO_LOOPBACK, ""],
    [PREF_VIDEO_LOOPBACK, ""],
    [PREF_FAKE_STREAMS, true],
    [PREF_FOCUS_SOURCE, false],
  ];

  await SpecialPowers.pushPrefEnv({ set: prefs });
});

/**
 * Tests notification silencing in a normal browser window.
 */
add_task(async function testNormalWindow() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      await testNotificationSilencing(browser);
    }
  );
});

/**
 * Tests notification silencing in a private browser window.
 */
add_task(async function testPrivateWindow() {
  let privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  await BrowserTestUtils.withNewTab(
    {
      gBrowser: privateWindow.gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      await testNotificationSilencing(browser);
    }
  );

  await BrowserTestUtils.closeWindow(privateWindow);
});

/**
 * Tests notification silencing when sharing a screen while already
 * sharing the microphone. Alone ensures that if we stop sharing the
 * screen, but continue sharing the microphone, that notification
 * silencing ends.
 */
add_task(async function testWhileSharingMic() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      let observerPromise = expectObserverCalled("getUserMedia:request");
      await promiseRequestDevice(true, true);
      await promise;
      await observerPromise;

      let indicatorPromise = promiseIndicatorWindow();
      let observerPromise1 = expectObserverCalled(
        "getUserMedia:response:allow"
      );
      let observerPromise2 = expectObserverCalled("recording-device-events");

      promise = promiseMessage("ok", () => {
        PopupNotifications.panel.firstElementChild.button.click();
      });
      await observerPromise1;
      await observerPromise2;
      await promise;

      Assert.deepEqual(
        await getMediaCaptureState(),
        { audio: true, video: true },
        "expected camera and microphone to be shared"
      );

      let indicator = await indicatorPromise;
      await checkSharingUI({ audio: true, video: true });

      await testNotificationSilencing(browser);

      let indicatorClosedPromise = BrowserTestUtils.domWindowClosed(indicator);
      await closeStream();
      await indicatorClosedPromise;
    }
  );
});
