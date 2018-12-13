/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/Preferences.jsm");

let gDefaultBranch = Services.prefs.getDefaultBranch("");
let gPrefArray;
let gPrefRowInEdit;
let gPrefInEdit;

function getPrefName(prefRow) {
  return prefRow.getAttribute("aria-label");
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

  gPrefArray = Services.prefs.getChildList("").map(function(name) {
    let hasUserValue = Services.prefs.prefHasUserValue(name);
    let pref = {
      name,
      hasUserValue,
      hasDefaultValue: hasUserValue ? prefHasDefaultValue(name) : true,
      isLocked: Services.prefs.prefIsLocked(name),
    };
    // Try in case it's a localized string or locked user added pref
    // If an execption is thrown the pref value is set to the empty string.
    try {
      // Throws an exception in case locked user added pref without default value
      pref.value = Preferences.get(name);
      // Throws an exception if there is no equivalent value in the localized files for the pref.
      if (!pref.hasUserValue && /^chrome:\/\/.+\/locale\/.+\.properties/.test(pref.value)) {
        pref.value = Services.prefs.getComplexValue(name, Ci.nsIPrefLocalizedString).data;
      }
    } catch (ex) {
      pref.value = "";
    }
    return pref;
  });

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
      pref.value = Preferences.get(prefName);
      pref.hasUserValue = false;
      // Update UI.
      prefRow.textContent = "";
      prefRow.classList.remove("has-user-value");
      prefRow.appendChild(getPrefRow(pref));
      prefRow.querySelector("td.cell-edit").firstChild.focus();
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
      pref.value = !pref.value;
      pref.hasUserValue = Services.prefs.prefHasUserValue(pref.name);
      // Update UI.
      prefRow.textContent = "";
      if (pref.hasUserValue) {
        prefRow.classList.add("has-user-value");
      } else {
        prefRow.classList.remove("has-user-value");
      }
      prefRow.appendChild(getPrefRow(pref));
      prefRow.querySelector("td.cell-edit").firstChild.focus();
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
    let row = document.createElement("tr");
    if (pref.hasUserValue) {
      row.classList.add("has-user-value");
    }
    if (pref.isLocked) {
      row.classList.add("locked");
    }
    row.setAttribute("aria-label", pref.name);

    row.appendChild(getPrefRow(pref));
    fragment.appendChild(row);
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

function getPrefRow(pref) {
  let rowFragment = document.createDocumentFragment();
  let nameCell = document.createElement("td");
  nameCell.className = "cell-name";
  // Add <wbr> behind dots to prevent line breaking in random mid-word places.
  let parts = pref.name.split(".");
  for (let i = 0; i < parts.length - 1; i++) {
    nameCell.append(parts[i] + ".", document.createElement("wbr"));
  }
  nameCell.append(parts[parts.length - 1]);
  rowFragment.appendChild(nameCell);

  let valueCell = document.createElement("td");
  valueCell.classList.add("cell-value");
  valueCell.textContent = pref.value;
  rowFragment.appendChild(valueCell);

  let editCell = document.createElement("td");
  editCell.className = "cell-edit";
  let button = document.createElement("button");
  if (Services.prefs.getPrefType(pref.name) == Services.prefs.PREF_BOOL) {
    document.l10n.setAttributes(button, "about-config-pref-toggle");
    button.className = "button-toggle";
  } else {
    document.l10n.setAttributes(button, "about-config-pref-edit");
    button.className = "button-edit";
  }
  if (pref.isLocked) {
    button.disabled = true;
  }
  editCell.appendChild(button);
  rowFragment.appendChild(editCell);

  let buttonCell = document.createElement("td");
  if (!pref.isLocked && pref.hasUserValue) {
    let resetButton = document.createElement("button");
    if (!pref.hasDefaultValue) {
      document.l10n.setAttributes(resetButton, "about-config-pref-delete");
    } else {
      document.l10n.setAttributes(resetButton, "about-config-pref-reset");
      resetButton.className = "button-reset";
    }
    buttonCell.appendChild(resetButton);
  }

  rowFragment.appendChild(buttonCell);
  return rowFragment;
}

function startEditingPref(row, arrayEntry) {
  if (gPrefRowInEdit != undefined) {
    // Abort editing-process first.
    gPrefRowInEdit.textContent = "";
    gPrefRowInEdit.appendChild(getPrefRow(gPrefInEdit));
  }
  gPrefRowInEdit = row;

  let name = getPrefName(row);
  gPrefInEdit = arrayEntry;

  let valueCell = row.querySelector("td.cell-value");
  let oldValue = valueCell.textContent;
  valueCell.textContent = "";
  // The form is needed for the invalid-tooltip to appear.
  let form = document.createElement("form");
  form.id = "form-" + name;
  let inputField = document.createElement("input");
  inputField.type = "text";
  inputField.value = oldValue;
  if (Services.prefs.getPrefType(name) == Services.prefs.PREF_INT) {
    inputField.setAttribute("pattern", "-?[0-9]*");
    document.l10n.setAttributes(inputField, "about-config-pref-input-number");
  } else {
    document.l10n.setAttributes(inputField, "about-config-pref-input-string");
  }
  inputField.placeholder = oldValue;
  form.appendChild(inputField);
  valueCell.appendChild(form);

  let buttonCell = row.querySelector("td.cell-edit");
  buttonCell.childNodes[0].remove();
  let button = document.createElement("button");
  button.classList.add("primary", "button-save");
  document.l10n.setAttributes(button, "about-config-pref-save");
  button.setAttribute("form", form.id);
  buttonCell.appendChild(button);
}

function endEditingPref(row) {
  let name = gPrefInEdit.name;
  let input = row.querySelector("td.cell-value").firstChild.firstChild;
  let newValue = input.value;

  if (Services.prefs.getPrefType(name) == Services.prefs.PREF_INT) {
    let numberValue = parseInt(newValue);
    if (!/^-?[0-9]*$/.test(newValue) || isNaN(numberValue)) {
      input.setCustomValidity(input.title);
      return;
    }
    newValue = numberValue;
    Services.prefs.setIntPref(name, newValue);
  } else {
    Services.prefs.setStringPref(name, newValue);
  }

  // Update gPrefArray.
  gPrefInEdit.value = newValue;
  gPrefInEdit.hasUserValue = Services.prefs.prefHasUserValue(name);
  // Update UI.
  row.textContent = "";
  if (gPrefInEdit.hasUserValue) {
    row.classList.add("has-user-value");
  } else {
    row.classList.remove("has-user-value");
  }
  row.appendChild(getPrefRow(gPrefInEdit));
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
  gPrefArray.push({
    name,
    value,
    hasUserValue: true,
    hasDefaultValue: false,
  });
  gPrefArray.sort((a, b) => a.name > b.name);
  filterPrefs();
}
