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

    const { blocks, matches, telemetryKey, uaTransformer } = override.config;
    const listener = details => {
      // We set the "used" telemetry key if the user would have had the
      // override applied, regardless of whether it is actually applied.
      if (!details.frameId && override.shouldSendDetailedTelemetry) {
        // For now, we only care about Telemetry on Fennec, where telemetry
        // is sent in Java code (as part of the core ping). That code must
        // be aware of each key we send, which we send as a SharedPreference.
        browser.sharedPreferences.setBoolPref(`${telemetryKey}Used`, true);
      }

      // Don't actually override the UA for an experiment if the user is not
      // part of the experiment (unless they force-enabed the override).
      if (
        !override.config.experiment ||
        override.experimentActive ||
        override.permanentPrefEnabled === true
      ) {
        for (const header of details.requestHeaders) {
          if (header.name.toLowerCase() === "user-agent") {
            header.value = uaTransformer(header.value);
          }
        }
      }
      return { requestHeaders: details.requestHeaders };
    };

    browser.webRequest.onBeforeSendHeaders.addListener(
      listener,
      { urls: matches },
      ["blocking", "requestHeaders"]
    );

    const listeners = { onBeforeSendHeaders: listener };
    if (blocks) {
      const blistener = details => {
        return { cancel: true };
      };

      browser.webRequest.onBeforeRequest.addListener(
        blistener,
        { urls: blocks },
        ["blocking"]
      );

      listeners.onBeforeRequest = blistener;
    }
    this._activeListeners.set(override, listeners);
    override.active = true;

    // If telemetry is being collected, note the addon version.
    if (telemetryKey) {
      const { version } = browser.runtime.getManifest();
      browser.sharedPreferences.setCharPref(`${telemetryKey}Version`, version);
    }

    // If collecting detailed telemetry on the override, note that it was activated.
    if (override.shouldSendDetailedTelemetry) {
      browser.sharedPreferences.setBoolPref(`${telemetryKey}Ready`, true);
    }
  }

  onOverrideConfigChanged(override) {
    // Check whether the override should be hidden from about:compat.
    override.hidden = override.config.hidden;

    // Also hide if the override is in an experiment the user is not part of.
    if (override.config.experiment && !override.experimentActive) {
      override.hidden = true;
    }

    // Setting the override's permanent pref overrules whether it is hidden.
    if (override.permanentPrefEnabled !== undefined) {
      override.hidden = !override.permanentPrefEnabled;
    }

    // Also check whether the override should be active.
    let shouldBeActive = true;

    // Overrides can be force-deactivated by their permanent preference.
    if (override.permanentPrefEnabled === false) {
      shouldBeActive = false;
    }

    // Only send detailed telemetry if the user is actively in an experiment or
    // has opted into an experimental feature.
    override.shouldSendDetailedTelemetry =
      override.config.telemetryKey &&
      (override.experimentActive || override.permanentPrefEnabled);

    // Overrides gated behind an experiment the user is not part of do not
    // have to be activated, unless they are gathering telemetry, or the
    // user has force-enabled them with their permanent pref.
    if (
      override.config.experiment &&
      !override.experimentActive &&
      !override.config.telemetryKey &&
      override.permanentPrefEnabled !== true
    ) {
      shouldBeActive = false;
    }

    if (shouldBeActive) {
      this.enableOverride(override);
    } else {
      this.disableOverride(override);
    }

    if (this._overridesEnabled) {
      this._aboutCompatBroker.portsToAboutCompatTabs.broadcast({
        overridesChanged: this._aboutCompatBroker.filterOverrides(
          this._availableOverrides
        ),
      });
    }
  }

  async registerUAOverrides() {
    const platformMatches = ["all"];
    let platformInfo = await browser.runtime.getPlatformInfo();
    platformMatches.push(platformInfo.os == "android" ? "android" : "desktop");

    for (const override of this._availableOverrides) {
      if (platformMatches.includes(override.platform)) {
        override.availableOnPlatform = true;

        // Note whether the user is actively in the override's experiment (if any).
        override.experimentActive = false;
        const experiment = override.config.experiment;
        if (experiment) {
          // We expect the definition to have either one string for 'experiment'
          // (just one branch) or an array of strings (multiple branches). So
          // here we turn the string case into a one-element array for the loop.
          const branches = Array.isArray(experiment)
            ? experiment
            : [experiment];
          for (const branch of branches) {
            if (await browser.experiments.isActive(branch)) {
              override.experimentActive = true;
              break;
            }
          }
        }

        // If there is a specific about:config preference governing
        // this override, monitor its state.
        const pref = override.config.permanentPref;
        override.permanentPrefEnabled =
          pref && (await browser.aboutConfigPrefs.getPref(pref));
        if (pref) {
          const checkOverridePref = () => {
            browser.aboutConfigPrefs.getPref(pref).then(value => {
              override.permanentPrefEnabled = value;
              this.onOverrideConfigChanged(override);
            });
          };
          browser.aboutConfigPrefs.onPrefChange.addListener(
            checkOverridePref,
            pref
          );
        }

        this.onOverrideConfigChanged(override);
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

    const listeners = this._activeListeners.get(override);
    for (const [name, listener] of Object.entries(listeners)) {
      browser.webRequest[name].removeListener(listener);
    }
    override.active = false;
    this._activeListeners.delete(override);
  }
}

module.exports = UAOverrides;
