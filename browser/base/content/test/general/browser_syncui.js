/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let {Log} = Cu.import("resource://gre/modules/Log.jsm", {});
let {Weave} = Cu.import("resource://services-sync/main.js", {});
let {Notifications} = Cu.import("resource://services-sync/notifications.js", {});

let stringBundle = Cc["@mozilla.org/intl/stringbundle;1"]
                   .getService(Ci.nsIStringBundleService)
                   .createBundle("chrome://weave/locale/services/sync.properties");

// ensure test output sees log messages.
Log.repository.getLogger("browserwindow.syncui").addAppender(new Log.DumpAppender());

function promiseObserver(topic) {
  return new Promise(resolve => {
    let obs = (subject, topic, data) => {
      Services.obs.removeObserver(obs, topic);
      resolve(subject);
    }
    Services.obs.addObserver(obs, topic, false);
  });
}

add_task(function* prepare() {
  let xps = Components.classes["@mozilla.org/weave/service;1"]
                              .getService(Components.interfaces.nsISupports)
                              .wrappedJSObject;
  yield xps.whenLoaded();
  // mock out the "_needsSetup()" function so we don't short-circuit.
  let oldNeedsSetup = window.gSyncUI._needsSetup;
  window.gSyncUI._needsSetup = () => false;
  registerCleanupFunction(() => {
    window.gSyncUI._needsSetup = oldNeedsSetup;
  });
});

add_task(function* testProlongedSyncError() {
  let promiseNotificationAdded = promiseObserver("weave:notification:added");
  Assert.equal(Notifications.notifications.length, 0, "start with no notifications");

  // Pretend we are in the "prolonged error" state.
  Weave.Status.sync = Weave.PROLONGED_SYNC_FAILURE;
  Weave.Status.login = Weave.LOGIN_SUCCEEDED;
  Services.obs.notifyObservers(null, "weave:ui:sync:error", null);

  let subject = yield promiseNotificationAdded;
  let notification = subject.wrappedJSObject.object; // sync's observer abstraction is abstract!
  Assert.equal(notification.title, stringBundle.GetStringFromName("error.sync.title"));
  Assert.equal(Notifications.notifications.length, 1, "exactly 1 notification");

  // Now pretend we just had a successful sync - the error notification should go away.
  let promiseNotificationRemoved = promiseObserver("weave:notification:removed");
  Weave.Status.sync = Weave.STATUS_OK;
  Services.obs.notifyObservers(null, "weave:ui:sync:finish", null);
  yield promiseNotificationRemoved;
  Assert.equal(Notifications.notifications.length, 0, "no notifications left");
});

add_task(function* testSyncLoginError() {
  let promiseNotificationAdded = promiseObserver("weave:notification:added");
  Assert.equal(Notifications.notifications.length, 0, "start with no notifications");

  // Pretend we are in the "prolonged error" state.
  Weave.Status.sync = Weave.LOGIN_FAILED;
  Weave.Status.login = Weave.LOGIN_FAILED_LOGIN_REJECTED;
  Services.obs.notifyObservers(null, "weave:ui:sync:error", null);

  let subject = yield promiseNotificationAdded;
  let notification = subject.wrappedJSObject.object; // sync's observer abstraction is abstract!
  Assert.equal(notification.title, stringBundle.GetStringFromName("error.login.title"));
  Assert.equal(Notifications.notifications.length, 1, "exactly 1 notification");

  // Now pretend we just had a successful login - the error notification should go away.
  Weave.Status.sync = Weave.STATUS_OK;
  Weave.Status.login = Weave.LOGIN_SUCCEEDED;
  let promiseNotificationRemoved = promiseObserver("weave:notification:removed");
  Services.obs.notifyObservers(null, "weave:service:login:start", null);
  Services.obs.notifyObservers(null, "weave:service:login:finish", null);
  yield promiseNotificationRemoved;
  Assert.equal(Notifications.notifications.length, 0, "no notifications left");
});

