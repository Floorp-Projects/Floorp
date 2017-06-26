"use strict";

// The ext-* files are imported into the same scopes.
/* import-globals-from ext-utils.js */

XPCOMUtils.defineLazyModuleGetter(global, "EventEmitter",
                                  "resource://gre/modules/EventEmitter.jsm");

// This function is pretty tightly tied to Extension.jsm.
// Its job is to fill in the |tab| property of the sender.
function getSender(extension, target, sender) {
  let tabId;
  if ("tabId" in sender) {
    // The message came from a privileged extension page running in a tab. In
    // that case, it should include a tabId property (which is filled in by the
    // page-open listener below).
    tabId = sender.tabId;
    delete sender.tabId;
  } else if (target instanceof Ci.nsIDOMXULElement) {
    tabId = tabTracker.getBrowserData(target).tabId;
  }

  if (tabId) {
    let tab = extension.tabManager.get(tabId, null);
    if (tab) {
      sender.tab = tab.convert();
    }
  }
}

// Used by Extension.jsm
global.tabGetSender = getSender;

/* eslint-disable mozilla/balanced-listeners */
extensions.on("uninstall", (msg, extension) => {
  if (extension.uninstallURL) {
    let browser = windowTracker.topWindow.gBrowser;
    browser.addTab(extension.uninstallURL, {relatedToCurrent: true});
  }
});

extensions.on("page-shutdown", (type, context) => {
  if (context.viewType == "tab") {
    if (context.extension.id !== context.xulBrowser.contentPrincipal.addonId) {
      // Only close extension tabs.
      // This check prevents about:addons from closing when it contains a
      // WebExtension as an embedded inline options page.
      return;
    }
    let {gBrowser} = context.xulBrowser.ownerGlobal;
    if (gBrowser) {
      let nativeTab = gBrowser.getTabForBrowser(context.xulBrowser);
      if (nativeTab) {
        gBrowser.removeTab(nativeTab);
      }
    }
  }
});
/* eslint-enable mozilla/balanced-listeners */

global.openOptionsPage = (extension) => {
  let window = windowTracker.topWindow;
  if (!window) {
    return Promise.reject({message: "No browser window available"});
  }

  if (extension.manifest.options_ui.open_in_tab) {
    window.switchToTabHavingURI(extension.manifest.options_ui.page, true, {
      triggeringPrincipal: extension.principal,
    });
    return Promise.resolve();
  }

  let viewId = `addons://detail/${encodeURIComponent(extension.id)}/preferences`;

  return window.BrowserOpenAddonsMgr(viewId);
};

extensions.registerModules({
  bookmarks: {
    url: "chrome://browser/content/ext-bookmarks.js",
    schema: "chrome://browser/content/schemas/bookmarks.json",
    scopes: ["addon_parent"],
    paths: [
      ["bookmarks"],
    ],
  },
  browserAction: {
    url: "chrome://browser/content/ext-browserAction.js",
    schema: "chrome://browser/content/schemas/browser_action.json",
    scopes: ["addon_parent"],
    manifest: ["browser_action"],
    paths: [
      ["browserAction"],
    ],
  },
  browsingData: {
    url: "chrome://browser/content/ext-browsingData.js",
    schema: "chrome://browser/content/schemas/browsing_data.json",
    scopes: ["addon_parent"],
    paths: [
      ["browsingData"],
    ],
  },
  chrome_settings_overrides: {
    url: "chrome://browser/content/ext-chrome-settings-overrides.js",
    scopes: [],
    schema: "chrome://browser/content/schemas/chrome_settings_overrides.json",
    manifest: ["chrome_settings_overrides"],
  },
  commands: {
    url: "chrome://browser/content/ext-commands.js",
    schema: "chrome://browser/content/schemas/commands.json",
    scopes: ["addon_parent"],
    manifest: ["commands"],
    paths: [
      ["commands"],
    ],
  },
  devtools: {
    url: "chrome://browser/content/ext-devtools.js",
    schema: "chrome://browser/content/schemas/devtools.json",
    scopes: ["devtools_parent"],
    manifest: ["devtools_page"],
    paths: [
      ["devtools"],
    ],
  },
  devtools_inspectedWindow: {
    url: "chrome://browser/content/ext-devtools-inspectedWindow.js",
    schema: "chrome://browser/content/schemas/devtools_inspected_window.json",
    scopes: ["devtools_parent"],
    paths: [
      ["devtools", "inspectedWindow"],
    ],
  },
  devtools_network: {
    url: "chrome://browser/content/ext-devtools-network.js",
    schema: "chrome://browser/content/schemas/devtools_network.json",
    scopes: ["devtools_parent"],
    paths: [
      ["devtools", "network"],
    ],
  },
  devtools_panels: {
    url: "chrome://browser/content/ext-devtools-panels.js",
    schema: "chrome://browser/content/schemas/devtools_panels.json",
    scopes: ["devtools_parent"],
    paths: [
      ["devtools", "panels"],
    ],
  },
  history: {
    url: "chrome://browser/content/ext-history.js",
    schema: "chrome://browser/content/schemas/history.json",
    scopes: ["addon_parent"],
    paths: [
      ["history"],
    ],
  },
  // This module supports the "menus" and "contextMenus" namespaces,
  // and because of permissions, the module name must differ from both.
  menusInternal: {
    url: "chrome://browser/content/ext-menus.js",
    schema: "chrome://browser/content/schemas/menus.json",
    scopes: ["addon_parent"],
    paths: [
      ["menusInternal"],
    ],
  },
  omnibox: {
    url: "chrome://browser/content/ext-omnibox.js",
    schema: "chrome://browser/content/schemas/omnibox.json",
    scopes: ["addon_parent"],
    manifest: ["omnibox"],
    paths: [
      ["omnibox"],
    ],
  },
  pageAction: {
    url: "chrome://browser/content/ext-pageAction.js",
    schema: "chrome://browser/content/schemas/page_action.json",
    scopes: ["addon_parent"],
    manifest: ["page_action"],
    paths: [
      ["pageAction"],
    ],
  },
  geckoProfiler: {
    url: "chrome://browser/content/ext-geckoProfiler.js",
    schema: "chrome://browser/content/schemas/geckoProfiler.json",
    scopes: ["addon_parent"],
    paths: [
      ["geckoProfiler"],
    ],
  },
  sessions: {
    url: "chrome://browser/content/ext-sessions.js",
    schema: "chrome://browser/content/schemas/sessions.json",
    scopes: ["addon_parent"],
    paths: [
      ["sessions"],
    ],
  },
  sidebarAction: {
    url: "chrome://browser/content/ext-sidebarAction.js",
    schema: "chrome://browser/content/schemas/sidebar_action.json",
    scopes: ["addon_parent"],
    manifest: ["sidebar_action"],
    paths: [
      ["sidebarAction"],
    ],
  },
  tabs: {
    url: "chrome://browser/content/ext-tabs.js",
    schema: "chrome://browser/content/schemas/tabs.json",
    scopes: ["addon_parent"],
    paths: [
      ["tabs"],
    ],
  },
  urlOverrides: {
    url: "chrome://browser/content/ext-url-overrides.js",
    schema: "chrome://browser/content/schemas/url_overrides.json",
    scopes: ["addon_parent"],
    manifest: ["chrome_url_overrides"],
    paths: [
      ["urlOverrides"],
    ],
  },
  windows: {
    url: "chrome://browser/content/ext-windows.js",
    schema: "chrome://browser/content/schemas/windows.json",
    scopes: ["addon_parent"],
    paths: [
      ["windows"],
    ],
  },
});
