/* eslint-disable no-undef */
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
var { AppConstants } = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

function setBrowserDesign() {
  const browserDesign = document.getElementById("browserdesgin");
  if (browserDesign) {
    browserDesign.remove();
  }

  const floorpInterfaceNum = Services.prefs.getIntPref("floorp.browser.user.interface");
  const updateNumber = new Date().getTime();
  const themeCSS = {
    ProtonfixUI: `@import url(chrome://browser/skin/protonfix/protonfix.css?${updateNumber});`,
    LeptonUI: `@import url(chrome://browser/skin/lepton/leptonChrome.css?${updateNumber}); @import url(chrome://browser/skin/lepton/leptonContent.css?${updateNumber});`,
    LeptonUIMultitab: `@import url(chrome://browser/skin/lepton/photonChrome-multitab.css?${updateNumber});
                       @import url(chrome://browser/skin/lepton/photonContent-multitab.css?${updateNumber});`,
    fluentUI: `@import url(chrome://browser/skin/fluentUI/fluentUI.css);`,
    gnomeUI: `@import url(chrome://browser/skin/gnomeUI/gnomeUI.css);`,
    FluerialUI: `@import url(chrome://browser/skin/floorplegacy/test_legacy.css?${updateNumber});`,
    FluerialUIMultitab: `@import url(chrome://browser/skin/floorplegacy/test_legacy.css?${updateNumber}); @import url(chrome://browser/skin/floorplegacy/test_legacy_multitab.css);`
  };

  const tag = document.createElement('style');
  tag.setAttribute("id", "browserdesgin");
  const enableMultitab = Services.prefs.getIntPref("floorp.tabbar.style") == 1

  switch (floorpInterfaceNum) {
    case 1:
      break;
    case 3:
      tag.innerText = enableMultitab ? themeCSS.LeptonUIMultitab : themeCSS.LeptonUI;
      break;
    case 5:
      if (AppConstants.platform !== "linux") {
        tag.innerText = themeCSS.fluentUI;
      }
      break;
    case 6:
      if (AppConstants.platform == "linux") {
        tag.innerText = themeCSS.gnomeUI;
      }
      break;
    case 8:
      tag.innerText = enableMultitab ? themeCSS.FluerialUIMultitab : themeCSS.FluerialUI;
      break;
  }

  document.head.appendChild(tag);

  // recalculate sidebar width
  setTimeout(() => {
    gURLBar._updateLayoutBreakoutDimensions();
  }, 100);

  setTimeout(() => {
    gURLBar._updateLayoutBreakoutDimensions();
  }, 500);

  setTimeout(() => {
    setMultirowTabMaxHeight();
  }, 1000);

  if (floorpInterfaceNum == 3) {
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
  Services.obs.addObserver(setPhotonUI, "set-photon-ui");
  Services.obs.addObserver(setLeptonUI, "set-lepton-ui");
  Services.obs.addObserver(setProtonFixUI, "set-protonfix-ui");
}, { once: true });

function setPhotonUI() {
  Services.prefs.setIntPref("floorp.lepton.interface", 1);
  Services.prefs.setBoolPref("userChrome.tab.connect_to_window",          true);
  Services.prefs.setBoolPref("userChrome.tab.color_like_toolbar",         true);
  
  Services.prefs.setBoolPref("userChrome.tab.lepton_like_padding",       false);
  Services.prefs.setBoolPref("userChrome.tab.photon_like_padding",        true);
  
  Services.prefs.setBoolPref("userChrome.tab.dynamic_separator",         false);
  Services.prefs.setBoolPref("userChrome.tab.static_separator",           true);
  Services.prefs.setBoolPref("userChrome.tab.static_separator.selected_accent", false);
  Services.prefs.setBoolPref("userChrome.tab.bar_separator",             false);
  
  Services.prefs.setBoolPref("userChrome.tab.newtab_button_like_tab",    false);
  Services.prefs.setBoolPref("userChrome.tab.newtab_button_smaller",      true);
  Services.prefs.setBoolPref("userChrome.tab.newtab_button_proton",      false);
  
  Services.prefs.setBoolPref("userChrome.icon.panel_full",               false);
  Services.prefs.setBoolPref("userChrome.icon.panel_photon",             true);
  
  Services.prefs.setBoolPref("userChrome.tab.box_shadow",                false);
  Services.prefs.setBoolPref("userChrome.tab.bottom_rounded_corner",     false);
  
  Services.prefs.setBoolPref("userChrome.tab.photon_like_contextline",    true);
  Services.prefs.setBoolPref("userChrome.rounding.square_tab",            true);
  setBrowserDesign();
}

