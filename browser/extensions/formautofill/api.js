/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals ExtensionAPI */

const CACHED_STYLESHEETS = new WeakMap();

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "FormAutofill",
                               "resource://formautofill/FormAutofill.jsm");
ChromeUtils.defineModuleGetter(this, "formAutofillParent",
                               "resource://formautofill/FormAutofillParent.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "resProto",
                                   "@mozilla.org/network/protocol;1?name=resource",
                                   "nsISubstitutingProtocolHandler");

const RESOURCE_HOST = "formautofill";

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

  insertStyleSheet(domWindow, "chrome://formautofill/content/formautofill.css");
  insertStyleSheet(domWindow, "resource://formautofill/autocomplete-item-shared.css");
  insertStyleSheet(domWindow, "resource://formautofill/autocomplete-item.css");
}

function isAvailable() {
  let availablePref = Services.prefs.getCharPref("extensions.formautofill.available");
  if (availablePref == "on") {
    return true;
  } else if (availablePref == "detect") {
    let locale = Services.locale.requestedLocale;
    let region = Services.prefs.getCharPref("browser.search.region", "");
    let supportedCountries = Services.prefs.getCharPref("extensions.formautofill.supportedCountries")
                                           .split(",");
    if (!Services.prefs.getBoolPref("extensions.formautofill.supportRTL") &&
        Services.locale.isAppLocaleRTL) {
      return false;
    }
    return locale == "en-US" && supportedCountries.includes(region);
  }
  return false;
}

this.formautofill = class extends ExtensionAPI {
  onStartup() {
    // We have to do this before actually determining if we're enabled, since
    // there are scripts inside of the core browser code that depend on the
    // FormAutofill JSMs being registered.
    let uri = Services.io.newURI("chrome/res/", null, this.extension.rootURI);
    resProto.setSubstitution(RESOURCE_HOST, uri);

    let aomStartup = Cc["@mozilla.org/addons/addon-manager-startup;1"]
                                 .getService(Ci.amIAddonManagerStartup);
    const manifestURI = Services.io.newURI("manifest.json", null, this.extension.rootURI);
    this.chromeHandle = aomStartup.registerChrome(manifestURI, [
      ["content", "formautofill", "chrome/content/"],
    ]);

    // Until we move to fluent (bug 1446164), we're stuck with
    // chrome.manifest for handling localization since its what the
    // build system can handle for localized repacks.
    if (this.extension.rootURI instanceof Ci.nsIJARURI) {
      this.autofillManifest = this.extension.rootURI.JARFile
                                  .QueryInterface(Ci.nsIFileURL).file;
    } else if (this.extension.rootURI instanceof Ci.nsIFileURL) {
      this.autofillManifest = this.extension.rootURI.file;
    }

    if (this.autofillManifest) {
      Components.manager.addBootstrappedManifestLocation(this.autofillManifest);
    } else {
      Cu.reportError("Cannot find formautofill chrome.manifest for registring translated strings");
    }

    if (!isAvailable()) {
      Services.prefs.clearUserPref("dom.forms.autocomplete.formautofill");
      // reset the sync related prefs incase the feature was previously available
      // but isn't now.
      Services.prefs.clearUserPref("services.sync.engine.addresses.available");
      Services.prefs.clearUserPref("services.sync.engine.creditcards.available");
      Services.telemetry.scalarSet("formautofill.availability", false);
      return;
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
    if (FormAutofill.isAutofillCreditCardsAvailable) {
      Services.prefs.setBoolPref("services.sync.engine.creditcards.available", true);
    } else {
      Services.prefs.clearUserPref("services.sync.engine.creditcards.available");
    }

    // Listen for the autocomplete popup message to lazily append our stylesheet related to the popup.
    Services.mm.addMessageListener("FormAutoComplete:MaybeOpenPopup", onMaybeOpenPopup);

    formAutofillParent.init().catch(Cu.reportError);
    Services.mm.loadFrameScript("chrome://formautofill/content/FormAutofillFrameScript.js", true, true);
  }

  onShutdown(isAppShutdown) {
    if (isAppShutdown) {
      return;
    }

    resProto.setSubstitution(RESOURCE_HOST, null);

    this.chromeHandle.destruct();
    this.chromeHandle = null;

    if (this.autofillManifest) {
      Components.manager.removeBootstrappedManifestLocation(this.autofillManifest);
    }

    Services.mm.removeMessageListener("FormAutoComplete:MaybeOpenPopup", onMaybeOpenPopup);

    for (let win of Services.wm.getEnumerator("navigator:browser")) {
      let cachedStyleSheets = CACHED_STYLESHEETS.get(win);

      if (!cachedStyleSheets) {
        continue;
      }

      while (cachedStyleSheets.length !== 0) {
        cachedStyleSheets.pop().remove();
      }
    }
  }
};
