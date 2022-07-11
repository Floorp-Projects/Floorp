/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["QuickActionsLoaderDefault"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};
XPCOMUtils.defineLazyModuleGetters(lazy, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  DevToolsShim: "chrome://devtools-startup/content/DevToolsShim.jsm",
  UrlbarProviderQuickActions:
    "resource:///modules/UrlbarProviderQuickActions.jsm",
});

const BASE_URL = Services.urlFormatter.formatURLPref("app.support.baseURL");

let openUrlFun = url => () => openUrl(url);
let openUrl = url => {
  let window = lazy.BrowserWindowTracker.getTopWindow();
  window.gBrowser.loadOneTab(url, {
    inBackground: false,
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });
};

// There are various actions that will either fail or do not
// make sense to use unless the user is viewing web content,
// Screenshots for example cannot be taken on about: pages.
// We may want to show these as disabled buttons, that may
// aid discovery but may also confuse users.
let currentPageIsWebContentFilter = () =>
  currentBrowser().currentURI.spec.startsWith("about:");
let currentBrowser = () =>
  lazy.BrowserWindowTracker.getTopWindow().gBrowser.selectedBrowser;

const DEFAULT_ACTIONS = {
  clear: {
    commands: ["clear"],
    label: "quickactions-clear",
    onPick: openUrlFun(
      `${BASE_URL}delete-browsing-search-download-history-firefox`
    ),
  },
  downloads: {
    commands: ["downloads"],
    icon: "chrome://browser/skin/downloads/downloads.svg",
    label: "quickactions-downloads",
    onPick: openUrlFun("about:downloads"),
  },
  inspect: {
    commands: ["inspector"],
    icon: "chrome://devtools/skin/images/tool-inspector.svg",
    label: "quickactions-inspector",
    isActive: currentPageIsWebContentFilter,
    onPick: openInspector,
  },
  print: {
    commands: ["print"],
    label: "quickactions-print",
    isActive: currentPageIsWebContentFilter,
    onPick: () => {
      lazy.BrowserWindowTracker.getTopWindow()
        .document.getElementById("cmd_print")
        .doCommand();
    },
  },
  refresh: {
    commands: ["reset"],
    label: "quickactions-refresh",
    onPick: openUrlFun(`${BASE_URL}refresh-firefox-reset-add-ons-and-settings`),
  },
  restart: {
    commands: ["restart"],
    label: "quickactions-restart",
    onPick: restartBrowser,
  },
  screenshot: {
    commands: ["screenshot"],
    label: "quickactions-screenshot",
    isActive: currentPageIsWebContentFilter,
    onPick: () => {
      Services.obs.notifyObservers(
        null,
        "menuitem-screenshot-extension",
        "contextMenu"
      );
    },
  },
  settings: {
    commands: ["settings"],
    label: "quickactions-settings",
    onPick: openUrlFun("about:preferences"),
  },
  update: {
    commands: ["update"],
    label: "quickactions-update",
    onPick: openUrlFun(`${BASE_URL}update-firefox-latest-release`),
  },
  viewsource: {
    commands: ["view-source"],
    icon: "chrome://global/skin/icons/settings.svg",
    label: "quickactions-viewsource",
    isActive: currentPageIsWebContentFilter,
    onPick: () => openUrl("view-source:" + currentBrowser().currentURI.spec),
  },
};

function openInspector() {
  // TODO: This is supposed to be called with an element to start inspecting.
  lazy.DevToolsShim.inspectNode(
    lazy.BrowserWindowTracker.getTopWindow().gBrowser.selectedTab
  );
}

// TODO: We likely want a prompt to confirm with the user that they want to restart
// the browser.
function restartBrowser() {
  // Notify all windows that an application quit has been requested.
  let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].createInstance(
    Ci.nsISupportsPRBool
  );
  Services.obs.notifyObservers(
    cancelQuit,
    "quit-application-requested",
    "restart"
  );
  // Something aborted the quit process.
  if (cancelQuit.data) {
    return;
  }
  // If already in safe mode restart in safe mode.
  if (Services.appinfo.inSafeMode) {
    Services.startup.restartInSafeMode(Ci.nsIAppStartup.eAttemptQuit);
  } else {
    Services.startup.quit(
      Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart
    );
  }
}

/**
 * Loads the default QuickActions.
 */
class QuickActionsLoaderDefault {
  static load() {
    for (const key in DEFAULT_ACTIONS) {
      lazy.UrlbarProviderQuickActions.addAction(key, DEFAULT_ACTIONS[key]);
    }
  }
}
