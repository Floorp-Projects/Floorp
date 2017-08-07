"use strict";

// The ext-* files are imported into the same scopes.
/* import-globals-from ext-utils.js */

global.EventEmitter = ExtensionUtils.EventEmitter;

// This function is pretty tightly tied to Extension.jsm.
// Its job is to fill in the |tab| property of the sender.
const getSender = (extension, target, sender) => {
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
};

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
