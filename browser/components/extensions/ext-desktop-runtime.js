"use strict";

/* eslint-disable mozilla/balanced-listeners */
extensions.on("uninstall", (msg, extension) => {
  if (extension.uninstallURL) {
    let browser = Services.wm.getMostRecentWindow("navigator:browser").gBrowser;
    browser.addTab(extension.uninstallURL, {relatedToCurrent: true});
  }
});