add_task(function* testSyncLoginNetworkError() {
  Assert.equal(Notifications.notifications.length, 0, "start with no notifications");

  // This test relies on the fact that observers are synchronous, and that error
  // notifications synchronously create the error notification, which itself
  // fires an observer notification.
  // ie, we should see the error notification *during* the notifyObservers call.

  // To prove that, we cause a notification that *does* show an error and make
  // sure we see the error notification during that call. We then cause a
  // notification that *should not* show an error, and the lack of the
  // notification during the call implies the error was ignored.

  // IOW, if this first part of the test fails in the future, it means the
  // above is no longer true and we need a different strategy to check for
  // ignored errors.

  let sawNotificationAdded = false;
  let obs = (subject, topic, data) => {
    sawNotificationAdded = true;
  }
  Services.obs.addObserver(obs, "weave:notification:added", false);
  try {
    // notify of a display-able error - we should synchronously see our flag set.
    Weave.Status.sync = Weave.LOGIN_FAILED;
    Weave.Status.login = Weave.LOGIN_FAILED_LOGIN_REJECTED;
    Services.obs.notifyObservers(null, "weave:ui:login:error", null);
    Assert.ok(sawNotificationAdded);

    // reset the flag and test what should *not* show an error.
    sawNotificationAdded = false;
    Weave.Status.sync = Weave.LOGIN_FAILED;
    Weave.Status.login = Weave.LOGIN_FAILED_NETWORK_ERROR;
    Services.obs.notifyObservers(null, "weave:ui:login:error", null);
    Assert.ok(!sawNotificationAdded);

    // ditto for LOGIN_FAILED_SERVER_ERROR
    Weave.Status.sync = Weave.LOGIN_FAILED;
    Weave.Status.login = Weave.LOGIN_FAILED_SERVER_ERROR;
    Services.obs.notifyObservers(null, "weave:ui:login:error", null);
    Assert.ok(!sawNotificationAdded);
    // we are done.
  } finally {
    Services.obs.removeObserver(obs, "weave:notification:added");
  }
});

function checkButtonsStatus(shouldBeActive) {
  let button = document.getElementById("sync-button");
  let fxaContainer = document.getElementById("PanelUI-footer-fxa");
  if (shouldBeActive) {
    Assert.equal(button.getAttribute("status"), "active");
    Assert.equal(fxaContainer.getAttribute("syncstatus"), "active");
  } else {
    Assert.ok(!button.hasAttribute("status"));
    Assert.ok(!fxaContainer.hasAttribute("syncstatus"));
  }
}

function testButtonActions(startNotification, endNotification) {
  checkButtonsStatus(false);
  // pretend a sync is starting.
  Services.obs.notifyObservers(null, startNotification, null);
  checkButtonsStatus(true);
  // and has stopped
  Services.obs.notifyObservers(null, endNotification, null);
  checkButtonsStatus(false);
}

add_task(function* testButtonActivities() {
  // add the Sync button to the panel so we can get it!
  CustomizableUI.addWidgetToArea("sync-button", CustomizableUI.AREA_PANEL);
  // check the button's functionality
  yield PanelUI.show();
  try {
    testButtonActions("weave:service:login:start", "weave:service:login:finish");
    testButtonActions("weave:service:login:start", "weave:service:login:error");

    testButtonActions("weave:service:sync:start", "weave:service:sync:finish");
    testButtonActions("weave:service:sync:start", "weave:service:sync:error");

    // and ensure the counters correctly handle multiple in-flight syncs
    Services.obs.notifyObservers(null, "weave:service:sync:start", null);
    checkButtonsStatus(true);
    // sync stops.
    Services.obs.notifyObservers(null, "weave:service:sync:finish", null);
    // Button should not be active.
    checkButtonsStatus(false);
  } finally {
    PanelUI.hide();
    CustomizableUI.removeWidgetFromArea("sync-button");
  }
});
