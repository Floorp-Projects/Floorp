/* globals ADDON_DISABLE */
const OLD_ADDON_PREF_NAME = "extensions.jid1-NeEaf3sAHdKHPA@jetpack.deviceIdInfo";
const OLD_ADDON_ID = "jid1-NeEaf3sAHdKHPA@jetpack";
const ADDON_ID = "screenshots@mozilla.org";
const TELEMETRY_ENABLED_PREF = "datareporting.healthreport.uploadEnabled";
const PREF_BRANCH = "extensions.screenshots.";
const USER_DISABLE_PREF = "extensions.screenshots.disabled";
const SYSTEM_DISABLE_PREF = "extensions.screenshots.system-disabled";

const { interfaces: Ci, utils: Cu } = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Console",
                                  "resource://gre/modules/Console.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "LegacyExtensionsUtils",
                                  "resource://gre/modules/LegacyExtensionsUtils.jsm");

let addonResourceURI;
let appStartupDone;
const appStartupPromise = new Promise((resolve, reject) => {
  appStartupDone = resolve;
});

const prefs = Services.prefs;
const prefObserver = {
  register() {
    prefs.addObserver(PREF_BRANCH, this, false); // eslint-disable-line mozilla/no-useless-parameters
  },

  unregister() {
    prefs.removeObserver(PREF_BRANCH, this, false); // eslint-disable-line mozilla/no-useless-parameters
  },

  observe(aSubject, aTopic, aData) {
    // aSubject is the nsIPrefBranch we're observing (after appropriate QI)
    // aData is the name of the pref that's been changed (relative to aSubject)
    if (aData == USER_DISABLE_PREF || aData == SYSTEM_DISABLE_PREF) {
      // eslint-disable-next-line promise/catch-or-return
      appStartupPromise.then(handleStartup);
    }
  }
};

const appStartupObserver = {
  register() {
    Services.obs.addObserver(this, "sessionstore-windows-restored", false); // eslint-disable-line mozilla/no-useless-parameters
  },

  unregister() {
    Services.obs.removeObserver(this, "sessionstore-windows-restored", false); // eslint-disable-line mozilla/no-useless-parameters
  },

  observe() {
    appStartupDone();
    this.unregister();
  }
}

const APP_STARTUP = 1;
let startupReason;

function startup(data, reason) { // eslint-disable-line no-unused-vars
  startupReason = reason;
  if (reason === APP_STARTUP) {
    appStartupObserver.register();
  } else {
    appStartupDone();
  }
  prefObserver.register();
  addonResourceURI = data.resourceURI;
  // eslint-disable-next-line promise/catch-or-return
  appStartupPromise.then(handleStartup);
}

function shutdown(data, reason) { // eslint-disable-line no-unused-vars
  prefObserver.unregister();
  const webExtension = LegacyExtensionsUtils.getEmbeddedExtensionFor({
    id: ADDON_ID,
    resourceURI: addonResourceURI
  });
  stop(webExtension, reason);
}

function install(data, reason) {} // eslint-disable-line no-unused-vars

function uninstall(data, reason) {} // eslint-disable-line no-unused-vars

function getBoolPref(pref) {
  return prefs.getPrefType(pref) && prefs.getBoolPref(pref);
}

function shouldDisable() {
  return getBoolPref(USER_DISABLE_PREF) || getBoolPref(SYSTEM_DISABLE_PREF);
}

function handleStartup() {
  const webExtension = LegacyExtensionsUtils.getEmbeddedExtensionFor({
    id: ADDON_ID,
    resourceURI: addonResourceURI
  });

  if (!shouldDisable() && !webExtension.started) {
    start(webExtension);
  } else if (shouldDisable()) {
    stop(webExtension, ADDON_DISABLE);
  }
}

function start(webExtension) {
  webExtension.startup(startupReason).then((api) => {
    api.browser.runtime.onMessage.addListener(handleMessage);
  }).catch((err) => {
    // The startup() promise will be rejected if the webExtension was
    // already started (a harmless error), or if initializing the
    // WebExtension failed and threw (an important error).
    console.error(err);
    if (err.message !== "This embedded extension has already been started") {
      // TODO: Should we send these errors to Sentry? #2420
    }
  });
}

function stop(webExtension, reason) {
  webExtension.shutdown(reason);
}

function handleMessage(msg, sender, sendReply) {
  if (!msg) {
    return;
  }

  if (msg.funcName === "getTelemetryPref") {
    let telemetryEnabled = getBoolPref(TELEMETRY_ENABLED_PREF);
    sendReply({type: "success", value: telemetryEnabled});
  } else if (msg.funcName === "getOldDeviceInfo") {
    let oldDeviceInfo = prefs.prefHasUserValue(OLD_ADDON_PREF_NAME) && prefs.getCharPref(OLD_ADDON_PREF_NAME);
    sendReply({type: "success", value: oldDeviceInfo || null});
  } else if (msg.funcName === "removeOldAddon") {
    AddonManager.getAddonByID(OLD_ADDON_ID, (addon) => {
      prefs.clearUserPref(OLD_ADDON_PREF_NAME);
      if (addon) {
        addon.uninstall();
      }
      sendReply({type: "success", value: !!addon});
    });
    return true;
  }
}
