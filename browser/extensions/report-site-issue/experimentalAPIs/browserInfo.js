/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global AppConstants, ExtensionAPI, Services */

function isTelemetryEnabled() {
  return Services.prefs.getBoolPref(
    "datareporting.healthreport.uploadEnabled",
    false
  );
}

function getSysinfoProperty(propertyName, defaultValue) {
  try {
    return Services.sysinfo.getProperty(propertyName);
  } catch (e) {}

  return defaultValue;
}

function getUserAgent() {
  const { userAgent } = Cc[
    "@mozilla.org/network/protocol;1?name=http"
  ].getService(Ci.nsIHttpProtocolHandler);
  return userAgent;
}

function getGfxData() {
  const gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo);
  const data = {};

  try {
    const {
      compositor,
      hwCompositing,
      openglCompositing,
      wrCompositor,
      wrSoftware,
    } = gfxInfo.getFeatures();

    data.features = {
      compositor,
      hwCompositing,
      openglCompositing,
      wrCompositor,
      wrSoftware,
    };
  } catch (e) {}

  try {
    if (AppConstants.platform !== "android") {
      data.monitors = gfxInfo.getMonitors();
    }
  } catch (e) {}

  return data;
}

function limitStringToLength(str, maxLength) {
  if (typeof str !== "string") {
    return null;
  }
  return str.substring(0, maxLength);
}

function getSecurityAppData() {
  const maxStringLength = 256;

  const keys = [
    ["registeredAntiVirus", "antivirus"],
    ["registeredAntiSpyware", "antispyware"],
    ["registeredFirewall", "firewall"],
  ];

  let result = {};

  for (let [inKey, outKey] of keys) {
    let prop = getSysinfoProperty(inKey, null);
    if (prop) {
      prop = limitStringToLength(prop, maxStringLength).split(";");
    }

    result[outKey] = prop;
  }

  return result;
}

function getAdditionalPrefs() {
  const prefs = {};
  for (const [name, dflt] of Object.entries({
    "browser.opaqueResponseBlocking": false,
    "extensions.InstallTrigger.enabled": false,
    "gfx.canvas.accelerated.force-enabled": false,
    "gfx.webrender.compositor.force-enabled": false,
    "privacy.resistFingerprinting": false,
  })) {
    prefs[name] = Services.prefs.getBoolPref(name, dflt);
  }
  const cookieBehavior = "network.cookie.cookieBehavior";
  prefs[cookieBehavior] = Services.prefs.getIntPref(cookieBehavior);

  return prefs;
}

function getMemoryMB() {
  let memoryMB = getSysinfoProperty("memsize", null);
  if (memoryMB) {
    memoryMB = Math.round(memoryMB / 1024 / 1024);
  }

  return memoryMB;
}

this.browserInfo = class extends ExtensionAPI {
  getAPI(context) {
    return {
      browserInfo: {
        async getGraphicsPrefs() {
          const prefs = {};
          for (const [name, dflt] of Object.entries({
            "layers.acceleration.force-enabled": false,
            "gfx.webrender.all": false,
            "gfx.webrender.blob-images": true,
            "gfx.webrender.enabled": false,
          })) {
            prefs[name] = Services.prefs.getBoolPref(name, dflt);
          }
          return prefs;
        },
        async getAppVersion() {
          return AppConstants.MOZ_APP_VERSION;
        },
        async getBlockList() {
          const trackingTable = Services.prefs.getCharPref(
            "urlclassifier.trackingTable"
          );
          // If content-track-digest256 is in the tracking table,
          // the user has enabled the strict list.
          return trackingTable.includes("content") ? "strict" : "basic";
        },
        async getBuildID() {
          return Services.appinfo.appBuildID;
        },
        async getUpdateChannel() {
          return AppConstants.MOZ_UPDATE_CHANNEL;
        },
        async getPlatform() {
          return AppConstants.platform;
        },
        async hasTouchScreen() {
          const gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(
            Ci.nsIGfxInfo
          );
          return gfxInfo.getInfo().ApzTouchInput == 1;
        },
        async getAdditionalData() {
          const blockList = await this.getBlockList();
          const userAgent = getUserAgent();
          const gfxData = getGfxData();
          const prefs = getAdditionalPrefs();
          const memoryMb = getMemoryMB();

          const data = {
            applicationName: Services.appinfo.name,
            version: Services.appinfo.version,
            updateChannel: AppConstants.MOZ_UPDATE_CHANNEL,
            osArchitecture: getSysinfoProperty("arch", null),
            osName: getSysinfoProperty("name", null),
            osVersion: getSysinfoProperty("version", null),
            fissionEnabled: Services.appinfo.fissionAutostart,
            userAgent,
            gfxData,
            blockList,
            prefs,
            memoryMb,
          };

          if (AppConstants.isPlatformAndVersionAtLeast("win", "6.2")) {
            data.sec = getSecurityAppData();
          }

          if (AppConstants.platform === "android") {
            data.device = getSysinfoProperty("device", null);
            data.isTablet = getSysinfoProperty("tablet", false);
          }

          return data;
        },
      },
    };
  }
};
