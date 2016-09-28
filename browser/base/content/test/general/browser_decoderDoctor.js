"use strict";

function* test_decoder_doctor_notification(type, notificationMessage, options) {
  yield BrowserTestUtils.withNewTab({ gBrowser }, function*(browser) {
    let awaitNotificationBar =
      BrowserTestUtils.waitForNotificationBar(gBrowser, browser, "decoder-doctor-notification");

    yield ContentTask.spawn(browser, type, function*(type) {
      Services.obs.notifyObservers(content.window,
                                   "decoder-doctor-notification",
                                   JSON.stringify({type: type,
                                                   isSolved: false,
                                                   decoderDoctorReportId: "test",
                                                   formats: "test"}));
    });

    let notification;
    try {
      notification = yield awaitNotificationBar;
    } catch (ex) {
      ok(false, ex);
      return;
    }
    ok(notification, "Got decoder-doctor-notification notification");

    is(notification.getAttribute("label"), notificationMessage,
      "notification message should match expectation");
    let button = notification.childNodes[0];
    if (options && options.noLearnMoreButton) {
      ok(!button, "There should not be a Learn More button");
      return;
    }

    is(button.getAttribute("label"), gNavigatorBundle.getString("decoder.noCodecs.button"),
      "notification button should be 'Learn more'");
    is(button.getAttribute("accesskey"), gNavigatorBundle.getString("decoder.noCodecs.accesskey"),
      "notification button should have accesskey");

    let baseURL = Services.urlFormatter.formatURLPref("app.support.baseURL");
    let url = baseURL + ((options && options.sumo) ||
                         "fix-video-audio-problems-firefox-windows");
    let awaitNewTab = BrowserTestUtils.waitForNewTab(gBrowser, url);
    button.click();
    let sumoTab = yield awaitNewTab;
    yield BrowserTestUtils.removeTab(sumoTab);
  });
}

add_task(function* test_adobe_cdm_not_found() {
  // This is only sent on Windows.
  if (AppConstants.platform != "win") {
    return;
  }

  let message;
  if (AppConstants.isPlatformAndVersionAtMost("win", "5.9")) {
    message = gNavigatorBundle.getFormattedString("emeNotifications.drmContentDisabled.message", [""]);
  } else {
    message = gNavigatorBundle.getString("decoder.noCodecs.message");
  }

  yield test_decoder_doctor_notification("adobe-cdm-not-found", message);
});

add_task(function* test_adobe_cdm_not_activated() {
  // This is only sent on Windows.
  if (AppConstants.platform != "win") {
    return;
  }

  let message;
  if (AppConstants.isPlatformAndVersionAtMost("win", "5.9")) {
    message = gNavigatorBundle.getString("decoder.noCodecsXP.message");
  } else {
    message = gNavigatorBundle.getString("decoder.noCodecs.message");
  }

  yield test_decoder_doctor_notification("adobe-cdm-not-activated", message);
});

add_task(function* test_platform_decoder_not_found() {
  // Not sent on Windows XP.
  if (AppConstants.isPlatformAndVersionAtMost("win", "5.9")) {
    return;
  }

  let message;
  let isLinux = AppConstants.platform == "linux";
  if (isLinux) {
    message = gNavigatorBundle.getString("decoder.noCodecsLinux.message");
  } else {
    message = gNavigatorBundle.getString("decoder.noHWAcceleration.message");
  }

  yield test_decoder_doctor_notification("platform-decoder-not-found",
                                         message,
                                         {noLearnMoreButton: isLinux});
});

add_task(function* test_cannot_initialize_pulseaudio() {
  // This is only sent on Linux.
  if (AppConstants.platform != "linux") {
    return;
  }

  let message = gNavigatorBundle.getString("decoder.noPulseAudio.message");
  yield test_decoder_doctor_notification("cannot-initialize-pulseaudio",
                                         message,
                                         {sumo: "fix-common-audio-and-video-issues"});
});
