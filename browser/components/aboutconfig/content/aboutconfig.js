/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/Preferences.jsm");

let gDefaultBranch = Services.prefs.getDefaultBranch("");

/**
 * Maps the name of each preference that exists in the back-end to its PrefRow
 * object. This is as an optimization to avoid querying the preferences service
 * each time the list is filtered.
 */
let gExistingPrefs = new Map();

/**
 * Maps each row element currently in the table to its PrefRow object.
 */
let gElementToPrefMap = new WeakMap();

/**
 * Reference to the PrefRow currently being edited, if any.
 */
let gPrefInEdit = null;

class PrefRow {
  constructor(name) {
    this.name = name;
    this.value = true;
    this.refreshValue();

    this.editing = false;
    this.element = document.createElement("tr");
    this._setupElement();
    gElementToPrefMap.set(this.element, this);
  }

  refreshValue() {
    this.hasDefaultValue = prefHasDefaultValue(this.name);
    this.hasUserValue = Services.prefs.prefHasUserValue(this.name);
    this.isLocked = Services.prefs.prefIsLocked(this.name);

    // If this preference has been deleted, we keep its last known value.
    if (!this.exists) {
      return;
    }

    try {
      // This can throw for locked preferences without a default value.
      this.value = Preferences.get(this.name);
      // We don't know which preferences should be read using getComplexValue,
      // so we use a heuristic to determine if this is a localized preference.
      if (!this.hasUserValue &&
          /^chrome:\/\/.+\/locale\/.+\.properties/.test(this.value)) {
        // This can throw if there is no value in the localized files.
        this.value = Services.prefs.getComplexValue(this.name,
          Ci.nsIPrefLocalizedString).data;
      }
    } catch (ex) {
      this.value = "";
    }
  }

  get type() {
    return this.value.constructor.name;
  }

  get exists() {
    return this.hasDefaultValue || this.hasUserValue;
  }

  _setupElement() {
    this.element.textContent = "";
    let nameCell = document.createElement("td");
    this.element.append(
      nameCell,
      this.valueCell = document.createElement("td"),
      this.editCell = document.createElement("td"),
      this.resetCell = document.createElement("td")
    );
    this.editCell.appendChild(
      this.editButton = document.createElement("button")
    );
    delete this.resetButton;

    nameCell.className = "cell-name";
    this.valueCell.className = "cell-value";
    this.editCell.className = "cell-edit";

    // Add <wbr> behind dots to prevent line breaking in random mid-word places.
    let parts = this.name.split(".");
    for (let i = 0; i < parts.length - 1; i++) {
      nameCell.append(parts[i] + ".", document.createElement("wbr"));
    }
    nameCell.append(parts[parts.length - 1]);

    this.refreshElement();
  }

  refreshElement() {
    this.element.classList.toggle("has-user-value", !!this.hasUserValue);
    this.element.classList.toggle("locked", !!this.isLocked);
    this.element.classList.toggle("deleted", !this.exists);
    if (this.exists && !this.editing) {
      this.valueCell.textContent = this.value;
      if (this.type == "Boolean") {
        document.l10n.setAttributes(this.editButton, "about-config-pref-toggle");
        this.editButton.className = "button-toggle";
      } else {
        document.l10n.setAttributes(this.editButton, "about-config-pref-edit");
        this.editButton.className = "button-edit";
      }
      this.editButton.removeAttribute("form");
      delete this.inputField;
    } else {
      this.valueCell.textContent = "";
      // The form is needed for the validation report to appear, but we need to
      // prevent the associated button from reloading the page.
      let form = document.createElement("form");
      form.addEventListener("submit", event => event.preventDefault());
      form.id = "form-edit";
      if (this.editing) {
        this.inputField = document.createElement("input");
        this.inputField.value = this.value;
        if (this.type == "Number") {
          this.inputField.type = "number";
          this.inputField.required = true;
          this.inputField.min = -2147483648;
          this.inputField.max = 2147483647;
        } else {
          this.inputField.type = "text";
        }
        form.appendChild(this.inputField);
        document.l10n.setAttributes(this.editButton, "about-config-pref-save");
        this.editButton.className = "primary button-save";
      } else {
        delete this.inputField;
        for (let type of ["Boolean", "Number", "String"]) {
          let radio = document.createElement("input");
          radio.type = "radio";
          radio.name = "type";
          radio.value = type;
          radio.checked = this.type == type;
          form.appendChild(radio);
          let radioLabel = document.createElement("span");
          radioLabel.textContent = type;
          form.appendChild(radioLabel);
        }
        form.addEventListener("click", event => {
          if (event.target.name != "type") {
            return;
          }
          let type = event.target.value;
          if (this.type != type) {
            if (type == "Boolean") {
              this.value = true;
            } else if (type == "Number") {
              this.value = 0;
            } else {
              this.value = "";
            }
          }
        });
        document.l10n.setAttributes(this.editButton, "about-config-pref-add");
        this.editButton.className = "button-add";
      }
      this.valueCell.appendChild(form);
      this.editButton.setAttribute("form", "form-edit");
    }
    this.editButton.disabled = this.isLocked;
    if (!this.isLocked && this.hasUserValue) {
      if (!this.resetButton) {
        this.resetButton = document.createElement("button");
        this.resetCell.appendChild(this.resetButton);
      }
      if (!this.hasDefaultValue) {
        document.l10n.setAttributes(this.resetButton,
                                    "about-config-pref-delete");
        this.resetButton.className = "";
      } else {
        document.l10n.setAttributes(this.resetButton,
                                    "about-config-pref-reset");
        this.resetButton.className = "button-reset";
      }
    } else if (this.resetButton) {
      this.resetButton.remove();
      delete this.resetButton;
    }
  }

