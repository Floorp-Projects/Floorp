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
    LeptonUI: `@import url(chrome://browser/skin/lepton/leptonChrome.css?${updateNumber}); @import url(chrome://browser/skin/lepton/leptonContent.css?${updateNumber});`,
    LeptonUIMultitab: `@import url(chrome://browser/skin/lepton/photonChrome-multitab.css?${updateNumber});
                       @import url(chrome://browser/skin/lepton/photonContent-multitab.css?${updateNumber});`,
    fluentUI: `@import url(chrome://browser/skin/fluentUI/fluentUI.css);`,
    gnomeUI: `@import url(chrome://browser/skin/gnomeUI/gnomeUI.css);`,
    FluerialUI: `@import url(chrome://browser/skin/floorplegacy/test_legacy.css?${updateNumber});`,
    FluerialUIMultitab:`@import url(chrome://browser/skin/floorplegacy/test_legacy.css?${updateNumber});
                        @import url(chrome://browser/skin/floorplegacy/test_legacy_multitab.css);`
  }
  var Tag = document.createElement('style');
  Tag.setAttribute("id", "browserdesgin");
  switch (floorpinterfacenum) {
    //ProtonUI 
    case 1:
      break;
    case 3:
      Tag.innerText = Services.prefs.getBoolPref("floorp.enable.multitab", false) ? ThemeCSS.LeptonUIMultitab : ThemeCSS.LeptonUI;
      break;
     //4 is deleted at v11.0.0 because it is MaterialUI.
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

  if (floorpinterfacenum == 3) {
    loadStyleSheetWithNsStyleSheetService("chrome://browser/skin/lepton/leptonContent.css");
  } else {
    unloadStyleSheetWithNsStyleSheetService("chrome://browser/skin/lepton/leptonContent.css");
  }
}

document.addEventListener("DOMContentLoaded", () => {
  setBrowserDesign();
  Services.prefs.addObserver("floorp.browser.user.interface", setBrowserDesign);
  Services.prefs.addObserver("floorp.fluerial.roundVerticalTabs", setBrowserDesign);
  Services.obs.addObserver(setBrowserDesign, "update-photon-pref");
}, { once: true });
