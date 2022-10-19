/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from extensionControlled.js */
/* import-globals-from preferences.js */

var { AppConstants } = ChromeUtils.import( "resource://gre/modules/AppConstants.jsm");
XPCOMUtils.defineLazyGetter(this, "L10n", () => {
  return new Localization([
    "branding/brand.ftl",
    "browser/floorp",
  ]);
});



var gLeptonPane = {
  _pane: null,

  // called when the document is first parsed
  init() {
    this._pane = document.getElementById("paneLepton");
  },
};


function setPhotonUI() {
  Services.prefs.setIntPref("floorp.lepton.interface", 1);
  Services.prefs.setBoolPref("userChrome.icon.panel_full", false);
  Services.prefs.setBoolPref("userChrome.tab.bottom_rounded_corner", false);
  Services.prefs.setBoolPref("userChrome.tab.dynamic_separator" ,false);
  Services.prefs.setBoolPref("userChrome.tab.lepton_like_padding" ,false);
  Services.prefs.setBoolPref("userChrome.icon.panel_photon", true);	
  Services.prefs.setBoolPref("userChrome.rounding.square_tab", true);
  Services.prefs.setBoolPref("userChrome.tab.photon_like_contextline",true);
  Services.prefs.setBoolPref("userChrome.tab.photon_like_padding",true);
  Services.prefs.setBoolPref("userChrome.tab.static_separator", true);
}

function setLeptonUI() {
  Services.prefs.setIntPref("floorp.lepton.interface", 2);
  Services.prefs.setBoolPref("userChrome.tab.bottom_rounded_corner", true);
  Services.prefs.setBoolPref("userChrome.tab.dynamic_separator" ,true);
  Services.prefs.setBoolPref("userChrome.tab.lepton_like_padding" ,true);
  Services.prefs.setBoolPref("userChrome.icon.panel_photon", false);	
  Services.prefs.setBoolPref("userChrome.rounding.square_tab", false);
  Services.prefs.setBoolPref("userChrome.tab.photon_like_contextline",false);
  Services.prefs.setBoolPref("userChrome.tab.photon_like_padding",false);
  Services.prefs.setBoolPref("userChrome.tab.static_separator", false);
}

window.setTimeout(function(){
  document.getElementById("lepton").addEventListener("command", setLeptonUI);
  document.getElementById("photon").addEventListener("command", setPhotonUI);
}, 1000);