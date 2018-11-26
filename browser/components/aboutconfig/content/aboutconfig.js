/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/Preferences.jsm");

let gDefaultBranch = Services.prefs.getDefaultBranch("");
let gPrefArray;

function onLoad() {
  gPrefArray = Services.prefs.getChildList("").map(function(name) {
    let hasUserValue = Services.prefs.prefHasUserValue(name);
    let pref = {
      name,
      value: Preferences.get(name),
      hasUserValue,
      hasDefaultValue: hasUserValue ? prefHasDefaultValue(name) : true,
    };
    // Try in case it's a localized string.
    // Throws an exception if there is no equivalent value in the localized files for the pref.
    // If an execption is thrown the pref value is set to the empty string.
    try {
      if (!pref.hasUserValue && /^chrome:\/\/.+\/locale\/.+\.properties/.test(pref.value)) {
        pref.value = Services.prefs.getComplexValue(name, Ci.nsIPrefLocalizedString).data;
      }
    } catch (ex) {
      pref.value = "";
    }
    return pref;
  });

  gPrefArray.sort((a, b) => a.name > b.name);

  document.getElementById("search").addEventListener("keypress", function(e) {
    if (e.code == "Enter") {
      filterPrefs();
    }
  });

  document.getElementById("prefs").addEventListener("click", (event) => {
    if (event.target.localName != "button") {
      return;
    }
    let prefRow = event.target.closest("tr");
    let prefName = prefRow.getAttribute("aria-label");
    let button = event.target.closest("button");
    if (button.classList.contains("button-reset")) {
      // Reset pref and update gPrefArray.
      Services.prefs.clearUserPref(prefName);
      let pref = gPrefArray.find(p => p.name == prefName);
      pref.value = Preferences.get(prefName);
      pref.hasUserValue = false;
      // Update UI.
      prefRow.textContent = "";
      prefRow.classList.remove("has-user-value");
      prefRow.appendChild(getPrefRow(pref));
    } else {
      Services.prefs.clearUserPref(prefName);
      gPrefArray.splice(gPrefArray.findIndex(pref => pref.name == prefName), 1);
      prefRow.remove();
    }
  });

  document.getElementById("prefs").appendChild(createPrefsFragment(gPrefArray));
}

function filterPrefs() {
  let substring = document.getElementById("search").value.trim();
  let fragment = createPrefsFragment(gPrefArray.filter(pref => pref.name.includes(substring)));
  document.getElementById("prefs").textContent = "";
  document.getElementById("prefs").appendChild(fragment);
}

function createPrefsFragment(prefArray) {
  let fragment = document.createDocumentFragment();
  for (let pref of prefArray) {
    let row = document.createElement("tr");
    if (pref.hasUserValue) {
      row.classList.add("has-user-value");
    }
    row.setAttribute("aria-label", pref.name);

    row.appendChild(getPrefRow(pref));
    fragment.appendChild(row);
  }
  return fragment;
}

function getPrefRow(pref) {
  let rowFragment = document.createDocumentFragment();
  let nameCell = document.createElement("td");
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

  let buttonCell = document.createElement("td");
  if (pref.hasUserValue) {
    let button = document.createElement("button");
    if (!pref.hasDefaultValue) {
      document.l10n.setAttributes(button, "about-config-pref-delete");
    } else {
      document.l10n.setAttributes(button, "about-config-pref-reset");
      button.className = "button-reset";
    }
    buttonCell.appendChild(button);
  }
  rowFragment.appendChild(buttonCell);
  return rowFragment;
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
