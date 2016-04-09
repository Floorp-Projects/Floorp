
Cu.import("resource://gre/modules/UpdateUtils.jsm");

function getNotificationBox(aWindow) {
  return aWindow.document.getElementById("high-priority-global-notificationbox");
}

function promiseNotificationShown(aWindow, aName) {
  return new Promise((resolve) => {
    let notification = getNotificationBox(aWindow);
    notification.addEventListener("AlertActive", function active() {
      notification.removeEventListener("AlertActive", active, true);
      is(notification.allNotifications.length, 1, "Notification Displayed.");
      resolve(notification);
    });
  });
}

function promiseReportCallMade(aValue) {
  return new Promise((resolve) => {
    let old = gTestHangReport.testCallback;
    gTestHangReport.testCallback = function (val) {
      gTestHangReport.testCallback = old;
      is(aValue, val, "was the correct method call made on the hang report object?");
      resolve();
    };
  });
}

function pushPrefs(...aPrefs) {
  return new Promise((resolve) => {
    SpecialPowers.pushPrefEnv({"set": aPrefs}, resolve);
    resolve();
  });
}

function popPrefs() {
  return new Promise((resolve) => {
    SpecialPowers.popPrefEnv(resolve);
    resolve();
  });
}

let gTestHangReport = {
  SLOW_SCRIPT: 1,
  PLUGIN_HANG: 2,

  TEST_CALLBACK_CANCELED: 1,
  TEST_CALLBACK_TERMSCRIPT: 2,
  TEST_CALLBACK_TERMPLUGIN: 3,

  _hangType: 1,
  _tcb: function (aCallbackType) {},

  get hangType() {
    return this._hangType;
  },

  set hangType(aValue) {
    this._hangType = aValue;
  },

  set testCallback(aValue) {
    this._tcb = aValue;
  },

  QueryInterface: function (aIID) {
    if (aIID.equals(Components.interfaces.nsIHangReport) ||
        aIID.equals(Components.interfaces.nsISupports))
      return this;
    throw Components.results.NS_NOINTERFACE;
  },

  userCanceled: function () {
    this._tcb(this.TEST_CALLBACK_CANCELED);
  },

  terminateScript: function () {
    this._tcb(this.TEST_CALLBACK_TERMSCRIPT);
  },

  terminatePlugin: function () {
    this._tcb(this.TEST_CALLBACK_TERMPLUGIN);
  },

  isReportForBrowser: function(aFrameLoader) {
    return true;
  }
};

// on dev edition we add a button for js debugging of hung scripts.
let buttonCount = (UpdateUtils.UpdateChannel == "aurora" ? 3 : 2);

/**
 * Test if hang reports receive a terminate script callback when the user selects
 * stop in response to a script hang.
 */

add_task(function* terminateScriptTest() {
  let promise = promiseNotificationShown(window, "process-hang");
  Services.obs.notifyObservers(gTestHangReport, "process-hang-report", null);
  let notification = yield promise;

  let buttons = notification.currentNotification.getElementsByTagName("button");
  // Fails on aurora on-push builds, bug 1232204
  // is(buttons.length, buttonCount, "proper number of buttons");

  // Click the "Stop It" button, we should get a terminate script callback
  gTestHangReport.hangType = gTestHangReport.SLOW_SCRIPT;
  promise = promiseReportCallMade(gTestHangReport.TEST_CALLBACK_TERMSCRIPT);
  buttons[0].click();
  yield promise;
});

/**
 * Test if hang reports receive user canceled callbacks after a user selects wait
 * and the browser frees up from a script hang on its own.
 */

add_task(function* waitForScriptTest() {
  let promise = promiseNotificationShown(window, "process-hang");
  Services.obs.notifyObservers(gTestHangReport, "process-hang-report", null);
  let notification = yield promise;

  let buttons = notification.currentNotification.getElementsByTagName("button");
  // Fails on aurora on-push builds, bug 1232204
  // is(buttons.length, buttonCount, "proper number of buttons");

  yield pushPrefs(["browser.hangNotification.waitPeriod", 1000]);

  function nocbcheck() {
    ok(false, "received a callback?");
  }
  let oldcb = gTestHangReport.testCallback;
  gTestHangReport.testCallback = nocbcheck;
  // Click the "Wait" button this time, we shouldn't get a callback at all.
  buttons[1].click();
  gTestHangReport.testCallback = oldcb;

  // send another hang pulse, we should not get a notification here
  Services.obs.notifyObservers(gTestHangReport, "process-hang-report", null);
  is(notification.currentNotification, null, "no notification should be visible");

  gTestHangReport.testCallback = function() {};
  Services.obs.notifyObservers(gTestHangReport, "clear-hang-report", null);
  gTestHangReport.testCallback = oldcb;

  yield popPrefs();
});

/**
 * Test if hang reports receive user canceled callbacks after the content
 * process stops sending hang notifications.
 */

add_task(function* hangGoesAwayTest() {
  yield pushPrefs(["browser.hangNotification.expiration", 1000]);

  let promise = promiseNotificationShown(window, "process-hang");
  Services.obs.notifyObservers(gTestHangReport, "process-hang-report", null);
  yield promise;

  promise = promiseReportCallMade(gTestHangReport.TEST_CALLBACK_CANCELED);
  Services.obs.notifyObservers(gTestHangReport, "clear-hang-report", null);
  yield promise;

  yield popPrefs();
});

/**
 * Tests if hang reports receive a terminate plugin callback when the user selects
 * stop in response to a plugin hang.
 */

add_task(function* terminatePluginTest() {
  let promise = promiseNotificationShown(window, "process-hang");
  Services.obs.notifyObservers(gTestHangReport, "process-hang-report", null);
  let notification = yield promise;

  let buttons = notification.currentNotification.getElementsByTagName("button");
  // Fails on aurora on-push builds, bug 1232204
  // is(buttons.length, buttonCount, "proper number of buttons");

  // Click the "Stop It" button, we should get a terminate script callback
  gTestHangReport.hangType = gTestHangReport.PLUGIN_HANG;
  promise = promiseReportCallMade(gTestHangReport.TEST_CALLBACK_TERMPLUGIN);
  buttons[0].click();
  yield promise;
});
