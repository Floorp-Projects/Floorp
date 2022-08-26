/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  UrlbarProviderQuickActions:
    "resource:///modules/UrlbarProviderQuickActions.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(lazy, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  DevToolsShim: "chrome://devtools-startup/content/DevToolsShim.jsm",
});

const BASE_URL = "https://support.mozilla.org/kb/";

let openUrlFun = url => () => openUrl(url);
let openUrl = url => {
  let window = lazy.BrowserWindowTracker.getTopWindow();

  if (url.startsWith("about:")) {
    window.switchToTabHavingURI(Services.io.newURI(url), true);
  } else {
    window.gBrowser.loadOneTab(url, {
      inBackground: false,
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
  }
  return { focusContent: true };
};

let openAddonsUrl = url => {
  return () => {
    let window = lazy.BrowserWindowTracker.getTopWindow();
    window.BrowserOpenAddonsMgr(url);
  };
};

// There are various actions that will either fail or do not
// make sense to use unless the user is viewing web content,
// Screenshots for example cannot be taken on about: pages.
// We may want to show these as disabled buttons, that may
// aid discovery but may also confuse users.
let currentPageIsWebContentFilter = () =>
  !currentBrowser()?.currentURI.spec.startsWith("about:");
let currentBrowser = () =>
  lazy.BrowserWindowTracker.getTopWindow()?.gBrowser.selectedBrowser;
let currentTab = () =>
  lazy.BrowserWindowTracker.getTopWindow()?.gBrowser.selectedTab;

let inspectorIsAvailable = () => {
  // The inspect action is available if:
  // 1. DevTools is enabled.
  // 2. The user can be considered as a DevTools user.
  // 3. The url is not about:devtools-toolbox.
  // 4. The inspector is not opened yet on the page.
  return (
    lazy.DevToolsShim.isEnabled() &&
    lazy.DevToolsShim.isDevToolsUser() &&
    !currentBrowser()?.currentURI.spec.startsWith("about:devtools-toolbox") &&
    !lazy.DevToolsShim.hasToolboxForTab(currentTab())
  );
};

let viewsourceIsAvailable = () => {
  return currentBrowser()?.currentURI.scheme !== "view-source";
};

XPCOMUtils.defineLazyGetter(lazy, "gFluentStrings", function() {
  return new Localization(["branding/brand.ftl", "browser/browser.ftl"], true);
});

const DEFAULT_ACTIONS = {
  addons: {
    l10nCommands: ["quickactions-cmd-addons2", "quickactions-addons"],
    icon: "chrome://mozapps/skin/extensions/category-extensions.svg",
    label: "quickactions-addons",
    onPick: openAddonsUrl("addons://discover/"),
  },
  bookmarks: {
    l10nCommands: ["quickactions-cmd-bookmarks", "quickactions-bookmarks"],
    icon: "chrome://browser/skin/bookmark.svg",
    label: "quickactions-bookmarks",
    onPick: () => {
      lazy.BrowserWindowTracker.getTopWindow().top.PlacesCommandHook.showPlacesOrganizer(
        "BookmarksToolbar"
      );
    },
  },
  clear: {
    l10nCommands: [
      "quickactions-cmd-clearhistory",
      "quickactions-clearhistory",
    ],
    label: "quickactions-clearhistory",
    onPick: openUrlFun(
      `${BASE_URL}delete-browsing-search-download-history-firefox`
    ),
  },
  downloads: {
    l10nCommands: ["quickactions-cmd-downloads", "quickactions-downloads"],
    icon: "chrome://browser/skin/downloads/downloads.svg",
    label: "quickactions-downloads",
    onPick: openUrlFun("about:downloads"),
  },
  extensions: {
    l10nCommands: ["quickactions-cmd-extensions", "quickactions-extensions"],
    icon: "chrome://mozapps/skin/extensions/category-extensions.svg",
    label: "quickactions-extensions",
    onPick: openAddonsUrl("addons://list/extension"),
  },
  inspect: {
    l10nCommands: ["quickactions-cmd-inspector", "quickactions-inspector"],
    icon: "chrome://devtools/skin/images/tool-inspector.svg",
    label: "quickactions-inspector",
    isActive: inspectorIsAvailable,
    onPick: openInspector,
  },
  logins: {
    l10nCommands: ["quickactions-cmd-logins", "quickactions-logins"],
    label: "quickactions-logins",
    onPick: openUrlFun("about:logins"),
  },
  plugins: {
    l10nCommands: ["quickactions-cmd-plugins", "quickactions-plugins"],
    icon: "chrome://mozapps/skin/extensions/category-extensions.svg",
    label: "quickactions-plugins",
    onPick: openAddonsUrl("addons://list/plugin"),
  },
  print: {
    l10nCommands: ["quickactions-cmd-print", "quickactions-print"],
    label: "quickactions-print",
    onPick: () => {
      lazy.BrowserWindowTracker.getTopWindow()
        .document.getElementById("cmd_print")
        .doCommand();
    },
  },
  private: {
    l10nCommands: ["quickactions-cmd-private", "quickactions-private"],
    label: "quickactions-private",
    icon: "chrome://global/skin/icons/indicator-private-browsing.svg",
    onPick: () => {
      lazy.BrowserWindowTracker.getTopWindow().OpenBrowserWindow({
        private: true,
      });
    },
  },
  refresh: {
    l10nCommands: ["quickactions-cmd-refresh", "quickactions-refresh"],
    label: "quickactions-refresh",
    onPick: openUrlFun(`${BASE_URL}refresh-firefox-reset-add-ons-and-settings`),
  },
  restart: {
    l10nCommands: ["quickactions-cmd-restart", "quickactions-restart"],
    label: "quickactions-restart",
    onPick: restartBrowser,
  },
  screenshot: {
    l10nCommands: ["quickactions-cmd-screenshot", "quickactions-screenshot2"],
    label: "quickactions-screenshot2",
    isActive: currentPageIsWebContentFilter,
    onPick: () => {
      lazy.BrowserWindowTracker.getTopWindow()
        .document.getElementById("Browser:Screenshot")
        .doCommand();
      return { focusContent: true };
    },
  },
  settings: {
    l10nCommands: ["quickactions-cmd-settings", "quickactions-settings"],
    label: "quickactions-settings",
    onPick: openUrlFun("about:preferences"),
  },
  themes: {
    l10nCommands: ["quickactions-cmd-themes", "quickactions-themes"],
    icon: "chrome://mozapps/skin/extensions/category-extensions.svg",
    label: "quickactions-themes",
    onPick: openAddonsUrl("addons://list/theme"),
  },
  update: {
    l10nCommands: ["quickactions-cmd-update", "quickactions-update"],
    label: "quickactions-update",
    onPick: openUrlFun(`${BASE_URL}update-firefox-latest-release`),
  },
  viewsource: {
    l10nCommands: ["quickactions-cmd-viewsource", "quickactions-viewsource"],
    icon: "chrome://global/skin/icons/settings.svg",
    label: "quickactions-viewsource",
    isActive: viewsourceIsAvailable,
    onPick: () => openUrl("view-source:" + currentBrowser().currentURI.spec),
  },
};

function openInspector() {
  lazy.DevToolsShim.showToolboxForTab(
    lazy.BrowserWindowTracker.getTopWindow().gBrowser.selectedTab,
    { toolId: "inspector" }
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
export class QuickActionsLoaderDefault {
  static async load() {
    for (const key in DEFAULT_ACTIONS) {
      let actionData = DEFAULT_ACTIONS[key];
      let messages = await lazy.gFluentStrings.formatMessages(
        actionData.l10nCommands.map(id => ({ id }))
      );
      actionData.commands = messages
        .map(({ value }) => value.split(",").map(x => x.trim().toLowerCase()))
        .flat();
      lazy.UrlbarProviderQuickActions.addAction(key, actionData);
    }
  }
}
