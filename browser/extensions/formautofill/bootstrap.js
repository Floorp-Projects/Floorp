/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported startup, shutdown, install, uninstall */

const {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;
const STYLESHEET_URI = "chrome://formautofill/content/formautofill.css";
const CACHED_STYLESHEETS = new WeakMap();

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager", "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AddonManagerPrivate",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "formAutofillParent",
                                  "resource://formautofill/FormAutofillParent.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FormAutofillUtils",
                                  "resource://formautofill/FormAutofillUtils.jsm");

function insertStyleSheet(domWindow, url) {
  let doc = domWindow.document;
  let styleSheetAttr = `href="${url}" type="text/css"`;
  let styleSheet = doc.createProcessingInstruction("xml-stylesheet", styleSheetAttr);

  doc.insertBefore(styleSheet, doc.documentElement);

  if (CACHED_STYLESHEETS.has(domWindow)) {
    CACHED_STYLESHEETS.get(domWindow).push(styleSheet);
  } else {
    CACHED_STYLESHEETS.set(domWindow, [styleSheet]);
  }
}

function onMaybeOpenPopup(evt) {
  let domWindow = evt.target.ownerGlobal;
  if (CACHED_STYLESHEETS.has(domWindow)) {
    // This window already has autofill stylesheets.
    return;
  }

  insertStyleSheet(domWindow, STYLESHEET_URI);
}

function addUpgradeListener(instanceID) {
  AddonManager.addUpgradeListener(instanceID, upgrade => {
    // don't install the upgrade by doing nothing here.
    // The upgrade will be installed upon next restart.
  });
}

function isAvailable() {
  let availablePref = Services.prefs.getCharPref("extensions.formautofill.available");
  if (availablePref == "on") {
    return true;
  } else if (availablePref == "detect") {
    let region = Services.prefs.getCharPref("browser.search.region", "");
    let supportedCountries = Services.prefs.getCharPref("extensions.formautofill.supportedCountries")
                                           .split(",");
    if (!Services.prefs.getBoolPref("extensions.formautofill.supportRTL") &&
        Services.locale.isAppLocaleRTL) {
      return false;
    }
    return supportedCountries.includes(region);
  }
  return false;
}

function startup(data) {
  if (!isAvailable()) {
    Services.prefs.clearUserPref("dom.forms.autocomplete.formautofill");
    // reset the sync related prefs incase the feature was previously available
    // but isn't now.
    Services.prefs.clearUserPref("services.sync.engine.addresses.available");
    Services.prefs.clearUserPref("services.sync.engine.creditcards.available");
    Services.telemetry.scalarSet("formautofill.availability", false);
    return;
  }

  if (data.hasOwnProperty("instanceID") && data.instanceID) {
    if (AddonManagerPrivate.isDBLoaded()) {
      addUpgradeListener(data.instanceID);
    } else {
      // Wait for the extension database to be loaded so we don't cause its init.
      Services.obs.addObserver(function xpiDatabaseLoaded() {
        Services.obs.removeObserver(xpiDatabaseLoaded, "xpi-database-loaded");
        addUpgradeListener(data.instanceID);
      }, "xpi-database-loaded");
    }
  } else {
    throw Error("no instanceID passed to bootstrap startup");
  }

  // This pref is used for web contents to detect the autocomplete feature.
  // When it's true, "element.autocomplete" will return tokens we currently
  // support -- otherwise it'll return an empty string.
  Services.prefs.setBoolPref("dom.forms.autocomplete.formautofill", true);
  Services.telemetry.scalarSet("formautofill.availability", true);

  // This pref determines whether the "addresses"/"creditcards" sync engine is
  // available (ie, whether it is shown in any UI etc) - it *does not* determine
  // whether the engine is actually enabled or not.
  Services.prefs.setBoolPref("services.sync.engine.addresses.available", true);
  if (FormAutofillUtils.isAutofillCreditCardsAvailable) {
    Services.prefs.setBoolPref("services.sync.engine.creditcards.available", true);
  } else {
    Services.prefs.clearUserPref("services.sync.engine.creditcards.available");
  }

  // Listen for the autocomplete popup message to lazily append our stylesheet related to the popup.
  Services.mm.addMessageListener("FormAutoComplete:MaybeOpenPopup", onMaybeOpenPopup);

  formAutofillParent.init().catch(Cu.reportError);
  Services.ppmm.loadProcessScript("data:,new " + function() {
    Components.utils.import("resource://formautofill/FormAutofillContent.jsm");
  }, true);
  Services.mm.loadFrameScript("chrome://formautofill/content/FormAutofillFrameScript.js", true);
}

function shutdown() {
  Services.mm.removeMessageListener("FormAutoComplete:MaybeOpenPopup", onMaybeOpenPopup);

  let enumerator = Services.wm.getEnumerator("navigator:browser");
  while (enumerator.hasMoreElements()) {
    let win = enumerator.getNext();
    let domWindow = win.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindow);
    let cachedStyleSheets = CACHED_STYLESHEETS.get(domWindow);

    if (!cachedStyleSheets) {
      continue;
    }

    while (cachedStyleSheets.length !== 0) {
      cachedStyleSheets.pop().remove();
    }
  }
}

function install() {}
function uninstall() {}