  edit() {
    if (gPrefInEdit) {
      gPrefInEdit.endEdit();
    }
    gPrefInEdit = this;
    this.editing = true;
    this.refreshElement();
    this.inputField.focus();
  }

  save() {
    if (this.type == "Number") {
      if (!this.inputField.reportValidity()) {
        return;
      }
      Services.prefs.setIntPref(this.name, parseInt(this.inputField.value));
    } else {
      Services.prefs.setStringPref(this.name, this.inputField.value);
    }
    this.refreshValue();
    this.endEdit();
    this.editButton.focus();
  }

  endEdit() {
    this.editing = false;
    this.refreshElement();
    gPrefInEdit = null;
  }
}

document.addEventListener("DOMContentLoaded", () => {
  if (!Preferences.get("browser.aboutConfig.showWarning")) {
    loadPrefs();
  }
}, { once: true });

function alterWarningState() {
  Services.prefs.setBoolPref("browser.aboutConfig.showWarning",
    document.getElementById("showWarningNextTime").checked);
}

function loadPrefs() {
  [...document.styleSheets].find(s => s.title == "infop").disabled = true;

  document.body.textContent = "";
  let search = document.createElement("input");
  search.type = "text";
  search.id = "search";
  document.l10n.setAttributes(search, "about-config-search");
  document.body.appendChild(search);
  let prefs = document.createElement("table");
  prefs.id = "prefs";
  document.body.appendChild(prefs);

  gExistingPrefs = new Map(Services.prefs.getChildList("")
                                   .map(name => [name, new PrefRow(name)]));

  search.addEventListener("keypress", e => {
    if (e.key == "Enter") {
      filterPrefs();
    }
  });

  prefs.addEventListener("click", event => {
    if (event.target.localName != "button") {
      return;
    }
    let prefRow = event.target.closest("tr");
    let pref = gElementToPrefMap.get(prefRow);
    let button = event.target.closest("button");
    if (button.classList.contains("button-reset")) {
      Services.prefs.clearUserPref(pref.name);
      pref.refreshValue();
      pref.refreshElement();
      pref.editButton.focus();
    } else if (button.classList.contains("button-add")) {
      Preferences.set(pref.name, pref.value);
      pref.refreshValue();
      pref.refreshElement();
      if (pref.type != "Boolean") {
        pref.edit();
      }
    } else if (button.classList.contains("button-toggle")) {
      Services.prefs.setBoolPref(pref.name, !pref.value);
      pref.refreshValue();
      pref.refreshElement();
    } else if (button.classList.contains("button-edit")) {
      pref.edit();
    } else if (button.classList.contains("button-save")) {
      pref.save();
    } else {
      pref.editing = false;
      Services.prefs.clearUserPref(pref.name);
      gExistingPrefs.delete(pref.name);
      pref.refreshValue();
      pref.refreshElement();
    }
  });

  filterPrefs();
}

function filterPrefs() {
  if (gPrefInEdit) {
    gPrefInEdit.endEdit();
  }

  let substring = document.getElementById("search").value.trim();
  let prefArray = [...gExistingPrefs.values()];
  if (substring) {
    prefArray = prefArray.filter(pref => pref.name.includes(substring));
  }
  prefArray.sort((a, b) => a.name > b.name);
  let prefsElement = document.getElementById("prefs");
  let fragment = document.createDocumentFragment();
  for (let pref of prefArray) {
    fragment.appendChild(pref.element);
  }
  if (substring && !gExistingPrefs.has(substring)) {
    fragment.appendChild((new PrefRow(substring)).element);
  }
  prefsElement.textContent = "";
  prefsElement.appendChild(fragment);
}

function prefHasDefaultValue(name) {
  try {
    switch (Services.prefs.getPrefType(name)) {
      case Ci.nsIPrefBranch.PREF_STRING:
        gDefaultBranch.getStringPref(name);
        return true;
      case Ci.nsIPrefBranch.PREF_INT:
        gDefaultBranch.getIntPref(name);
        return true;
      case Ci.nsIPrefBranch.PREF_BOOL:
        gDefaultBranch.getBoolPref(name);
        return true;
    }
  } catch (ex) {}
  return false;
}
