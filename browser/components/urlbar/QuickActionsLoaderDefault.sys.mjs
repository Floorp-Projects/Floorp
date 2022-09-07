/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarProviderQuickActions:
    "resource:///modules/UrlbarProviderQuickActions.sys.mjs",
});

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

XPCOMUtils.defineLazyModuleGetters(lazy, {
  AppUpdater: "resource:///modules/AppUpdater.jsm",
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  ClientEnvironment: "resource://normandy/lib/ClientEnvironment.jsm",
  DevToolsShim: "chrome://devtools-startup/content/DevToolsShim.jsm",
  ResetProfile: "resource://gre/modules/ResetProfile.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "BrowserUpdater", () => {
  return AppConstants.MOZ_UPDATER ? new lazy.AppUpdater() : null;
});

let openUrlFun = url => () => openUrl(url);
let openUrl = url => {
  let window = lazy.BrowserWindowTracker.getTopWindow();

  if (url.startsWith("about:")) {
    window.switchToTabHavingURI(Services.io.newURI(url), true, {
      ignoreFragment: "whenComparing",
    });
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

XPCOMUtils.defineLazyGetter(lazy, "gFluentStrings", function() {
  return new Localization(["branding/brand.ftl", "browser/browser.ftl"], true);
});

const DEFAULT_ACTIONS = {
  addons: {
    l10nCommands: ["quickactions-cmd-addons2", "quickactions-addons"],
    icon: "chrome://mozapps/skin/extensions/category-extensions.svg",
    label: "quickactions-addons",
    onPick: () => openUrl("about:addons"),
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
    onPick: () => {
      lazy.BrowserWindowTracker.getTopWindow()
        .document.getElementById("Tools:Sanitize")
        .doCommand();
    },
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
    isVisible: () =>
      lazy.DevToolsShim.isEnabled() || lazy.DevToolsShim.isDevToolsUser(),
    isActive: () => {
      // The inspect action is available if:
      // 1. DevTools is enabled.
      // 2. The user can be considered as a DevTools user.
      // 3. The url is not about:devtools-toolbox.
      // 4. The inspector is not opened yet on the page.
      return (
        lazy.DevToolsShim.isEnabled() &&
        lazy.DevToolsShim.isDevToolsUser() &&
        !currentBrowser()?.currentURI.spec.startsWith(
          "about:devtools-toolbox"
        ) &&
        !lazy.DevToolsShim.hasToolboxForTab(currentTab())
      );
    },
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
    onPick: () => {
      lazy.ResetProfile.openConfirmationDialog(
        lazy.BrowserWindowTracker.getTopWindow()
      );
    },
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
    isActive: () => !!lazy.BrowserUpdater?.isReadyForRestart,
    onPick: restartBrowser,
  },
  viewsource: {
    l10nCommands: ["quickactions-cmd-viewsource", "quickactions-viewsource"],
    icon: "chrome://global/skin/icons/settings.svg",
    label: "quickactions-viewsource",
    isActive: () => currentBrowser()?.currentURI.scheme !== "view-source",
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

function random(seed) {
  let x = Math.sin(seed) * 10000;
  return x - Math.floor(x);
}

function shuffle(array, seed) {
  for (let i = array.length - 1; i > 0; i--) {
    const j = Math.floor(random(seed) * (i + 1));
    [array[i], array[j]] = [array[j], array[i]];
  }
}

/**
 * Loads the default QuickActions.
 */
export class QuickActionsLoaderDefault {
  static async load() {
    let keys = Object.keys(DEFAULT_ACTIONS);
    if (lazy.UrlbarPrefs.get("quickactions.randomOrderActions")) {
      // We insert the actions in a random order which means they will be returned
      // in a random but consistent order (the order of results for "view" and "views"
      // should be the same).
      // We use the Nimbus randomizationId as the seed as the order should not change
      // for the user between restarts, it should be random between users but a user should
      // see actions the same order.
      let seed = [...lazy.ClientEnvironment.randomizationId]
        .map(x => x.charCodeAt(0))
        .reduce((sum, a) => sum + a, 0);
      shuffle(keys, seed);
    }
    for (const key of keys) {
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
