"use strict";

/* eslint-disable mozilla/balanced-listeners */
extensions.on("uninstall", (msg, extension) => {
  if (extension.uninstallURL) {
    let browser = WindowManager.topWindow.gBrowser;
    browser.addTab(extension.uninstallURL, {relatedToCurrent: true});
  }
});

global.openOptionsPage = (extension) => {
  let window = WindowManager.topWindow;
  if (!window) {
    return Promise.reject({message: "No browser window available"});
  }

  if (extension.manifest.options_ui.open_in_tab) {
    window.switchToTabHavingURI(extension.manifest.options_ui.page, true);
    return Promise.resolve();
  }

  let viewId = `addons://detail/${encodeURIComponent(extension.id)}/preferences`;

  return window.BrowserOpenAddonsMgr(viewId);
};

