/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
var { AppConstants } = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

function setBrowserDesign() {
  document.getElementById("browserdesgin")?.remove();
  let floorpinterfacenum = Services.prefs.getIntPref("floorp.browser.user.interface");
  let updateNumber = (new Date()).getTime();
  const ThemeCSS = {
    ProtonfixUI: `@import url(chrome://browser/skin/protonfix/protonfix.css?${updateNumber});`,
    LeptonUI: `@import url(chrome://browser/skin/lepton/userChrome.css?${updateNumber});`,
    LeptonUIMultitab: `@import url(chrome://browser/skin/lepton/photonChrome-multitab.css?${updateNumber});
                       @import url(chrome://browser/skin/lepton/photonContent-multitab.css?${updateNumber});`,
    MaterialUI: `@import url(chrome://browser/skin/floorplegacy/floorplegacy.css);`,
    MaterialUIMultitab: `@import url(chrome://browser/skin/floorplegacy/floorplegacy.css);
                         .tabbrowser-tab { margin-top: 0.7em !important;  position: relative !important;  top: -0.34em !important; }`,
    fluentUI: `@import url(chrome://browser/skin/fluentUI/fluentUI.css);`,
    gnomeUI: `@import url(chrome://browser/skin/gnomeUI/gnomeUI.css);`,
    FluerialUI: `@import url(chrome://browser/skin/floorplegacy/test_legacy.css);`,
    FluerialUIMultitab:`@import url(chrome://browser/skin/floorplegacy/test_legacy.css);
                        @import url(chrome://browser/skin/floorplegacy/test_legacy_multitab.css);`
  }
  var Tag = document.createElement('style');
  Tag.setAttribute("id", "browserdesgin");
  switch (floorpinterfacenum) {
    //ProtonUI 
    case 1:
      break;
    case 2:
      Tag.innerText = ThemeCSS.ProtonfixUI;
      break;
    case 3:
      Tag.innerText = Services.prefs.getBoolPref("floorp.enable.multitab", false) ? ThemeCSS.LeptonUIMultitab : ThemeCSS.LeptonUI;
      break;
    case 4:
      Tag.innerText = Services.prefs.getBoolPref("floorp.enable.multitab", false) ? ThemeCSS.MaterialUIMultitab : ThemeCSS.MaterialUI;
      break;
    case 5:
      if (AppConstants.platform != "linux") Tag.innerText = ThemeCSS.fluentUI;
      break;
    case 6:
      if (AppConstants.platform == "linux") Tag.innerText = ThemeCSS.gnomeUI;
      break;
    case 8: 
      Tag.innerText = Services.prefs.getBoolPref("floorp.enable.multitab", false) ? ThemeCSS.FluerialUIMultitab : ThemeCSS.FluerialUI;
      break;
  }
  document.head.appendChild(Tag);
  // re-calculate urlbar height
  setTimeout(function () {
    gURLBar._updateLayoutBreakoutDimensions();
  }, 100);
  setTimeout(function () {
    gURLBar._updateLayoutBreakoutDimensions();
  }, 500);
  setTimeout(function () {
    setMultirowTabMaxHeight();
  }, 1000);
}

document.addEventListener("DOMContentLoaded", () => {
  setBrowserDesign();
  Services.prefs.addObserver("floorp.browser.user.interface", setBrowserDesign);
  Services.obs.addObserver(setBrowserDesign, "update-photon-pref");
}, { once: true });
