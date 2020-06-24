/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from preferences.js */

var { FeatureGate } = ChromeUtils.import(
  "resource://featuregates/FeatureGate.jsm"
);

var gExperimentalPane = {
  inited: false,
  _template: null,
  _featureGatesContainer: null,
  _boundRestartObserver: null,
  _observedPrefs: [],
  _shouldPromptForRestart: true,

  _featureGatePrefTypeToPrefServiceType(featureGatePrefType) {
    if (featureGatePrefType != "boolean") {
      throw new Error("Only boolean FeatureGates are supported");
    }
    return "bool";
  },

  async _observeRestart(aSubject, aTopic, aData) {
    if (!this._shouldPromptForRestart) {
      return;
    }
    let prefValue = Services.prefs.getBoolPref(aData);
    let buttonIndex = await confirmRestartPrompt(prefValue, 1, true, false);
    if (buttonIndex == CONFIRM_RESTART_PROMPT_RESTART_NOW) {
      Services.startup.quit(
        Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart
      );
      return;
    }
    this._shouldPromptForRestart = false;
    Services.prefs.setBoolPref(aData, !prefValue);
    this._shouldPromptForRestart = true;
  },

  addPrefObserver(name, fn) {
    this._observedPrefs.push({ name, fn });
    Services.prefs.addObserver(name, fn);
  },

  removePrefObservers() {
    for (let { name, fn } of this._observedPrefs) {
      Services.prefs.removeObserver(name, fn);
    }
    this._observedPrefs = [];
  },

  async init() {
    if (this.inited) {
      return;
    }
    this.inited = true;
    window.addEventListener("unload", () => this.removePrefObservers());
    this._template = document.getElementById("template-featureGate");
    this._featureGatesContainer = document.getElementById(
      "pane-experimental-featureGates"
    );
    this._boundRestartObserver = this._observeRestart.bind(this);
    let features = await FeatureGate.all();
    let frag = document.createDocumentFragment();
    for (let feature of features) {
      if (Preferences.get(feature.preference)) {
        console.error(
          "Preference control already exists for experimental feature '" +
            feature.id +
            "' with preference '" +
            feature.preference +
            "'"
        );
        continue;
      }
      if (feature.restartRequired) {
        this.addPrefObserver(feature.preference, this._boundRestartObserver);
      }
      let template = this._template.content.cloneNode(true);
      let checkbox = template.querySelector(".featureGateCheckbox");
      checkbox.setAttribute("preference", feature.preference);
      checkbox.id = feature.id;
      let title = template.querySelector(".featureGateTitle");
      document.l10n.setAttributes(title, feature.title);
      title.setAttribute("control", feature.id);
      let description = template.querySelector(".featureGateDescription");
      let descriptionLinks = feature.descriptionLinks || {};
      for (let [key, value] of Object.entries(descriptionLinks)) {
        let link = document.createElement("a");
        link.setAttribute("data-l10n-name", key);
        link.setAttribute("href", value);
        link.setAttribute("target", "_blank");
        description.append(link);
      }
      document.l10n.setAttributes(description, feature.description);
      description.setAttribute("control", feature.id);
      frag.appendChild(template);
      let preference = Preferences.add({
        id: feature.preference,
        type: gExperimentalPane._featureGatePrefTypeToPrefServiceType(
          feature.type
        ),
      });
      preference.setElementValue(checkbox);
    }
    this._featureGatesContainer.appendChild(frag);
    Preferences.updateAllElements();
  },
};
