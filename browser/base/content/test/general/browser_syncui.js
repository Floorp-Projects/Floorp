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

// Sync manages 3 broadcasters so the menus correctly reflect the Sync state.
// Only one of these 3 should ever be visible - pass the ID of the broadcaster
// you expect to be visible and it will check it's the only one that is.
function checkBroadcasterVisible(broadcasterId) {
  let all = ["sync-reauth-state", "sync-setup-state", "sync-syncnow-state"];
  Assert.ok(all.indexOf(broadcasterId) >= 0, "valid id");
  for (let check of all) {
    let eltHidden = document.getElementById(check).hidden;
    Assert.equal(eltHidden, check == broadcasterId ? false : true, check);
  }
}

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
  checkBroadcasterVisible("sync-setup-state");
  // mock out the "_needsSetup()" function so we don't short-circuit.
  let oldNeedsSetup = window.gSyncUI._needsSetup;
  window.gSyncUI._needsSetup = () => false;
  registerCleanupFunction(() => {
    window.gSyncUI._needsSetup = oldNeedsSetup;
  });
  // and a notification to have the state change away from "needs setup"
  Services.obs.notifyObservers(null, "weave:ui:clear-error", null);
  checkBroadcasterVisible("sync-syncnow-state");
});

add_task(function* testSyncLoginError() {
  Assert.equal(Notifications.notifications.length, 0, "start with no notifications");
  checkBroadcasterVisible("sync-syncnow-state");

  // Pretend we are in a "login failed" error state
  Weave.Status.sync = Weave.LOGIN_FAILED;
  Weave.Status.login = Weave.LOGIN_FAILED_LOGIN_REJECTED;
  Services.obs.notifyObservers(null, "weave:ui:sync:error", null);

  Assert.equal(Notifications.notifications.length, 0, "no notifications shown on login error");
  // But the menu *should* reflect the login error.
  checkBroadcasterVisible("sync-reauth-state");

  // Now pretend we just had a successful login - the error notification should go away.
  Weave.Status.sync = Weave.STATUS_OK;
  Weave.Status.login = Weave.LOGIN_SUCCEEDED;
  Services.obs.notifyObservers(null, "weave:service:login:start", null);
  Services.obs.notifyObservers(null, "weave:service:login:finish", null);
  Assert.equal(Notifications.notifications.length, 0, "no notifications left");
  // The menus should be back to "all good"
  checkBroadcasterVisible("sync-syncnow-state");
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
