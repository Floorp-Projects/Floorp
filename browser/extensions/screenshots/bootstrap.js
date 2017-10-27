/* globals ADDON_DISABLE */
const OLD_ADDON_PREF_NAME = "extensions.jid1-NeEaf3sAHdKHPA@jetpack.deviceIdInfo";
const OLD_ADDON_ID = "jid1-NeEaf3sAHdKHPA@jetpack";
const ADDON_ID = "screenshots@mozilla.org";
const TELEMETRY_ENABLED_PREF = "datareporting.healthreport.uploadEnabled";
const PREF_BRANCH = "extensions.screenshots.";
const USER_DISABLE_PREF = "extensions.screenshots.disabled";

const { interfaces: Ci, utils: Cu } = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AppConstants",
                                  "resource://gre/modules/AppConstants.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Console",
                                  "resource://gre/modules/Console.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CustomizableUI",
                                  "resource:///modules/CustomizableUI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "LegacyExtensionsUtils",
                                  "resource://gre/modules/LegacyExtensionsUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PageActions",
                                  "resource:///modules/PageActions.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

let addonResourceURI;
let appStartupDone;
let appStartupPromise = new Promise((resolve, reject) => {
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
    if (aData == USER_DISABLE_PREF) {
      // eslint-disable-next-line promise/catch-or-return
      appStartupPromise = appStartupPromise.then(handleStartup);
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

const LibraryButton = {
  ITEM_ID: "appMenu-library-screenshots",

  init(webExtension) {
    this._initialized = true;
    let permissionPages = [...webExtension.extension.permissions].filter(p => (/^https?:\/\//i).test(p));
    if (permissionPages.length > 1) {
      Cu.reportError(new Error("Should not have more than 1 permission page, but got: " + JSON.stringify(permissionPages)));
    }
    this.PAGE_TO_OPEN = permissionPages.length == 1 ? permissionPages[0].replace(/\*$/, "") : "https://screenshots.firefox.com/";
    this.PAGE_TO_OPEN += "shots";
    this.ICON_URL = webExtension.extension.getURL("icons/icon-16-v2.svg");
    this.ICON_URL_2X = webExtension.extension.getURL("icons/icon-32-v2.svg");
    this.LABEL = webExtension.extension.localizeMessage("libraryLabel");
    CustomizableUI.addListener(this);
    for (let win of CustomizableUI.windows) {
      this.onWindowOpened(win);
    }
  },

  uninit() {
    if (!this._initialized) {
      return;
    }
    for (let win of CustomizableUI.windows) {
      let item = win.document.getElementById(this.ITEM_ID);
      if (item) {
        item.remove();
      }
    }
    CustomizableUI.removeListener(this);
    this._initialized = false;
  },

  onWindowOpened(win) {
    let libraryViewInsertionPoint = win.document.getElementById("appMenu-library-remotetabs-button");
    // If the library view doesn't exist (on non-photon builds, for instance),
    // this will be null, and we bail out early.
    if (!libraryViewInsertionPoint) {
      return;
    }
    let parent = libraryViewInsertionPoint.parentNode;
    let {nextSibling} = libraryViewInsertionPoint;
    let item = win.document.createElement("toolbarbutton");
    item.className = "subviewbutton subviewbutton-iconic";
    item.addEventListener("command", () => win.openUILinkIn(this.PAGE_TO_OPEN, "tab"));
    item.id = this.ITEM_ID;
    let iconURL = win.devicePixelRatio >= 1.1 ? this.ICON_URL_2X : this.ICON_URL;
    item.setAttribute("image", iconURL);
    item.setAttribute("label", this.LABEL);

    parent.insertBefore(item, nextSibling);
  },
};

const APP_STARTUP = 1;
const APP_SHUTDOWN = 2;
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
  appStartupPromise = appStartupPromise.then(handleStartup);
}

function shutdown(data, reason) { // eslint-disable-line no-unused-vars
  prefObserver.unregister();
  const webExtension = LegacyExtensionsUtils.getEmbeddedExtensionFor({
    id: ADDON_ID,
    resourceURI: addonResourceURI
  });
  // Immediately exit if Firefox is exiting, #3323
  if (reason === APP_SHUTDOWN) {
    stop(webExtension, reason);
    return;
  }
  // Because the prefObserver is unregistered above, this _should_ terminate the promise chain.
  appStartupPromise = appStartupPromise.then(() => { stop(webExtension, reason); });
}

function install(data, reason) {} // eslint-disable-line no-unused-vars

function uninstall(data, reason) {} // eslint-disable-line no-unused-vars

function getBoolPref(pref) {
  return prefs.getPrefType(pref) && prefs.getBoolPref(pref);
}

function shouldDisable() {
  return getBoolPref(USER_DISABLE_PREF);
}

function handleStartup() {
  const webExtension = LegacyExtensionsUtils.getEmbeddedExtensionFor({
    id: ADDON_ID,
    resourceURI: addonResourceURI
  });

  if (!shouldDisable() && !webExtension.started) {
    return start(webExtension);
  } else if (shouldDisable()) {
    return stop(webExtension, ADDON_DISABLE);
  }
}

function start(webExtension) {
  return webExtension.startup(startupReason).then((api) => {
    api.browser.runtime.onMessage.addListener(handleMessage);
    LibraryButton.init(webExtension);
    initPhotonPageAction(api, webExtension);
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
  if (reason != APP_SHUTDOWN) {
    LibraryButton.uninit();
    if (photonPageAction) {
      photonPageAction.remove();
      photonPageAction = null;
    }
  }
  return Promise.resolve(webExtension.shutdown(reason));
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

let photonPageAction;

// If the current Firefox version supports Photon (57 and later), this sets up
// a Photon page action and removes the UI for the WebExtension browser action.
// Does nothing otherwise.  Ideally, in the future, WebExtension page actions
// and Photon page actions would be one in the same, but they aren't right now.
function initPhotonPageAction(api, webExtension) {
  let id = "screenshots";
  let port = null;

  let {tabManager} = webExtension.extension;

  // Make the page action.
  photonPageAction = PageActions.actionForID(id) || PageActions.addAction(new PageActions.Action({
    id,
    title: "Take a Screenshot",
    iconURL: webExtension.extension.getURL("icons/icon-32-v2.svg"),
    _insertBeforeActionID: null,
    onCommand(event, buttonNode) {
      if (port) {
        let browserWin = buttonNode.ownerGlobal;
        let tab = tabManager.getWrapper(browserWin.gBrowser.selectedTab);
        port.postMessage({
          type: "click",
          tab: {id: tab.id, url: tab.url}
        });
      }
    },
  }));

  // Establish a port to the WebExtension side.
  api.browser.runtime.onConnect.addListener((listenerPort) => {
    if (listenerPort.name != "photonPageActionPort") {
      return;
    }
    port = listenerPort;
    port.onMessage.addListener((message) => {
      switch (message.type) {
      case "setProperties":
        if (message.title) {
          photonPageAction.title = message.title;
        }
        if (message.iconPath) {
          photonPageAction.iconURL = webExtension.extension.getURL(message.iconPath);
        }
        break;
      default:
        console.error("Unrecognized message:", message);
        break;
      }
    });
  });
}
