/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cu = Components.utils;
const {Services} = Cu.import("resource://gre/modules/Services.jsm");

window.addEventListener("load", function onLoad() {
  window.removeEventListener("load", onLoad);

  // Listen to preference changes
  let inputs = document.querySelectorAll("[data-pref]");
  for (let i of inputs) {
    let pref = i.dataset.pref;
    Services.prefs.addObserver(pref, FillForm, false);
    i.addEventListener("change", SaveForm, false);
  }

  // Buttons
  document.querySelector("#close").onclick = CloseUI;
  document.querySelector("#restore").onclick = RestoreDefaults;
  document.querySelector("#manageComponents").onclick = ShowAddons;

  // Initialize the controls
  FillForm();

}, true);

window.addEventListener("unload", function onUnload() {
  window.removeEventListener("unload", onUnload);
  let inputs = document.querySelectorAll("[data-pref]");
  for (let i of inputs) {
    let pref = i.dataset.pref;
    i.removeEventListener("change", SaveForm, false);
    Services.prefs.removeObserver(pref, FillForm, false);
  }
}, true);

function CloseUI() {
  window.parent.UI.openProject();
}

function ShowAddons() {
  window.parent.Cmds.showAddons();
}

function FillForm() {
  let inputs = document.querySelectorAll("[data-pref]");
  for (let i of inputs) {
    let pref = i.dataset.pref;
    let val = GetPref(pref);
    if (i.type == "checkbox") {
      i.checked = val;
    } else {
      i.value = val;
    }
  }
}

function SaveForm(e) {
  let inputs = document.querySelectorAll("[data-pref]");
  for (let i of inputs) {
    let pref = i.dataset.pref;
    if (i.type == "checkbox") {
      SetPref(pref, i.checked);
    } else {
      SetPref(pref, i.value);
    }
  }
}

function GetPref(name) {
  let type = Services.prefs.getPrefType(name);
  switch (type) {
    case Services.prefs.PREF_STRING:
      return Services.prefs.getCharPref(name);
    case Services.prefs.PREF_INT:
      return Services.prefs.getIntPref(name);
    case Services.prefs.PREF_BOOL:
      return Services.prefs.getBoolPref(name);
    default:
      throw new Error("Unknown type");
  }
}

function SetPref(name, value) {
  let type = Services.prefs.getPrefType(name);
  switch (type) {
    case Services.prefs.PREF_STRING:
      return Services.prefs.setCharPref(name, value);
    case Services.prefs.PREF_INT:
      return Services.prefs.setIntPref(name, value);
    case Services.prefs.PREF_BOOL:
      return Services.prefs.setBoolPref(name, value);
    default:
      throw new Error("Unknown type");
  }
}

function RestoreDefaults() {
  let inputs = document.querySelectorAll("[data-pref]");
  for (let i of inputs) {
    let pref = i.dataset.pref;
    Services.prefs.clearUserPref(pref);
  }
}
