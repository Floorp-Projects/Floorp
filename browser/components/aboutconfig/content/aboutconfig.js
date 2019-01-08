/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/Preferences.jsm");

let gDefaultBranch = Services.prefs.getDefaultBranch("");
let gPrefArray;
let gPrefInEdit;

class PrefRow {
  constructor(name) {
    this.name = name;
    this.refreshValue();

    this.element = document.createElement("tr");
    this.element.setAttribute("aria-label", this.name);
    this.rebuildElement();
  }

  refreshValue() {
    this.hasUserValue = Services.prefs.prefHasUserValue(this.name);
    this.hasDefaultValue = this.hasUserValue ? prefHasDefaultValue(this.name)
                                             : true;
    this.isLocked = Services.prefs.prefIsLocked(this.name);

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

  rebuildElement() {
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
    this.valueCell.textContent = this.value;
    if (this.value.constructor.name == "Boolean") {
      document.l10n.setAttributes(this.editButton, "about-config-pref-toggle");
      this.editButton.className = "button-toggle";
    } else {
      document.l10n.setAttributes(this.editButton, "about-config-pref-edit");
      this.editButton.className = "button-edit";
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
}

function getPrefName(prefRow) {
  return prefRow.getAttribute("aria-label");
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

  gPrefArray = Services.prefs.getChildList("").map(name => new PrefRow(name));

  gPrefArray.sort((a, b) => a.name > b.name);

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
    let prefName = getPrefName(prefRow);
    let pref = gPrefArray.find(p => p.name == prefName);
    let button = event.target.closest("button");
    if (button.classList.contains("button-reset")) {
      // Reset pref and update gPrefArray.
      Services.prefs.clearUserPref(prefName);
      pref.refreshValue();
      pref.rebuildElement();
      pref.editButton.focus();
    } else if (button.classList.contains("add-true")) {
      addNewPref(prefRow.firstChild.innerHTML, true);
    } else if (button.classList.contains("add-false")) {
      addNewPref(prefRow.firstChild.innerHTML, false);
    } else if (button.classList.contains("add-Number") ||
      button.classList.contains("add-String")) {
      addNewPref(prefRow.firstChild.innerHTML,
        button.classList.contains("add-Number") ? 0 : "");
      prefRow = [...prefs.getElementsByTagName("tr")]
        .find(row => row.querySelector("td").textContent == prefName);
      startEditingPref(prefRow, gPrefArray.find(p => p.name == prefName));
      prefRow.querySelector("td.cell-value").firstChild.firstChild.focus();
    } else if (button.classList.contains("button-toggle")) {
      // Toggle the pref and update gPrefArray.
      Services.prefs.setBoolPref(prefName, !pref.value);
      pref.refreshValue();
      pref.refreshElement();
    } else if (button.classList.contains("button-edit")) {
      startEditingPref(prefRow, pref);
      prefRow.querySelector("td.cell-value").firstChild.firstChild.focus();
    } else if (button.classList.contains("button-save")) {
      endEditingPref(prefRow);
      prefRow.querySelector("td.cell-edit").firstChild.focus();
    } else {
      Services.prefs.clearUserPref(prefName);
      gPrefArray.splice(gPrefArray.findIndex(p => p.name == prefName), 1);
      prefRow.remove();
    }
  });

  filterPrefs();
}

function filterPrefs() {
  let substring = document.getElementById("search").value.trim();
  document.getElementById("prefs").textContent = "";
  if (substring && !gPrefArray.some(pref => pref.name == substring)) {
    document.getElementById("prefs").appendChild(createNewPrefFragment(substring));
  }
  let fragment = createPrefsFragment(gPrefArray.filter(pref => pref.name.includes(substring)));
  document.getElementById("prefs").appendChild(fragment);
}

function createPrefsFragment(prefArray) {
  let fragment = document.createDocumentFragment();
  for (let pref of prefArray) {
    fragment.appendChild(pref.element);
  }
  return fragment;
}

function createNewPrefFragment(name) {
  let fragment = document.createDocumentFragment();
  let row = document.createElement("tr");
  row.classList.add("has-user-value");
  row.setAttribute("aria-label", name);
  let nameCell = document.createElement("td");
  nameCell.className = "cell-name";
  nameCell.append(name);
  row.appendChild(nameCell);

  let valueCell = document.createElement("td");
  valueCell.classList.add("cell-value");
  let guideText = document.createElement("span");
  document.l10n.setAttributes(guideText, "about-config-pref-add");
  valueCell.appendChild(guideText);
  for (let item of ["true", "false", "Number", "String"]) {
    let optionBtn = document.createElement("button");
    optionBtn.textContent = item;
    optionBtn.classList.add("add-" + item);
    valueCell.appendChild(optionBtn);
  }
  row.appendChild(valueCell);

  let editCell = document.createElement("td");
  row.appendChild(editCell);

  let buttonCell = document.createElement("td");
  row.appendChild(buttonCell);

  fragment.appendChild(row);
  return fragment;
}

function startEditingPref(row, arrayEntry) {
  if (gPrefInEdit) {
    // Abort editing-process first.
    gPrefInEdit.rebuildElement();
  }

  let name = getPrefName(row);
  gPrefInEdit = arrayEntry;

  let valueCell = row.querySelector("td.cell-value");
  valueCell.textContent = "";
  // The form is needed for the invalid-tooltip to appear.
  let form = document.createElement("form");
  form.id = "form-edit";
  let inputField = document.createElement("input");
  inputField.value = arrayEntry.value;
  if (arrayEntry.value.constructor.name == "Number") {
    inputField.type = "number";
    inputField.required = true;
    inputField.min = -2147483648;
    inputField.max = 2147483647;
  } else {
    inputField.type = "text";
  }
  form.appendChild(inputField);
  valueCell.appendChild(form);

  let buttonCell = row.querySelector("td.cell-edit");
  buttonCell.childNodes[0].remove();
  let button = document.createElement("button");
  button.classList.add("primary", "button-save");
  document.l10n.setAttributes(button, "about-config-pref-save");
  button.setAttribute("form", "form-edit");
  buttonCell.appendChild(button);
}

function endEditingPref(row) {
  let name = gPrefInEdit.name;
  let input = row.querySelector("td.cell-value").firstChild.firstChild;
  let newValue = input.value;
  if (Services.prefs.getPrefType(name) == Services.prefs.PREF_INT) {
    if (!input.reportValidity()) {
      return;
    }
    Services.prefs.setIntPref(name, parseInt(newValue));
  } else {
    Services.prefs.setStringPref(name, newValue);
  }

  // Update gPrefArray.
  gPrefInEdit.refreshValue();
  gPrefInEdit.rebuildElement();
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

function addNewPref(name, value) {
  Preferences.set(name, value);
  gPrefArray.push(new PrefRow(name));
  gPrefArray.sort((a, b) => a.name > b.name);
  filterPrefs();
}
