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

function init_all() {
  document.documentElement.instantApply = true;
  window.history.replaceState("landing", document.title);
  window.addEventListener("popstate", onStatePopped, true);
  updateCommands();
  gMainPane.init();
#ifdef XP_WIN
  gTabsPane.init();
#endif
  gPrivacyPane.init();
  gAdvancedPane.init();
  gApplicationsPane.init();
  gContentPane.init();
  gSyncPane.init();
  gSecurityPane.init();
  var initFinished = document.createEvent("Event");
  initFinished.initEvent("Initialized", true, true);
  document.dispatchEvent(initFinished);
}

function gotoPref(page) {
  search(page, "data-category");
  window.history.pushState(page, document.title);
  updateCommands();
}
 
function cmd_back() {
  window.history.back();
}
 
function cmd_forward() {
  window.history.forward();
}

function onStatePopped(aEvent) {
  updateCommands();
  search(aEvent.state, "data-category");
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
