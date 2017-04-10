"use strict";

// 'data' contains the notification data object:
// - data.type must be provided.
// - data.isSolved and data.decoderDoctorReportId will be added if not provided
//   (false and "testReportId" resp.)
// - Other fields (e.g.: data.formats) may be provided as needed.
// 'notificationMessage': Expected message in the notification bar.
//   Falsy if nothing is expected after the notification is sent, in which case
//   we won't have further checks, so the following parameters are not needed.
// 'label': Expected button label. Falsy if no button is expected, in which case
//   we won't have further checks, so the following parameters are not needed.
// 'accessKey': Expected access key for the button.
// 'tabChecker': function(openedTab) called with the opened tab that resulted
//   from clicking the button.
function* test_decoder_doctor_notification(data, notificationMessage,
                                           label, accessKey, tabChecker) {
  if (typeof data.type === "undefined") {
    ok(false, "Test implementation error: data.type must be provided");
    return;
  }
  data.isSolved = data.isSolved || false;
  if (typeof data.decoderDoctorReportId === "undefined") {
    data.decoderDoctorReportId = "testReportId";
  }
  yield BrowserTestUtils.withNewTab({ gBrowser }, function*(browser) {
    let awaitNotificationBar =
      BrowserTestUtils.waitForNotificationBar(gBrowser, browser, "decoder-doctor-notification");

    yield ContentTask.spawn(browser, data, function*(aData) {
      Services.obs.notifyObservers(content.window,
                                   "decoder-doctor-notification",
                                   JSON.stringify(aData));
    });

    if (!notificationMessage) {
      ok(true, "Tested notifying observers with a nonsensical message, no effects expected");
      return;
    }

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
    if (!label) {
      ok(!button, "There should not be button");
      return;
    }

    is(button.getAttribute("label"),
       label,
       `notification button should be '${label}'`);
    is(button.getAttribute("accesskey"),
       accessKey,
       "notification button should have accesskey");

    if (!tabChecker) {
      ok(false, "Test implementation error: Missing tabChecker");
      return;
    }
    let awaitNewTab = BrowserTestUtils.waitForNewTab(gBrowser);
    button.click();
    let openedTab = yield awaitNewTab;
    tabChecker(openedTab);
    yield BrowserTestUtils.removeTab(openedTab);
  });
}

function tab_checker_for_sumo(expectedPath) {
  return function(openedTab) {
    let baseURL = Services.urlFormatter.formatURLPref("app.support.baseURL");
    let url = baseURL + expectedPath;
    is(openedTab.linkedBrowser.currentURI.spec, url,
       `Expected '${url}' in new tab`);
  };
}

add_task(function* test_platform_decoder_not_found() {
  let message = "";
  let isLinux = AppConstants.platform == "linux";
  if (isLinux) {
    message = gNavigatorBundle.getString("decoder.noCodecsLinux.message");
  } else if (AppConstants.platform == "win") {
    message = gNavigatorBundle.getString("decoder.noHWAcceleration.message");
  }

  yield test_decoder_doctor_notification(
    {type: "platform-decoder-not-found", formats: "testFormat"},
    message,
    isLinux ? "" : gNavigatorBundle.getString("decoder.noCodecs.button"),
    isLinux ? "" : gNavigatorBundle.getString("decoder.noCodecs.accesskey"),
    tab_checker_for_sumo("fix-video-audio-problems-firefox-windows"));
});

add_task(function* test_cannot_initialize_pulseaudio() {
  let message = "";
  // This is only sent on Linux.
  if (AppConstants.platform == "linux") {
    message = gNavigatorBundle.getString("decoder.noPulseAudio.message");
  }

  yield test_decoder_doctor_notification(
    {type: "cannot-initialize-pulseaudio", formats: "testFormat"},
    message,
    gNavigatorBundle.getString("decoder.noCodecs.button"),
    gNavigatorBundle.getString("decoder.noCodecs.accesskey"),
    tab_checker_for_sumo("fix-common-audio-and-video-issues"));
});

add_task(function* test_unsupported_libavcodec() {
  let message = "";
  // This is only sent on Linux.
  if (AppConstants.platform == "linux") {
    message =
      gNavigatorBundle.getString("decoder.unsupportedLibavcodec.message");
  }

  yield test_decoder_doctor_notification(
    {type: "unsupported-libavcodec", formats: "testFormat"}, message);
});
