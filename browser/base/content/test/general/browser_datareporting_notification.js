/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let originalPolicy = null;

/**
 * Display a datareporting notification to the user.
 *
 * @param  {String} name
 */
function sendNotifyRequest(name) {
  let ns = {};
  Cu.import("resource://gre/modules/services/datareporting/policy.jsm", ns);
  Cu.import("resource://gre/modules/Preferences.jsm", ns);

  let service = Cc["@mozilla.org/datareporting/service;1"]
                  .getService(Ci.nsISupports)
                  .wrappedJSObject;
  ok(service.healthReporter, "Health Reporter instance is available.");

  Cu.import("resource://gre/modules/Promise.jsm", ns);
  let deferred = ns.Promise.defer();

  if (!originalPolicy) {
    originalPolicy = service.policy;
  }

  let policyPrefs = new ns.Preferences("testing." + name + ".");
  ok(service._prefs, "Health Reporter prefs are available.");
  let hrPrefs = service._prefs;

  let policy = new ns.DataReportingPolicy(policyPrefs, hrPrefs, service);
  policy.dataSubmissionPolicyBypassNotification = false;
  service.policy = policy;
  policy.firstRunDate = new Date(Date.now() - 24 * 60 * 60 * 1000);

  service.healthReporter.onInit().then(function onSuccess () {
    is(policy.ensureUserNotified(), false, "User not notified about data policy on init.");
    ok(policy._userNotifyPromise, "_userNotifyPromise defined.");
    policy._userNotifyPromise.then(
      deferred.resolve.bind(deferred),
      deferred.reject.bind(deferred)
    );
  }.bind(this), deferred.reject.bind(deferred));

  return [policy, deferred.promise];
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
  registerCleanupFunction(cleanup);
  waitForExplicitFinish();

  let ns = {};
  Components.utils.import("resource://gre/modules/Log.jsm", ns);
  rootLogger = ns.Log.repository.rootLogger;
  dumpAppender = new ns.Log.DumpAppender();
  dumpAppender.level = ns.Log.Level.All;
  rootLogger.addAppender(dumpAppender);

  closeAllNotifications().then(function onSuccess () {
    let notification = document.getElementById("global-notificationbox");

    notification.addEventListener("AlertActive", function active() {
      notification.removeEventListener("AlertActive", active, true);
      is(notification.allNotifications.length, 1, "Notification Displayed.");

      executeSoon(function afterNotification() {
        waitForNotificationClose(notification.currentNotification, function onClose() {
          is(notification.allNotifications.length, 0, "No notifications remain.");
          is(policy.dataSubmissionPolicyAcceptedVersion, 1, "Version pref set.");
          ok(policy.dataSubmissionPolicyNotifiedDate.getTime() > -1, "Date pref set.");
          test_multiple_windows();
        });
        notification.currentNotification.close();
      });
    }, true);

    let [policy, promise] = sendNotifyRequest("single_window_notified");

    is(policy.dataSubmissionPolicyAcceptedVersion, 0, "No version should be set on init.");
    is(policy.dataSubmissionPolicyNotifiedDate.getTime(), 0, "No date should be set on init.");
    is(policy.userNotifiedOfCurrentPolicy, false, "User not notified about datareporting policy.");

    promise.then(function () {
      is(policy.dataSubmissionPolicyAcceptedVersion, 1, "Policy version set.");
      is(policy.dataSubmissionPolicyNotifiedDate.getTime() > 0, true, "Policy date set.");
      is(policy.userNotifiedOfCurrentPolicy, true, "User notified about datareporting policy.");
    }.bind(this), function (err) {
      throw err;
    });

  }.bind(this), function onError (err) {
    throw err;
  });
}

function test_multiple_windows() {
  // Ensure we see the notification on all windows and that action on one window
  // results in dismiss on every window.
  let window2 = OpenBrowserWindow();
  whenDelayedStartupFinished(window2, function onWindow() {
    let notification1 = document.getElementById("global-notificationbox");
    let notification2 = window2.document.getElementById("global-notificationbox");
    ok(notification2, "2nd window has a global notification box.");

    let [policy, promise] = sendNotifyRequest("multiple_window_behavior");
    let displayCount = 0;
    let prefWindowClosed = false;
    let mutationObserversRemoved = false;

    function onAlertDisplayed() {
      displayCount++;

      if (displayCount != 2) {
        return;
      }

      ok(true, "Data reporting info bar displayed on all open windows.");

      // We register two independent observers and we need both to clean up
      // properly. This handles gating for test completion.
      function maybeFinish() {
        if (!prefWindowClosed) {
          dump("Not finishing test yet because pref pane isn't closed.\n");
          return;
        }

        if (!mutationObserversRemoved) {
          dump("Not finishing test yet because mutation observers haven't been removed yet.\n");
          return;
        }

        window2.close();

        dump("Finishing multiple window test.\n");
        rootLogger.removeAppender(dumpAppender);
        dumpAppender = null;
        rootLogger = null;
        finish();
      }
      let closeCount = 0;

      function onAlertClose() {
        closeCount++;

        if (closeCount != 2) {
          return;
        }

        ok(true, "Closing info bar on one window closed them on all.");
        is(policy.userNotifiedOfCurrentPolicy, true, "Data submission policy accepted.");

        is(notification1.allNotifications.length, 0, "No notifications remain on main window.");
        is(notification2.allNotifications.length, 0, "No notifications remain on 2nd window.");

        mutationObserversRemoved = true;
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

        ok(true, "Advanced preferences opened on info bar button press.");
        executeSoon(function soon() {
          dump("Closing preferences.\n");
          prefWin.close();
          prefWindowClosed = true;
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

    promise.then(null, function onError(err) {
      throw err;
    });
  });
}

function cleanup () {
  // In case some test fails.
  if (originalPolicy) {
    let service = Cc["@mozilla.org/datareporting/service;1"]
                    .getService(Ci.nsISupports)
                    .wrappedJSObject;
    service.policy = originalPolicy;
  }

  return closeAllNotifications();
}
