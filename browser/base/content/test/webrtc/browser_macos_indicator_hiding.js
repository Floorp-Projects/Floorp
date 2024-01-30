/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * This test only runs for MacOS 14.0 and above to test the case for
 * Bug 1857254 - MacOS 14 displays two camera in use icons in menu bar
 */

"use strict";

const TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "https://example.com/"
);
const TEST_PAGE = TEST_ROOT + "get_user_media.html";

const { MockRegistrar } = ChromeUtils.importESModule(
  "resource://testing-common/MockRegistrar.sys.mjs"
);

var systemStatusBarService = {
  counter: 0,
  _reset() {
    this.counter = 0;
  },
  QueryInterface: ChromeUtils.generateQI(["nsISystemStatusBar"]),

  addItem(element) {
    info(
      `Add item call was fired for element ${element}, updating counter from ${
        this.counter
      } to ${this.counter + 1}`
    );
    this.counter += 1;
  },

  removeItem(element) {
    info(`remove item call was fired for element ${element}`);
  },
};

/**
 * Helper to test if the indicators are shown based on the params
 *
 * @param {Object}
 *   expectedCount (number) - expected number of indicators turned on
 *   cameraState (boolean) - if the camera indicator should be shown
 *   microphoneState (boolean) - if the microphone indicator should be shown
 *   screenShareState (string) - if the screen share indicator should be shown
 *                                  (SCREEN_SHARE or "")
 */
async function indicatorHelper({
  expectedCount,
  cameraState,
  microphoneState,
  shareScreenState,
}) {
  await BrowserTestUtils.withNewTab(TEST_PAGE, async browser => {
    let indicatorPromise = promiseIndicatorWindow();

    await shareDevices(browser, cameraState, microphoneState, shareScreenState);
    await indicatorPromise;
  });

  is(
    systemStatusBarService.counter,
    expectedCount,
    `${expectedCount} indicator(s) should be shown`
  );

  systemStatusBarService._reset();
}

add_task(async function testIconChanges() {
  SpecialPowers.pushPrefEnv({
    set: [["privacy.webrtc.showIndicatorsOnMacos14AndAbove", false]],
  });

  let fakeStatusBarService = MockRegistrar.register(
    "@mozilla.org/widget/systemstatusbar;1",
    systemStatusBarService
  );

  systemStatusBarService._reset();

  registerCleanupFunction(function () {
    MockRegistrar.unregister(fakeStatusBarService);
  });

  info("Created mock system status bar service");

  let prefs = [
    [PREF_PERMISSION_FAKE, true],
    [PREF_AUDIO_LOOPBACK, ""],
    [PREF_VIDEO_LOOPBACK, ""],
    [PREF_FAKE_STREAMS, true],
    [PREF_FOCUS_SOURCE, false],
  ];
  await SpecialPowers.pushPrefEnv({ set: prefs });

  await indicatorHelper({
    expectedCount: 0,
    cameraState: true,
    microphoneState: true,
    shareScreenState: SHARE_SCREEN,
  });
  await indicatorHelper({
    expectedCount: 0,
    cameraState: true,
    microphoneState: false,
    shareScreenState: SHARE_SCREEN,
  });

  // In case we want to be able to see the indicator
  SpecialPowers.pushPrefEnv({
    set: [["privacy.webrtc.showIndicatorsOnMacos14AndAbove", true]],
  });

  await indicatorHelper({
    expectedCount: 3,
    cameraState: true,
    microphoneState: true,
    shareScreenState: SHARE_SCREEN,
  });
  await indicatorHelper({
    expectedCount: 1,
    cameraState: false,
    microphoneState: false,
    shareScreenState: SHARE_SCREEN,
  });
  await indicatorHelper({
    expectedCount: 1,
    cameraState: true,
    microphoneState: false,
    shareScreenState: "",
  });
  await indicatorHelper({
    expectedCount: 1,
    cameraState: false,
    microphoneState: true,
    shareScreenState: "",
  });

  await SpecialPowers.popPrefEnv();
});
