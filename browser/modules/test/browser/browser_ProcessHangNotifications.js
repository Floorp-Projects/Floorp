/* globals ProcessHangMonitor */

const { WebExtensionPolicy } =
  Cu.getGlobalForObject(ChromeUtils.import("resource://gre/modules/Services.jsm", {}));

ChromeUtils.import("resource://gre/modules/UpdateUtils.jsm");

function getNotificationBox(aWindow) {
  return aWindow.document.getElementById("high-priority-global-notificationbox");
}

function promiseNotificationShown(aWindow, aName) {
  return new Promise((resolve) => {
    let notification = getNotificationBox(aWindow);
    notification.addEventListener("AlertActive", function() {
      is(notification.allNotifications.length, 1, "Notification Displayed.");
      resolve(notification);
    }, {once: true});
  });
}

function pushPrefs(...aPrefs) {
  return SpecialPowers.pushPrefEnv({"set": aPrefs});
}

function popPrefs() {
  return SpecialPowers.popPrefEnv();
}

const TEST_ACTION_UNKNOWN = 0;
const TEST_ACTION_CANCELLED = 1;
const TEST_ACTION_TERMSCRIPT = 2;
const TEST_ACTION_TERMPLUGIN = 3;
const TEST_ACTION_TERMGLOBAL = 4;
const SLOW_SCRIPT = 1;
const PLUGIN_HANG = 2;
const ADDON_HANG = 3;
const ADDON_ID = "fake-addon";

/**
 * A mock nsIHangReport that we can pass through nsIObserverService
 * to trigger notifications.
 *
 * @param hangType
 *        One of SLOW_SCRIPT, PLUGIN_HANG, ADDON_HANG.
 * @param browser (optional)
 *        The <xul:browser> that this hang should be associated with.
 *        If not supplied, the hang will be associated with every browser,
 *        but the nsIHangReport.scriptBrowser attribute will return the
 *        currently selected browser in this window's gBrowser.
 */
let TestHangReport = function(hangType = SLOW_SCRIPT,
                              browser = gBrowser.selectedBrowser) {
  this.promise = new Promise((resolve, reject) => {
    this._resolver = resolve;
  });

  if (hangType == ADDON_HANG) {
    // Add-on hangs are actually script hangs, but have an associated
    // add-on ID for us to blame.
    this._hangType = SLOW_SCRIPT;
    this._addonId = ADDON_ID;
  } else {
    this._hangType = hangType;
  }

  this._browser = browser;
};

TestHangReport.prototype = {
  SLOW_SCRIPT,
  PLUGIN_HANG,

  get addonId() {
    return this._addonId;
  },

  get hangType() {
    return this._hangType;
  },

  QueryInterface: ChromeUtils.generateQI(["nsIHangReport"]),

  userCanceled() {
    this._resolver(TEST_ACTION_CANCELLED);
  },

  terminateScript() {
    this._resolver(TEST_ACTION_TERMSCRIPT);
  },

  terminatePlugin() {
    this._resolver(TEST_ACTION_TERMPLUGIN);
  },

  terminateGlobal() {
    this._resolver(TEST_ACTION_TERMGLOBAL);
  },

  isReportForBrowser(aFrameLoader) {
    if (this._browser) {
      return this._browser.frameLoader === aFrameLoader;
    }

    return true;
  },

  get scriptBrowser() {
    return this._browser;
  }
};

// on dev edition we add a button for js debugging of hung scripts.
let buttonCount = (UpdateUtils.UpdateChannel == "aurora" ? 3 : 2);

add_task(async function setup() {
  // Create a fake WebExtensionPolicy that we can use for
  // the add-on hang notification.
  const uuidGen = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);
  const uuid = uuidGen.generateUUID().number.slice(1, -1);
  let policy = new WebExtensionPolicy({
    name: "Scapegoat",
    id: ADDON_ID,
    mozExtensionHostname: uuid,
    baseURL: "file:///",
    allowedOrigins: new MatchPatternSet([]),
    localizeCallback() {},
  });
  policy.active = true;

  registerCleanupFunction(() => {
    policy.active = false;
  });
});

