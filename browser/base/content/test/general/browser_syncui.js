/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let {Weave} = Cu.import("resource://services-sync/main.js", {});
let {Notifications} = Cu.import("resource://services-sync/notifications.js", {});

let stringBundle = Cc["@mozilla.org/intl/stringbundle;1"]
                   .getService(Ci.nsIStringBundleService)
                   .createBundle("chrome://weave/locale/services/sync.properties");

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
    // this test leaves the tab focused which can cause browser_tabopen_reflows
    // to fail, so re-select the URLBar.
    gURLBar.select();
  });
});

add_task(function* testProlongedError() {
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

add_task(function* testLoginError() {
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

function testButtonActions(startNotification, endNotification) {
  let button = document.getElementById("sync-button");
  Assert.ok(button, "button exists");
  let panelbutton = document.getElementById("PanelUI-fxa-status");
  Assert.ok(panelbutton, "panel button exists");

  // pretend a sync is starting.
  Services.obs.notifyObservers(null, startNotification, null);
  Assert.equal(button.getAttribute("status"), "active");
  Assert.equal(panelbutton.getAttribute("syncstatus"), "active");

  Services.obs.notifyObservers(null, endNotification, null);
  Assert.ok(!button.hasAttribute("status"));
  Assert.ok(!panelbutton.hasAttribute("syncstatus"));
}

add_task(function* testButtonActivities() {
  // add the Sync button to the panel so we can get it!
  CustomizableUI.addWidgetToArea("sync-button", CustomizableUI.AREA_PANEL);
  // check the button's functionality
  yield PanelUI.show();
  try {
    testButtonActions("weave:service:login:start", "weave:service:login:finish");
    testButtonActions("weave:service:sync:start", "weave:ui:sync:finish");
  } finally {
    PanelUI.hide();
    CustomizableUI.removeWidgetFromArea("sync-button");
  }
});
