/* eslint-disable mozilla/no-useless-parameters */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from extensionControlled.js */
/* import-globals-from preferences.js */

var { AppConstants } = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
XPCOMUtils.defineLazyGetter(this, "L10n", () => {
  return new Localization([
    "branding/brand.ftl",
    "browser/floorp",
  ]);
});


Preferences.addAll([
   //select lepton mode ( Obsert the below 1 or 2 Mode will Select Lepton or Photon )
   { id: "floorp.lepton.interface",                       type : "int"},

   //auto hide
   { id: "userChrome.autohide.tab",                       type : "bool"},
   { id: "userChrome.autohide.tabbar",                    type : "bool"},
   { id: "userChrome.autohide.navbar",                    type : "bool"},
   { id: "userChrome.autohide.bookmarkbar",               type : "bool"},
   { id: "userChrome.autohide.sidebar",                   type : "bool"},
   { id: "userChrome.autohide.fill_urlbar",               type : "bool"},
   { id: "userChrome.autohide.back_button",               type : "bool"},
   { id: "userChrome.autohide.forward_button",            type : "bool"},
   { id: "userChrome.autohide.page_action",               type : "bool"},
   { id: "userChrome.autohide.toolbar_overlap",           type : "bool"},
   { id: "userChrome.autohide.toolbar_overlap.allow_layout_shift",type : "bool"},

   // hide Elements
   { id: "userChrome.hidden.tab_icon",                    type : "bool"},
   { id: "userChrome.hidden.tabbar",                      type : "bool"},
   { id: "userChrome.hidden.navbar",                      type : "bool"},
   { id: "userChrome.hidden.sidebar_header",              type : "bool"},
   { id: "userChrome.hidden.urlbar_iconbox",              type : "bool"},
   { id: "userChrome.hidden.bookmarkbar_icon",            type : "bool"},
   { id: "userChrome.hidden.bookmarkbar_label",           type : "bool"},
   { id: "userChrome.hidden.disabled_menu",               type : "bool"},
   { id: "userChrome.icon.disabled",                      type : "bool"},
   { id: "userChrome.icon.menu",                          type : "bool"},

   { id: "userChrome.centered.tab",                       type : "bool"},
   { id: "userChrome.centered.tab.label",                 type : "bool"},
   { id: "userChrome.centered.urlbar",                    type : "bool"},
   { id: "userChrome.centered.bookmarkbar",               type : "bool"},

   { id: "userChrome.urlView.move_icon_to_left",          type : "bool"},
   { id: "userChrome.urlView.go_button_when_typing",      type : "bool"},
   { id: "userChrome.urlView.always_show_page_actions",   type : "bool"},

   { id: "userChrome.tabbar.as_titlebar",                 type : "bool"},
   { id: "userChrome.tabbar.one_liner",                   type : "bool"},

   { id: "userChrome.sidebar.overlap",                    type : "bool"},
]);

var gLeptonPane = {
  _pane: null,

  // called when the document is first parsed
  init() {
    this._pane = document.getElementById("paneLepton");

document.getElementById("backtogeneral").addEventListener("command", function(){gotoPref("design");});
  document.getElementById("lepton-design-mode").addEventListener("click", setLeptonUI, false);
  document.getElementById("photon-design-mode").addEventListener("click", setPhotonUI, false);
  document.getElementById("protonfix-design-mode").addEventListener("click", setProtonFixUI, false);
  let targets = document.getElementsByClassName("photonCheckbox");
  for(let i = 0; i < targets.length; i++){
    targets[i].addEventListener("click",() => {
      Services.obs.notifyObservers({}, "update-photon-pref");
    }, false);
  }
  },
};


function setPhotonUI() {
  Services.obs.notifyObservers({}, "set-photon-ui");
}

function setLeptonUI() {
  Services.obs.notifyObservers({}, "set-lepton-ui");
}

function setProtonFixUI() {
  Services.obs.notifyObservers({}, "set-protonfix-ui");
}