/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals browser, module */

class UAOverrides {
  constructor(availableOverrides) {
    this.OVERRIDE_PREF = "perform_ua_overrides";

    this._overridesEnabled = true;

    this._availableOverrides = availableOverrides;
    this._activeListeners = new Map();
  }

  bindAboutCompatBroker(broker) {
    this._aboutCompatBroker = broker;
  }

  bootup() {
    browser.aboutConfigPrefs.onPrefChange.addListener(() => {
      this.checkOverridePref();
    }, this.OVERRIDE_PREF);
    this.checkOverridePref();
  }

  checkOverridePref() {
    browser.aboutConfigPrefs.getPref(this.OVERRIDE_PREF).then(value => {
      if (value === undefined) {
        browser.aboutConfigPrefs.setPref(this.OVERRIDE_PREF, true);
      } else if (value === false) {
        this.unregisterUAOverrides();
      } else {
        this.registerUAOverrides();
      }
    });
  }

  getAvailableOverrides() {
    return this._availableOverrides;
  }

  isEnabled() {
    return this._overridesEnabled;
  }

  enableOverride(override) {
    if (override.active) {
      return;
    }

    const { matches, uaTransformer } = override.config;
    const listener = details => {
      for (const header of details.requestHeaders) {
        if (header.name.toLowerCase() === "user-agent") {
          header.value = uaTransformer(header.value);
        }
      }
      return { requestHeaders: details.requestHeaders };
    };

    browser.webRequest.onBeforeSendHeaders.addListener(
      listener,
      { urls: matches },
      ["blocking", "requestHeaders"]
    );

    this._activeListeners.set(override, listener);
    override.active = true;
  }

  async registerUAOverrides() {
    const platformMatches = ["all"];
    let platformInfo = await browser.runtime.getPlatformInfo();
    platformMatches.push(platformInfo.os == "android" ? "android" : "desktop");

    for (const override of this._availableOverrides) {
      if (platformMatches.includes(override.platform)) {
        override.availableOnPlatform = true;
        this.enableOverride(override);
      }
    }

    this._overridesEnabled = true;
    this._aboutCompatBroker.portsToAboutCompatTabs.broadcast({
      overridesChanged: this._aboutCompatBroker.filterOverrides(
        this._availableOverrides
      ),
    });
  }

  unregisterUAOverrides() {
    for (const override of this._availableOverrides) {
      this.disableOverride(override);
    }

    this._overridesEnabled = false;
    this._aboutCompatBroker.portsToAboutCompatTabs.broadcast({
      overridesChanged: false,
    });
  }

  disableOverride(override) {
    if (!override.active) {
      return;
    }

    browser.webRequest.onBeforeSendHeaders.removeListener(
      this._activeListeners.get(override)
    );
    override.active = false;
    this._activeListeners.delete(override);
  }
}

module.exports = UAOverrides;
