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
    let listItem = document.createElement("li");
    listItem.textContent = pref.name + " || " +
      (pref.hasUserValue ? "Modified" : "Default") + " || " +
      pref.value.constructor.name + " || " + pref.value;
    fragment.appendChild(listItem);
  }
  document.getElementById("list").appendChild(fragment);
}
