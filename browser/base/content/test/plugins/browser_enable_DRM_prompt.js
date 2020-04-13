/*
 * Bug 1366167 - Tests that the "Enable DRM" prompt shows if EME is requested while EME is disabled.
 */

const TEST_URL =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "empty_file.html";

add_task(async function() {
  await BrowserTestUtils.withNewTab(TEST_URL, async function(browser) {
    // Note: SpecialPowers.pushPrefEnv has problems with the "Enable DRM"
    // button on the notification box toggling the prefs. So manually
    // set/unset the prefs the UI we're testing toggles.
    let emeWasEnabled = Services.prefs.getBoolPref("media.eme.enabled", false);
    let cdmWasEnabled = Services.prefs.getBoolPref(
      "media.gmp-widevinecdm.enabled",
      false
    );

    // Restore the preferences to their pre-test state on test finish.
    registerCleanupFunction(function() {
      Services.prefs.setBoolPref("media.eme.enabled", emeWasEnabled);
      Services.prefs.setBoolPref(
        "media.gmp-widevinecdm.enabled",
        cdmWasEnabled
      );
    });

    // Turn off EME and Widevine CDM.
    Services.prefs.setBoolPref("media.eme.enabled", false);
    Services.prefs.setBoolPref("media.gmp-widevinecdm.enabled", false);

    // Have content request access to Widevine, UI should drop down to
    // prompt user to enable DRM.
    let result = await SpecialPowers.spawn(browser, [], async function() {
      try {
        let config = [
          {
            initDataTypes: ["webm"],
            videoCapabilities: [{ contentType: 'video/webm; codecs="vp9"' }],
          },
        ];
        await content.navigator.requestMediaKeySystemAccess(
          "com.widevine.alpha",
          config
        );
      } catch (ex) {
        return { rejected: true };
      }
      return { rejected: false };
    });
    is(
      result.rejected,
      true,
      "EME request should be denied because EME disabled."
    );

    // Verify the UI prompt showed.
    let box = gBrowser.getNotificationBox(browser);
    let notification = box.currentNotification;

    ok(notification, "Notification should be visible");
    is(
      notification.getAttribute("value"),
      "drmContentDisabled",
      "Should be showing the right notification"
    );

    // Verify the "Enable DRM" button is there.
    let buttons = notification.querySelectorAll(".notification-button");
    is(buttons.length, 1, "Should have one button.");

    // Prepare a Promise that should resolve when the "Enable DRM" button's
    // page reload completes.
    let refreshPromise = BrowserTestUtils.browserLoaded(browser);
    buttons[0].click();

    // Wait for the reload to complete.
    await refreshPromise;

    // Verify clicking the "Enable DRM" button enabled DRM.
    let enabled = Services.prefs.getBoolPref("media.eme.enabled", true);
    is(
      enabled,
      true,
      "EME should be enabled after click on 'Enable DRM' button"
    );
  });
});

add_task(async function test_eme_locked() {
  await BrowserTestUtils.withNewTab(TEST_URL, async function(browser) {
    // Reset prefs manually since we need to unlock them.
    let emeWasEnabled = Services.prefs.getBoolPref("media.eme.enabled", false);

    // Restore the preferences to their pre-test state on test finish.
    registerCleanupFunction(function() {
      Services.prefs.setBoolPref("media.eme.enabled", emeWasEnabled);
      Services.prefs.unlockPref("media.eme.enabled");
    });

    // Turn off EME and Widevine CDM.
    Services.prefs.setBoolPref("media.eme.enabled", false);
    Services.prefs.lockPref("media.eme.enabled");

    // Have content request access to Widevine, UI should drop down to
    // prompt user to enable DRM.
    let result = await SpecialPowers.spawn(browser, [], async function() {
      try {
        let config = [
          {
            initDataTypes: ["webm"],
            videoCapabilities: [{ contentType: 'video/webm; codecs="vp9"' }],
          },
        ];
        await content.navigator.requestMediaKeySystemAccess(
          "com.widevine.alpha",
          config
        );
      } catch (ex) {
        return { rejected: true };
      }
      return { rejected: false };
    });
    is(
      result.rejected,
      true,
      "EME request should be denied because EME disabled."
    );

    // Verify the UI prompt did not show.
    let box = gBrowser.getNotificationBox(browser);
    let notification = box.currentNotification;

    is(
      notification,
      null,
      "Notification should not be displayed since pref is locked"
    );
  });
});
