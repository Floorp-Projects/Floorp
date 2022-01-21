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

const FRAMEWORK_KEYS = ["hasFastClick", "hasMobify", "hasMarfeel"];

browser.helpMenu.onHelpMenuCommand.addListener(tab => {
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

function hasFastClickPageScript() {
  const win = window.wrappedJSObject;

  if (win.FastClick) {
    return true;
  }

  for (const property in win) {
    try {
      const proto = win[property].prototype;
      if (proto && proto.needsClick) {
        return true;
      }
    } catch (_) {}
  }

  return false;
}

function hasMobifyPageScript() {
  const win = window.wrappedJSObject;
  return !!(win.Mobify && win.Mobify.Tag);
}

function hasMarfeelPageScript() {
  const win = window.wrappedJSObject;
  return !!win.marfeel;
}

function checkForFrameworks(tabId) {
  return browser.tabs
    .executeScript(tabId, {
      code: `
      (function() {
        ${hasFastClickPageScript};
        ${hasMobifyPageScript};
        ${hasMarfeelPageScript};

        const result = {
          hasFastClick: hasFastClickPageScript(),
          hasMobify: hasMobifyPageScript(),
          hasMarfeel: hasMarfeelPageScript(),
        }

        return result;
      })();
    `,
    })
    .then(([results]) => results)
    .catch(() => false);
}

function getWebCompatInfoForTab(tab) {
  const { id, url } = tab;
  return Promise.all([
    browser.browserInfo.getBlockList(),
    browser.browserInfo.getBuildID(),
    browser.browserInfo.getGraphicsPrefs(),
    browser.browserInfo.getUpdateChannel(),
    browser.browserInfo.hasTouchScreen(),
    browser.tabExtras.getWebcompatInfo(id),
    checkForFrameworks(id),
    browser.tabs.captureTab(id, Config.screenshotFormat).catch(e => {
      console.error("WebCompat Reporter: getting a screenshot failed", e);
      return Promise.resolve(undefined);
    }),
  ]).then(
    ([
      blockList,
      buildID,
      graphicsPrefs,
      channel,
      hasTouchScreen,
      frameInfo,
      frameworks,
      screenshot,
    ]) => {
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
          frameworks,
          hasTouchScreen,
          "mixed active content blocked":
            frameInfo.hasMixedActiveContentBlocked,
          "mixed passive content blocked":
            frameInfo.hasMixedDisplayContentBlocked,
          "tracking content blocked": frameInfo.hasTrackingContentBlocked
            ? `true (${blockList})`
            : "false",
        }),
        screenshot,
        url,
      });
    }
  );
}

function stripNonASCIIChars(str) {
  // eslint-disable-next-line no-control-regex
  return str.replace(/[^\x00-\x7F]/g, "");
}

async function openWebCompatTab(compatInfo) {
  const url = new URL(Config.newIssueEndpoint);
  const { details } = compatInfo;
  const params = {
    url: `${compatInfo.url}`,
    utm_source: "desktop-reporter",
    utm_campaign: "report-site-issue-button",
    src: "desktop-reporter",
    details,
    extra_labels: [],
  };

  for (let framework of FRAMEWORK_KEYS) {
    if (details.frameworks[framework]) {
      params.details[framework] = true;
      params.extra_labels.push(
        framework.replace(/^has/, "type-").toLowerCase()
      );
    }
  }
  delete details.frameworks;

  if (details["gfx.webrender.all"] || details["gfx.webrender.enabled"]) {
    params.extra_labels.push("type-webrender-enabled");
  }
  if (compatInfo.hasTrackingContentBlocked) {
    params.extra_labels.push(
      `type-tracking-protection-${compatInfo.blockList}`
    );
  }

  const json = stripNonASCIIChars(JSON.stringify(params));
  const tab = await browser.tabs.create({ url: url.href });
  await browser.tabs.executeScript(tab.id, {
    runAt: "document_end",
    code: `(function() {
      async function postMessageData(dataURI, metadata) {
        const res = await fetch(dataURI);
        const blob = await res.blob();
        const data = {
           screenshot: blob,
           message: metadata
        };
        postMessage(data, "${url.origin}");
      }
      postMessageData("${compatInfo.screenshot}", ${json});
    })()`,
  });
}