/**
 * Test if hang reports receive a terminate script callback when the user selects
 * stop in response to a script hang.
 */
add_task(async function terminateScriptTest() {
  let promise = promiseNotificationShown(window, "process-hang");
  let hangReport = new TestHangReport();
  Services.obs.notifyObservers(hangReport, "process-hang-report");
  let notification = await promise;

  let buttons = notification.currentNotification.getElementsByTagName("button");
  // Fails on aurora on-push builds, bug 1232204
  // is(buttons.length, buttonCount, "proper number of buttons");

  // Click the "Stop It" button, we should get a terminate script callback
  buttons[0].click();
  let action = await hangReport.promise;
  is(action, TEST_ACTION_TERMSCRIPT,
     "Clicking 'Stop It' should have terminated the script.");
});

/**
 * Test if hang reports receive user canceled callbacks after a user selects wait
 * and the browser frees up from a script hang on its own.
 */
add_task(async function waitForScriptTest() {
  let hangReport = new TestHangReport();
  let promise = promiseNotificationShown(window, "process-hang");
  Services.obs.notifyObservers(hangReport, "process-hang-report");
  let notification = await promise;

  let buttons = notification.currentNotification.getElementsByTagName("button");
  // Fails on aurora on-push builds, bug 1232204
  // is(buttons.length, buttonCount, "proper number of buttons");

  await pushPrefs(["browser.hangNotification.waitPeriod", 1000]);

  let ignoringReport = true;

  hangReport.promise.then((action) => {
    if (ignoringReport) {
      ok(false,
         "Hang report was somehow dealt with when it " +
         "should have been ignored.");
    } else {
      is(action, TEST_ACTION_CANCELLED,
         "Hang report should have been cancelled.");
    }
  });

  // Click the "Wait" button this time, we shouldn't get a callback at all.
  buttons[1].click();

  // send another hang pulse, we should not get a notification here
  Services.obs.notifyObservers(hangReport, "process-hang-report");
  is(notification.currentNotification, null, "no notification should be visible");

  // Make sure that any queued Promises have run to give our report-ignoring
  // then() a chance to fire.
  await Promise.resolve();

  ignoringReport = false;
  Services.obs.notifyObservers(hangReport, "clear-hang-report");

  await popPrefs();
});

/**
 * Test if hang reports receive user canceled callbacks after the content
 * process stops sending hang notifications.
 */
add_task(async function hangGoesAwayTest() {
  await pushPrefs(["browser.hangNotification.expiration", 1000]);

  let hangReport = new TestHangReport();
  let promise = promiseNotificationShown(window, "process-hang");
  Services.obs.notifyObservers(hangReport, "process-hang-report");
  await promise;

  Services.obs.notifyObservers(hangReport, "clear-hang-report");
  let action = await hangReport.promise;
  is(action, TEST_ACTION_CANCELLED,
     "Hang report should have been cancelled.");

  await popPrefs();
});

/**
 * Tests if hang reports receive a terminate plugin callback when the user selects
 * stop in response to a plugin hang.
 */
add_task(async function terminatePluginTest() {
  let hangReport = new TestHangReport(PLUGIN_HANG);
  let promise = promiseNotificationShown(window, "process-hang");
  Services.obs.notifyObservers(hangReport, "process-hang-report");
  let notification = await promise;

  let buttons = notification.currentNotification.getElementsByTagName("button");
  // Fails on aurora on-push builds, bug 1232204
  // is(buttons.length, buttonCount, "proper number of buttons");

  // Click the "Stop It" button, we should get a terminate script callback
  buttons[0].click();
  let action = await hangReport.promise;
  is(action, TEST_ACTION_TERMPLUGIN,
     "Expected the 'Stop it' button to terminate the plug-in");
});

/**
 * Tests that if we're shutting down, any pre-existing hang reports will
 * be terminated appropriately.
 */
