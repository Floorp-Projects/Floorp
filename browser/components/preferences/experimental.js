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

  _featureGatePrefTypeToPrefServiceType(featureGatePrefType) {
    if (featureGatePrefType != "boolean") {
      throw new Error("Only boolean FeatureGates are supported");
    }
    return "bool";
  },

  async init() {
    if (this.inited) {
      return;
    }
    this._template = document.getElementById("template-featureGate");
    this._featureGatesContainer = document.getElementById(
      "pane-experimental-featureGates"
    );
    this.inited = true;
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
      let template = this._template.content.cloneNode(true);
      let checkbox = template.querySelector(".featureGateCheckbox");
      checkbox.setAttribute("preference", feature.preference);
      checkbox.id = feature.id;
      let title = template.querySelector(".featureGateTitle");
      document.l10n.setAttributes(title, feature.title);
      title.setAttribute("control", feature.id);
      let description = template.querySelector(".featureGateDescription");
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
