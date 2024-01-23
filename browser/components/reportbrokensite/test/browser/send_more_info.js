/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Helper methods for testing the "send more info" link
 * of the Report Broken Site feature.
 */

/* import-globals-from head.js */
/* import-globals-from send.js */

"use strict";

Services.scriptloader.loadSubScript(
  getRootDirectory(gTestPath) + "send.js",
  this
);

async function reformatExpectedWebCompatInfo(tab, overrides) {
  const gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo);
  const snapshot = await Troubleshoot.snapshot();
  const expected = await getExpectedWebCompatInfo(tab, snapshot, true);
  const { browserInfo, tabInfo } = expected;
  const { app, graphics, prefs, security } = browserInfo;
  const {
    applicationName,
    defaultUseragentString,
    fissionEnabled,
    osArchitecture,
    osName,
    osVersion,
    updateChannel,
    version,
  } = app;
  const { devicePixelRatio, hasTouchScreen } = graphics;
  const { antitracking, languages, useragentString } = tabInfo;

  const atOverrides = overrides.antitracking;
  const blockList = atOverrides?.blockList ?? antitracking.blockList;
  const hasMixedActiveContentBlocked =
    atOverrides?.hasMixedActiveContentBlocked ??
    antitracking.hasMixedActiveContentBlocked;
  const hasMixedDisplayContentBlocked =
    atOverrides?.hasMixedDisplayContentBlocked ??
    antitracking.hasMixedDisplayContentBlocked;
  const hasTrackingContentBlocked =
    atOverrides?.hasTrackingContentBlocked ??
    antitracking.hasTrackingContentBlocked;
  const isPrivateBrowsing =
    atOverrides?.isPrivateBrowsing ?? antitracking.isPrivateBrowsing;

  const extra_labels = [];
  const frameworks = overrides.frameworks ?? {
    fastclick: false,
    mobify: false,
    marfeel: false,
  };

  // ignore the console log unless explicily testing for it.
  const consoleLog = overrides.consoleLog ?? (() => true);

  const finalPrefs = {};
  for (const [key, pref] of Object.entries({
    cookieBehavior: "network.cookie.cookieBehavior",
    forcedAcceleratedLayers: "layers.acceleration.force-enabled",
    globalPrivacyControlEnabled: "privacy.globalprivacycontrol.enabled",
    installtriggerEnabled: "extensions.InstallTrigger.enabled",
    opaqueResponseBlocking: "browser.opaqueResponseBlocking",
    resistFingerprintingEnabled: "privacy.resistFingerprinting",
    softwareWebrender: "gfx.webrender.software",
  })) {
    if (key in prefs) {
      finalPrefs[pref] = prefs[key];
    }
  }

  const reformatted = {
    blockList,
    details: {
      additionalData: {
        applicationName,
        blockList,
        devicePixelRatio: parseInt(devicePixelRatio),
        finalUserAgent: useragentString,
        fissionEnabled,
        gfxData: {
          devices(actual) {
            const devices = getExpectedGraphicsDevices(snapshot);
            return compareGraphicsDevices(devices, actual);
          },
          drivers(actual) {
            const drvs = getExpectedGraphicsDrivers(snapshot);
            return compareGraphicsDrivers(drvs, actual);
          },
          features(actual) {
            const features = getExpectedGraphicsFeatures(snapshot);
            return areObjectsEqual(actual, features);
          },
          hasTouchScreen,
          monitors(actual) {
            // We don't care about monitor data on Android right now.
            if (AppConstants.platform === "android") {
              return actual == undefined;
            }
            return areObjectsEqual(actual, gfxInfo.getMonitors());
          },
        },
        hasMixedActiveContentBlocked,
        hasMixedDisplayContentBlocked,
        hasTrackingContentBlocked,
        isPB: isPrivateBrowsing,
        languages,
        osArchitecture,
        osName,
        osVersion,
        prefs: finalPrefs,
        updateChannel,
        userAgent: defaultUseragentString,
        version,
      },
      blockList,
      consoleLog,
      frameworks,
      hasTouchScreen,
      "gfx.webrender.software": prefs.softwareWebrender,
      "mixed active content blocked": hasMixedActiveContentBlocked,
      "mixed passive content blocked": hasMixedDisplayContentBlocked,
      "tracking content blocked": hasTrackingContentBlocked
        ? `true (${blockList})`
        : "false",
    },
    extra_labels,
    src: "desktop-reporter",
    utm_campaign: "report-broken-site",
    utm_source: "desktop-reporter",
  };

  // We only care about this pref on Linux right now on webcompat.com.
  if (AppConstants.platform != "linux") {
    delete finalPrefs["layers.acceleration.force-enabled"];
  } else {
    reformatted.details["layers.acceleration.force-enabled"] =
      finalPrefs["layers.acceleration.force-enabled"];
  }

  // Only bother adding the security key if it has any data
  if (Object.values(security).filter(e => e).length) {
    reformatted.details.additionalData.sec = security;
  }

  const expectedCodecs = snapshot.media.codecSupportInfo
    .replaceAll(" NONE", "")
    .split("\n")
    .sort()
    .join("\n");
  if (expectedCodecs) {
    reformatted.details.additionalData.gfxData.codecSupport = rawActual => {
      const actual = Object.entries(rawActual)
        .map(([name, { hardware, software }]) =>
          `${name} ${software ? "SW" : ""} ${hardware ? "HW" : ""}`.trim()
        )
        .sort()
        .join("\n");
      return areObjectsEqual(actual, expectedCodecs);
    };
  }

  if (blockList != "basic") {
    extra_labels.push(`type-tracking-protection-${blockList}`);
  }

  if (overrides.expectNoTabDetails) {
    delete reformatted.details.frameworks;
    delete reformatted.details.consoleLog;
    delete reformatted.details["mixed active content blocked"];
    delete reformatted.details["mixed passive content blocked"];
    delete reformatted.details["tracking content blocked"];
  } else {
    const { fastclick, mobify, marfeel } = frameworks;
    if (fastclick) {
      extra_labels.push("type-fastclick");
      reformatted.details.fastclick = true;
    }
    if (mobify) {
      extra_labels.push("type-mobify");
      reformatted.details.mobify = true;
    }
    if (marfeel) {
      extra_labels.push("type-marfeel");
      reformatted.details.marfeel = true;
    }
  }

  return reformatted;
}

async function testSendMoreInfo(tab, menu, expectedOverrides = {}) {
  const url = expectedOverrides.url ?? menu.win.gBrowser.currentURI.spec;
  const description = expectedOverrides.description ?? "";

  let rbs = await menu.openAndPrefillReportBrokenSite(url, description);

  const receivedData = await rbs.clickSendMoreInfo();
  const { message } = receivedData;

  const expected = await reformatExpectedWebCompatInfo(tab, expectedOverrides);
  expected.url = url;
  expected.description = description;

  ok(areObjectsEqual(message, expected), "ping matches expectations");

  // re-opening the panel, the url and description should be reset
  rbs = await menu.openReportBrokenSite();
  rbs.isMainViewResetToCurrentTab();
  rbs.close();
}