add_task(async function terminateAtShutdown() {
  let pausedHang = new TestHangReport(SLOW_SCRIPT);
  Services.obs.notifyObservers(pausedHang, "process-hang-report");
  ProcessHangMonitor.waitLonger(window);
  ok(ProcessHangMonitor.findPausedReport(gBrowser.selectedBrowser),
     "There should be a paused report for the selected browser.");

  let pluginHang = new TestHangReport(PLUGIN_HANG);
  let scriptHang = new TestHangReport(SLOW_SCRIPT);
  let addonHang = new TestHangReport(ADDON_HANG);

  [pluginHang, scriptHang, addonHang].forEach(hangReport => {
    Services.obs.notifyObservers(hangReport, "process-hang-report");
  });

  // Simulate the browser being told to shutdown. This should cause
  // hangs to terminate scripts / plugins.
  ProcessHangMonitor.onQuitApplicationGranted();

  // In case this test happens to throw before it can finish, make
  // sure to reset the shutting-down state.
  registerCleanupFunction(() => {
    ProcessHangMonitor._shuttingDown = false;
  });

  let pausedAction = await pausedHang.promise;
  let pluginAction = await pluginHang.promise;
  let scriptAction = await scriptHang.promise;
  let addonAction = await addonHang.promise;

  is(pausedAction, TEST_ACTION_TERMSCRIPT,
     "On shutdown, should have terminated script for paused script hang.");
  is(pluginAction, TEST_ACTION_TERMPLUGIN,
     "On shutdown, should have terminated plugin for plugin hang.");
  is(scriptAction, TEST_ACTION_TERMSCRIPT,
     "On shutdown, should have terminated script for script hang.");
  is(addonAction, TEST_ACTION_TERMGLOBAL,
     "On shutdown, should have terminated global for add-on hang.");

  // ProcessHangMonitor should now be in the "shutting down" state,
  // meaning that any further hangs should be handled immediately
  // without user interaction.
  let pluginHang2 = new TestHangReport(PLUGIN_HANG);
  let scriptHang2 = new TestHangReport(SLOW_SCRIPT);
  let addonHang2 = new TestHangReport(ADDON_HANG);

  [pluginHang2, scriptHang2, addonHang2].forEach(hangReport => {
    Services.obs.notifyObservers(hangReport, "process-hang-report");
  });

  let pluginAction2 = await pluginHang.promise;
  let scriptAction2 = await scriptHang.promise;
  let addonAction2 = await addonHang.promise;

  is(pluginAction2, TEST_ACTION_TERMPLUGIN,
     "On shutdown, should have terminated plugin for plugin hang.");
  is(scriptAction2, TEST_ACTION_TERMSCRIPT,
     "On shutdown, should have terminated script for script hang.");
  is(addonAction2, TEST_ACTION_TERMGLOBAL,
     "On shutdown, should have terminated global for add-on hang.");

  ProcessHangMonitor._shuttingDown = false;
});

/**
 * Test that if there happens to be no open browser windows, that any
 * hang reports that exist or appear while in this state will be handled
 * automatically.
 */
