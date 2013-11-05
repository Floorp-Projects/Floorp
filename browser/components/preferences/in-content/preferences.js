/* - This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this file,
   - You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
 
const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

addEventListener("DOMContentLoaded", function onLoad() {
  removeEventListener("DOMContentLoaded", onLoad);
  init_all();
});

function init_all() {
  document.documentElement.instantApply = true;
  gMainPane.init();
  gPrivacyPane.init();
  gAdvancedPane.init();
  gApplicationsPane.init();
  gContentPane.init();
  gSyncPane.init();
  gSecurityPane.init();
  var initFinished = document.createEvent("Event");
  initFinished.initEvent("Initialized", true, true);
  document.dispatchEvent(initFinished);

  let categories = document.getElementById("categories");
  categories.addEventListener("select", event => gotoPref(event.target.value));
  window.addEventListener("popstate", event => selectCategory(event.state));

  if (history.length > 1 && history.state) {
    updateCommands();
    selectCategory(history.state);
  } else {
    history.replaceState("paneGeneral", document.title);
  }
}

function selectCategory(name) {
  let categories = document.getElementById("categories");
  let item = categories.querySelector(".category[value=" + name + "]");
  categories.selectedItem = item;
}

function gotoPref(page) {
  if (history.state != page) {
    window.history.pushState(page, document.title);
  }

  updateCommands();
  search(page, "data-category");
}
 
function cmd_back() {
  window.history.back();
}
 
function cmd_forward() {
  window.history.forward();
}

function updateCommands() {
  document.getElementById("back-btn").disabled = !canGoBack();
  document.getElementById("forward-btn").disabled = !canGoForward();
}

function canGoBack() {
  return window.QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIWebNavigation)
               .canGoBack;
}

function canGoForward() {
  return window.QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIWebNavigation)
               .canGoForward;
}

function search(aQuery, aAttribute) {
  let elements = document.getElementById("mainPrefPane").children;
  for (let element of elements) {
    let attributeValue = element.getAttribute(aAttribute);
    element.hidden = (attributeValue != aQuery);
  }
}
