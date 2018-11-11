/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/Preferences.jsm");

let gPrefArray;

function onLoad() {
  gPrefArray = Services.prefs.getChildList("").map(function(name) {
    let pref = {
      name,
      value: Preferences.get(name),
      hasUserValue: Services.prefs.prefHasUserValue(name),
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

  let fragment = document.createDocumentFragment();
  for (let pref of gPrefArray) {
    let row = document.createElement("tr");
    if (pref.hasUserValue) {
      row.classList.add("has-user-value");
    }
    row.setAttribute("aria-label", pref.name);

    let nameCell = document.createElement("td");
    // Add <wbr> behind dots to prevent line breaking in random mid-word places.
    let parts = pref.name.split(".");
    for (let i = 0; i < parts.length - 1; i++) {
      nameCell.append(parts[i] + ".", document.createElement("wbr"));
    }
    nameCell.append(parts[parts.length - 1]);
    row.appendChild(nameCell);

    let valueCell = document.createElement("td");
    valueCell.classList.add("cell-value");
    valueCell.textContent = pref.value;
    row.appendChild(valueCell);

    fragment.appendChild(row);
  }
  document.getElementById("prefs").appendChild(fragment);
}