add_task(async function terminateNoWindows() {
  let testWin = await BrowserTestUtils.openNewBrowserWindow();

  let pausedHang = new TestHangReport(SLOW_SCRIPT, testWin.gBrowser.selectedBrowser);
  Services.obs.notifyObservers(pausedHang, "process-hang-report");
  ProcessHangMonitor.waitLonger(testWin);
  ok(ProcessHangMonitor.findPausedReport(testWin.gBrowser.selectedBrowser),
     "There should be a paused report for the selected browser.");

  let pluginHang = new TestHangReport(PLUGIN_HANG);
  let scriptHang = new TestHangReport(SLOW_SCRIPT);
  let addonHang = new TestHangReport(ADDON_HANG);

  [pluginHang, scriptHang, addonHang].forEach(hangReport => {
    Services.obs.notifyObservers(hangReport, "process-hang-report");
  });

  // Quick and dirty hack to trick the window mediator into thinking there
  // are no browser windows without actually closing all browser windows.
  document.documentElement.setAttribute("windowtype", "navigator:browsertestdummy");

  // In case this test happens to throw before it can finish, make
  // sure to reset this.
  registerCleanupFunction(() => {
    document.documentElement.setAttribute("windowtype", "navigator:browser");
  });

  await BrowserTestUtils.closeWindow(testWin);

  let pausedAction = await pausedHang.promise;
  let pluginAction = await pluginHang.promise;
  let scriptAction = await scriptHang.promise;
  let addonAction = await addonHang.promise;

  is(pausedAction, TEST_ACTION_TERMSCRIPT,
     "With no open windows, should have terminated script for paused script hang.");
  is(pluginAction, TEST_ACTION_TERMPLUGIN,
     "With no open windows, should have terminated plugin for plugin hang.");
  is(scriptAction, TEST_ACTION_TERMSCRIPT,
     "With no open windows, should have terminated script for script hang.");
  is(addonAction, TEST_ACTION_TERMGLOBAL,
     "With no open windows, should have terminated global for add-on hang.");

  // ProcessHangMonitor should notice we're in the "no windows" state,
  // so any further hangs should be handled immediately without user
  // interaction.
  let pluginHang2 = new TestHangReport(PLUGIN_HANG);
  let scriptHang2 = new TestHangReport(SLOW_SCRIPT);
  let addonHang2 = new TestHangReport(ADDON_HANG);

  [pluginHang2, scriptHang2, addonHang2].forEach(hangReport => {
    Services.obs.notifyObservers(hangReport, "process-hang-report");
  });

  let pluginAction2 = await pluginHang.promise;
  let scriptAction2 = await scriptHang.promise;
  let addonAction2 = await addonHang.promise;

  is(pluginAction2, TEST_ACTION_TERMPLUGIN,
     "With no open windows, should have terminated plugin for plugin hang.");
  is(scriptAction2, TEST_ACTION_TERMSCRIPT,
     "With no open windows, should have terminated script for script hang.");
  is(addonAction2, TEST_ACTION_TERMGLOBAL,
     "With no open windows, should have terminated global for add-on hang.");

  document.documentElement.setAttribute("windowtype", "navigator:browser");
});

/**
 * Test that if a script hang occurs in one browser window, and that
 * browser window goes away, that we clear the hang. For plug-in hangs,
 * we do the conservative thing and terminate any plug-in hangs when a
 * window closes, even though we don't exactly know which window it
 * belongs to.
 */
add_task(async function terminateClosedWindow() {
  let testWin = await BrowserTestUtils.openNewBrowserWindow();
  let testBrowser = testWin.gBrowser.selectedBrowser;

  let pausedHang = new TestHangReport(SLOW_SCRIPT, testBrowser);
  Services.obs.notifyObservers(pausedHang, "process-hang-report");
  ProcessHangMonitor.waitLonger(testWin);
  ok(ProcessHangMonitor.findPausedReport(testWin.gBrowser.selectedBrowser),
     "There should be a paused report for the selected browser.");

  let pluginHang = new TestHangReport(PLUGIN_HANG, testBrowser);
  let scriptHang = new TestHangReport(SLOW_SCRIPT, testBrowser);
  let addonHang = new TestHangReport(ADDON_HANG, testBrowser);

  [pluginHang, scriptHang, addonHang].forEach(hangReport => {
    Services.obs.notifyObservers(hangReport, "process-hang-report");
  });

  await BrowserTestUtils.closeWindow(testWin);

  let pausedAction = await pausedHang.promise;
  let pluginAction = await pluginHang.promise;
  let scriptAction = await scriptHang.promise;
  let addonAction = await addonHang.promise;

  is(pausedAction, TEST_ACTION_TERMSCRIPT,
     "When closing window, should have terminated script for a paused script hang.");
  is(pluginAction, TEST_ACTION_TERMPLUGIN,
     "When closing window, should have terminated hung plug-in.");
  is(scriptAction, TEST_ACTION_TERMSCRIPT,
     "When closing window, should have terminated script for script hang.");
  is(addonAction, TEST_ACTION_TERMGLOBAL,
     "When closing window, should have terminated global for add-on hang.");
});
