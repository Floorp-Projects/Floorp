/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function sendNotifyRequest(name) {
  let ns = {};
  Components.utils.import("resource://gre/modules/services/datareporting/policy.jsm", ns);
  Components.utils.import("resource://gre/modules/Preferences.jsm", ns);

  let service = Components.classes["@mozilla.org/datareporting/service;1"]
                                  .getService(Components.interfaces.nsISupports)
                                  .wrappedJSObject;
  ok(service.healthReporter, "Health Reporter instance is available.");

  let policyPrefs = new ns.Preferences("testing." + name + ".");
  ok(service._prefs, "Health Reporter prefs are available.");
  let hrPrefs = service._prefs;

  let policy = new ns.DataReportingPolicy(policyPrefs, hrPrefs, service);
  policy.firstRunDate = new Date(Date.now() - 24 * 60 * 60 * 1000);

  is(policy.notifyState, policy.STATE_NOTIFY_UNNOTIFIED, "Policy is in unnotified state.");

  service.healthReporter.onInit().then(function onInit() {
    is(policy.ensureNotifyResponse(new Date()), false, "User has not responded to policy.");
  });

  return policy;
}

/**
 * Wait for a <notification> to be closed then call the specified callback.
 */
function waitForNotificationClose(notification, cb) {
  let parent = notification.parentNode;

  let observer = new MutationObserver(function onMutatations(mutations) {
    for (let mutation of mutations) {
      for (let i = 0; i < mutation.removedNodes.length; i++) {
        let node = mutation.removedNodes.item(i);

        if (node != notification) {
          continue;
        }

        observer.disconnect();
        cb();
      }
    }
  });

  observer.observe(parent, {childList: true});
}

let dumpAppender, rootLogger;

function test() {
  waitForExplicitFinish();

  let ns = {};
  Components.utils.import("resource://gre/modules/Log.jsm", ns);
  rootLogger = ns.Log.repository.rootLogger;
  dumpAppender = new ns.Log.DumpAppender();
  dumpAppender.level = ns.Log.Level.All;
  rootLogger.addAppender(dumpAppender);

  let notification = document.getElementById("global-notificationbox");
  let policy;

  notification.addEventListener("AlertActive", function active() {
    notification.removeEventListener("AlertActive", active, true);

    executeSoon(function afterNotification() {
      is(policy.notifyState, policy.STATE_NOTIFY_WAIT, "Policy is waiting for user response.");
      ok(!policy.dataSubmissionPolicyAccepted, "Data submission policy not yet accepted.");

      waitForNotificationClose(notification.currentNotification, function onClose() {
        is(policy.notifyState, policy.STATE_NOTIFY_COMPLETE, "Closing info bar completes user notification.");
        ok(policy.dataSubmissionPolicyAccepted, "Data submission policy accepted.");
        is(policy.dataSubmissionPolicyResponseType, "accepted-info-bar-dismissed",
           "Reason for acceptance was info bar dismissal.");
        is(notification.allNotifications.length, 0, "No notifications remain.");
        test_multiple_windows();
      });
      notification.currentNotification.close();
    });
  }, true);

  policy = sendNotifyRequest("single_window_notified");
}

function test_multiple_windows() {
  // Ensure we see the notification on all windows and that action on one window
  // results in dismiss on every window.
  let window2 = OpenBrowserWindow();
  whenDelayedStartupFinished(window2, function onWindow() {
    let notification1 = document.getElementById("global-notificationbox");
    let notification2 = window2.document.getElementById("global-notificationbox");
    ok(notification2, "2nd window has a global notification box.");

    let policy;

    let displayCount = 0;
    let prefPaneClosed = false;
    let childWindowClosed = false;

    function onAlertDisplayed() {
      displayCount++;

      if (displayCount != 2) {
        return;
      }

      ok(true, "Data reporting info bar displayed on all open windows.");

      // We register two independent observers and we need both to clean up
      // properly. This handles gating for test completion.
      function maybeFinish() {
        if (!prefPaneClosed) {
          dump("Not finishing test yet because pref pane isn't closed.\n");
          return;
        }

        if (!childWindowClosed) {
          dump("Not finishing test yet because child window isn't closed.\n");
          return;
        }

        dump("Finishing multiple window test.\n");
        rootLogger.removeAppender(dumpAppender);
        delete dumpAppender;
        delete rootLogger;
        finish();
      }

      let closeCount = 0;
      function onAlertClose() {
        closeCount++;

        if (closeCount != 2) {
          return;
        }

        ok(true, "Closing info bar on one window closed them on all.");

        is(policy.notifyState, policy.STATE_NOTIFY_COMPLETE,
           "Closing info bar with multiple windows completes notification.");
        ok(policy.dataSubmissionPolicyAccepted, "Data submission policy accepted.");
        is(policy.dataSubmissionPolicyResponseType, "accepted-info-bar-button-pressed",
           "Policy records reason for acceptance was button press.");
        is(notification1.allNotifications.length, 0, "No notifications remain on main window.");
        is(notification2.allNotifications.length, 0, "No notifications remain on 2nd window.");

        window2.close();
        childWindowClosed = true;
        maybeFinish();
      }

      waitForNotificationClose(notification1.currentNotification, onAlertClose);
      waitForNotificationClose(notification2.currentNotification, onAlertClose);

      // While we're here, we dual purpose this test to check that pressing the
      // button does the right thing.
      let buttons = notification2.currentNotification.getElementsByTagName("button");
      is(buttons.length, 1, "There is 1 button in the data reporting notification.");
      let button = buttons[0];

      // Automatically close preferences window when it is opened as part of
      // button press.
      Services.obs.addObserver(function observer(prefWin, topic, data) {
        Services.obs.removeObserver(observer, "advanced-pane-loaded");

        ok(true, "Pref pane opened on info bar button press.");
        executeSoon(function soon() {
          dump("Closing pref pane.\n");
          prefWin.close();
          prefPaneClosed = true;
          maybeFinish();
        });
      }, "advanced-pane-loaded", false);

      button.click();
    }

    notification1.addEventListener("AlertActive", function active1() {
      notification1.removeEventListener("AlertActive", active1, true);
      executeSoon(onAlertDisplayed);
    }, true);

    notification2.addEventListener("AlertActive", function active2() {
      notification2.removeEventListener("AlertActive", active2, true);
      executeSoon(onAlertDisplayed);
    }, true);

    policy = sendNotifyRequest("multiple_window_behavior");
  });
}

