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

let gLastHash = "";

let gCategoryInits = new Map();
function init_category_if_required(category) {
  let categoryInfo = gCategoryInits.get(category);
  if (!categoryInfo) {
    throw "Unknown in-content prefs category! Can't init " + category;
  }
  if (categoryInfo.inited) {
    return;
  }
  categoryInfo.init();
}

function register_module(categoryName, categoryObject) {
  gCategoryInits.set(categoryName, {
    inited: false,
    init: function() {
      categoryObject.init();
      this.inited = true;
    }
  });
}

addEventListener("DOMContentLoaded", function onLoad() {
  removeEventListener("DOMContentLoaded", onLoad);
  init_all();
});

function init_all() {
  document.documentElement.instantApply = true;

  gSubDialog.init();
  register_module("paneGeneral", gMainPane);
  register_module("paneSearch", gSearchPane);
  register_module("panePrivacy", gPrivacyPane);
  register_module("paneAdvanced", gAdvancedPane);
  register_module("paneApplications", gApplicationsPane);
  register_module("paneContent", gContentPane);
  register_module("paneSync", gSyncPane);
  register_module("paneSecurity", gSecurityPane);

  let categories = document.getElementById("categories");
  categories.addEventListener("select", event => gotoPref(event.target.value));

  document.documentElement.addEventListener("keydown", function(event) {
    if (event.keyCode == KeyEvent.DOM_VK_TAB) {
      categories.setAttribute("keyboard-navigation", "true");
    }
  });
  categories.addEventListener("mousedown", function() {
    this.removeAttribute("keyboard-navigation");
  });

  window.addEventListener("hashchange", onHashChange);
  gotoPref();

  var initFinished = new CustomEvent("Initialized", {
    'bubbles': true,
    'cancelable': true
  });
  document.dispatchEvent(initFinished);

  let helpCmd = document.getElementById("help-button");
  helpCmd.addEventListener("command", helpButtonCommand);

  // Wait until initialization of all preferences are complete before
  // notifying observers that the UI is now ready.
  Services.obs.notifyObservers(window, "advanced-pane-loaded", null);
}

window.addEventListener("unload", function onUnload() {
  gSubDialog.uninit();
});

function onHashChange() {
  gotoPref();
}

function gotoPref(aCategory) {
  let categories = document.getElementById("categories");
  const kDefaultCategoryInternalName = categories.firstElementChild.value;
  let hash = document.location.hash;
  let category = aCategory || hash.substr(1) || kDefaultCategoryInternalName;
  category = friendlyPrefCategoryNameToInternalName(category);

  // Updating the hash (below) or changing the selected category
  // will re-enter gotoPref.
  if (gLastHash == category)
    return;
  let item = categories.querySelector(".category[value=" + category + "]");
  if (!item) {
    category = kDefaultCategoryInternalName;
    item = categories.querySelector(".category[value=" + category + "]");
  }

  try {
    init_category_if_required(category);
  } catch (ex) {
    Cu.reportError("Error initializing preference category " + category + ": " + ex);
    throw ex;
  }

  let newHash = internalPrefCategoryNameToFriendlyName(category);
  if (gLastHash || category != kDefaultCategoryInternalName) {
    document.location.hash = newHash;
  }
  // Need to set the gLastHash before setting categories.selectedItem since
  // the categories 'select' event will re-enter the gotoPref codepath.
  gLastHash = category;
  categories.selectedItem = item;
  window.history.replaceState(category, document.title);
  search(category, "data-category");
  let mainContent = document.querySelector(".main-content");
  mainContent.scrollTop = 0;
}

function search(aQuery, aAttribute) {
  let elements = document.getElementById("mainPrefPane").children;
  for (let element of elements) {
    let attributeValue = element.getAttribute(aAttribute);
    element.hidden = (attributeValue != aQuery);
  }
}

function helpButtonCommand() {
  let pane = history.state;
  let categories = document.getElementById("categories");
  let helpTopic = categories.querySelector(".category[value=" + pane + "]")
                            .getAttribute("helpTopic");
  openHelpLink(helpTopic);
}

function friendlyPrefCategoryNameToInternalName(aName) {
  if (aName.startsWith("pane"))
    return aName;
  return "pane" + aName.substring(0,1).toUpperCase() + aName.substr(1);
}

// This function is duplicated inside of utilityOverlay.js's openPreferences.
function internalPrefCategoryNameToFriendlyName(aName) {
  return (aName || "").replace(/^pane./, function(toReplace) { return toReplace[4].toLowerCase(); });
}
