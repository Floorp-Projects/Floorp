/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals browser */

const Config = {
  newIssueEndpoint: "https://webcompat.com/issues/new",
  newIssueEndpointPref: "newIssueEndpoint",
  screenshotFormat: {
    format: "jpeg",
    quality: 75,
  },
};

browser.pageActionExtras.setLabelForHistogram("webcompat");

browser.pageAction.onClicked.addListener(tab => {
  return getWebCompatInfoForTab(tab).then(
    info => {
      return openWebCompatTab(info);
    },
    err => {
      console.error("WebCompat Reporter: unexpected error", err);
    }
  );
});

browser.aboutConfigPrefs.onEndpointPrefChange.addListener(checkEndpointPref);

checkEndpointPref();

async function checkEndpointPref() {
  const value = await browser.aboutConfigPrefs.getEndpointPref();
  if (value === undefined) {
    browser.aboutConfigPrefs.setEndpointPref(Config.newIssueEndpoint);
  } else {
    Config.newIssueEndpoint = value;
  }
}

function getWebCompatInfoForTab(tab) {
  const {id, url} = tab;
  return Promise.all([
    browser.browserInfo.getBlockList(),
    browser.browserInfo.getBuildID(),
    browser.browserInfo.getGraphicsPrefs(),
    browser.browserInfo.getUpdateChannel(),
    browser.browserInfo.hasTouchScreen(),
    browser.tabExtras.getWebcompatInfo(id),
    browser.tabs.captureTab(id, Config.screenshotFormat).catch(e => {
      console.error("WebCompat Reporter: getting a screenshot failed", e);
      return Promise.resolve(undefined);
    }),
  ]).then(([blockList, buildID, graphicsPrefs, channel, hasTouchScreen,
            frameInfo, screenshot]) => {
    if (channel !== "linux") {
      delete graphicsPrefs["layers.acceleration.force-enabled"];
    }

    const consoleLog = frameInfo.log;
    delete frameInfo.log;

    return Object.assign(frameInfo, {
      tabId: id,
      blockList,
      details: Object.assign(graphicsPrefs, {
        buildID,
        channel,
        consoleLog,
        hasTouchScreen,
        "mixed active content blocked": frameInfo.hasMixedActiveContentBlocked,
        "mixed passive content blocked": frameInfo.hasMixedDisplayContentBlocked,
        "tracking content blocked": frameInfo.hasTrackingContentBlocked ?
                                    `true (${blockList})` : "false",
      }),
      screenshot,
      url,
    });
  });
}

function stripNonASCIIChars(str) {
  // eslint-disable-next-line no-control-regex
  return str.replace(/[^\x00-\x7F]/g, "");
}

browser.l10n.getMessage("wc-reporter.label2").then(
  browser.pageActionExtras.setDefaultTitle, () => {});

browser.l10n.getMessage("wc-reporter.tooltip").then(
  browser.pageActionExtras.setTooltipText, () => {});

async function openWebCompatTab(compatInfo) {
  const url = new URL(Config.newIssueEndpoint);
  const {details} = compatInfo;
  const params = {
    url: `${compatInfo.url}`,
    src: "desktop-reporter",
    details,
    label: [],
  };
  if (details["gfx.webrender.all"] || details["gfx.webrender.enabled"]) {
    params.label.push("type-webrender-enabled");
  }
  if (compatInfo.hasTrackingContentBlocked) {
    params.label.push(`type-tracking-protection-${compatInfo.blockList}`);
  }

  const tab = await browser.tabs.create({url: "about:blank"});
  const json = stripNonASCIIChars(JSON.stringify(params));
  await browser.tabExtras.loadURIWithPostData(tab.id, url.href, json,
                                              "application/json");
  await browser.tabs.executeScript(tab.id, {
    runAt: "document_end",
    code: `(function() {
      async function sendScreenshot(dataURI) {
        const res = await fetch(dataURI);
        const blob = await res.blob();
        postMessage(blob, "${url.origin}");
      }
      sendScreenshot("${compatInfo.screenshot}");
    })()`,
  });
}
