/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from preferences.js */

Preferences.addAll([
  { id: "floorp.browser.tabbar.settings", type: "int" },
  { id: "floorp.browser.user.interface", type: "int" },
  { id: "floorp.bookmarks.bar.focus.mode", type: "bool" },
  { id: "floorp.bookmarks.fakestatus.mode", type: "bool" },
  { id: "floorp.navbar.bottom", type: "bool" },
  { id: "floorp.enable.dualtheme", type: "bool" },
  { id: "floorp.material.effect.enable", type: "bool" },
  { id: "floorp.delete.browser.border", type: "bool" },
  { id: "floorp.chrome.theme.mode", type: "int" },
  { id: "floorp.tabbar.style", type: "int" },
  { id: "floorp.browser.tabbar.multirow.max.enabled", type: "bool"},
  { id: "floorp.browser.tabbar.multirow.newtab-inside.enabled", type: "bool"},
  { id: "floorp.verticaltab.hover.enabled", type: "bool" },
  { id: "floorp.titlebar.favicon.color", type: "bool" },
  { id: "floorp.Tree-type.verticaltab.optimization", type: "bool" },
])
var gDesign = {
  _pane: null,
  init() {
    this._pane = document.getElementById("paneDesign");
    document.getElementById("leptonButton").addEventListener("click", function () {
      window.location.href = "about:preferences#lepton";
    });

    let disableMultirowPref = () => {
      let elems = document.getElementsByClassName("multiRowTabs")
      for (let i = 0; i < elems.length; i++) {
        elems[i].disabled = Services.prefs.getIntPref("floorp.tabbar.style", 0) != 1;
      }
      elems = document.getElementsByClassName("verticalTabs")
      for (let i = 0; i < elems.length; i++) {
        elems[i].disabled = Services.prefs.getIntPref("floorp.tabbar.style", 0) != 2;
      }
    }
    disableMultirowPref()
    Services.prefs.addObserver("floorp.tabbar.style",disableMultirowPref);

    {
      let prefName = "floorp.browser.tabbar.multirow.max.row";
      let elem = document.getElementById("MultirowValue");
      elem.value = Services.prefs.getIntPref(prefName, undefined);
      elem.addEventListener('change', function () {
        Services.prefs.setIntPref(prefName, Number(elem.value));
      });
      Services.prefs.addObserver(prefName, function () {
        elem.value = Services.prefs.getIntPref(prefName, undefined);
      });
    }

    document.getElementById("leptonButton").addEventListener("click", function () {
      window.location.href = "about:preferences#lepton";
    });

    document
        .getElementById("colors")
        .addEventListener("command", gMainPane.configureColors.bind(gMainPane));
        AppearanceChooser.init();
  },
};

const TSTStatus = async (addonID, className) => {
  const addon = await AddonManager.getAddonByID(addonID);
  if (addon !== null) {
    const addontag = document.createElement("style");
    addontag.setAttribute("id", className);
    addontag.innerText = `
      .${className} {
        display: none !important;
      }
      `;
    document.getElementsByTagName("head")[0].insertAdjacentElement("beforeend", addontag);
  }
};

TSTStatus("treestyletab@piro.sakura.ne.jp", "TST");
