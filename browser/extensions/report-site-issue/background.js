/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals browser */

const desktopReporterConfig = {
  src: "desktop-reporter",
  utm_campaign: "report-site-issue-button",
  utm_source: "desktop-reporter",
};

const androidReporterConfig = {
  src: "android-components-reporter",
  utm_campaign: "report-site-issue-button",
  utm_source: "android-components-reporter",
};

let reporterConfig = desktopReporterConfig;

(async () => {
  const permissions = ["nativeMessaging"];
  if (await browser.permissions.contains({ permissions })) {
    reporterConfig = androidReporterConfig;

    const port = browser.runtime.connectNative("mozacWebcompatReporter");
    port.onMessage.addListener(message => {
      if ("productName" in message) {
        reporterConfig.productName = message.productName;

        // For now, setting the productName is the only use for this port, and that's only happening
        // once after startup, so let's disconnect the port when we're done.
        port.disconnect();
      }
    });
  }
})();

async function loadTab(url) {
  const newTab = await browser.tabs.create({ url });
  return new Promise(resolve => {
    const listener = (tabId, changeInfo, tab) => {
      if (
        tabId == newTab.id &&
        tab.url !== "about:blank" &&
        changeInfo.status == "complete"
      ) {
        browser.tabs.onUpdated.removeListener(listener);
        resolve(newTab);
      }
    };
    browser.tabs.onUpdated.addListener(listener);
  });
}

async function captureAndSendReport(tab) {
  const { id, url } = tab;
  try {
    const { endpointUrl, webcompatInfo } =
      await browser.tabExtras.getWebcompatInfo(id);
    const dataToSend = {
      endpointUrl,
      reportUrl: url,
      reporterConfig,
      webcompatInfo,
    };
    const newTab = await loadTab(endpointUrl);
    browser.tabExtras.sendWebcompatInfo(newTab.id, dataToSend);
  } catch (err) {
    console.error("WebCompat Reporter: unexpected error", err);
  }
}

if ("helpMenu" in browser) {
  // desktop
  browser.helpMenu.onHelpMenuCommand.addListener(captureAndSendReport);
} else {
  // Android
  browser.pageAction.onClicked.addListener(captureAndSendReport);
}