function setLeptonUI() {
  Services.prefs.setIntPref("floorp.lepton.interface", 2);
  Services.prefs.setBoolPref("userChrome.tab.connect_to_window",          true);
  Services.prefs.setBoolPref("userChrome.tab.color_like_toolbar",         true);
  
  Services.prefs.setBoolPref("userChrome.tab.lepton_like_padding",        true);
  Services.prefs.setBoolPref("userChrome.tab.photon_like_padding",       false);
  
  Services.prefs.setBoolPref("userChrome.tab.dynamic_separator",          true);
  Services.prefs.setBoolPref("userChrome.tab.static_separator",          false);
  Services.prefs.setBoolPref("userChrome.tab.static_separator.selected_accent", false);
  Services.prefs.setBoolPref("userChrome.tab.bar_separator",             false);
  
  Services.prefs.setBoolPref("userChrome.tab.newtab_button_like_tab",     true);
  Services.prefs.setBoolPref("userChrome.tab.newtab_button_smaller",     false);
  Services.prefs.setBoolPref("userChrome.tab.newtab_button_proton",      false);
  
  Services.prefs.setBoolPref("userChrome.icon.panel_full",                true);
  Services.prefs.setBoolPref("userChrome.icon.panel_photon",             false);
  
  Services.prefs.setBoolPref("userChrome.tab.box_shadow",                 false);
  Services.prefs.setBoolPref("userChrome.tab.bottom_rounded_corner",      true);
  
  Services.prefs.setBoolPref("userChrome.tab.photon_like_contextline",   false);
  Services.prefs.setBoolPref("userChrome.rounding.square_tab",           false);
  setBrowserDesign();
}

function setProtonFixUI() {
  Services.prefs.setIntPref("floorp.lepton.interface", 3);

  Services.prefs.setBoolPref("userChrome.tab.connect_to_window",         false);
  Services.prefs.setBoolPref("userChrome.tab.color_like_toolbar",        false);
  
  Services.prefs.setBoolPref("userChrome.tab.lepton_like_padding",       false);
  Services.prefs.setBoolPref("userChrome.tab.photon_like_padding",       false);
  
  Services.prefs.setBoolPref("userChrome.tab.dynamic_separator",          true);
  Services.prefs.setBoolPref("userChrome.tab.static_separator",          false);
  Services.prefs.setBoolPref("userChrome.tab.static_separator.selected_accent", false);
  Services.prefs.setBoolPref("userChrome.tab.bar_separator",             false);
  
  Services.prefs.setBoolPref("userChrome.tab.newtab_button_like_tab",    false);
  Services.prefs.setBoolPref("userChrome.tab.newtab_button_smaller",     false);
  Services.prefs.setBoolPref("userChrome.tab.newtab_button_proton",       true);
  
  Services.prefs.setBoolPref("userChrome.icon.panel_full",                true);
  Services.prefs.setBoolPref("userChrome.icon.panel_photon",             false);
  
  Services.prefs.setBoolPref("userChrome.tab.box_shadow",                false);
  Services.prefs.setBoolPref("userChrome.tab.bottom_rounded_corner",     false);
  
  Services.prefs.setBoolPref("userChrome.tab.photon_like_contextline",   false);
  Services.prefs.setBoolPref("userChrome.rounding.square_tab",           false);

  setBrowserDesign();
}