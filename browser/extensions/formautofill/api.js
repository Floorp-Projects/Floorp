/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals ExtensionAPI, Services, XPCOMUtils */

const CACHED_STYLESHEETS = new WeakMap();

ChromeUtils.defineModuleGetter(
  this,
  "FormAutofill",
  "resource://autofill/FormAutofill.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "FormAutofillStatus",
  "resource://autofill/FormAutofillParent.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "FormAutofillParent",
  "resource://autofill/FormAutofillParent.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "AutoCompleteParent",
  "resource://gre/actors/AutoCompleteParent.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "resProto",
  "@mozilla.org/network/protocol;1?name=resource",
  "nsISubstitutingProtocolHandler"
);

const RESOURCE_HOST = "formautofill";

function insertStyleSheet(domWindow, url) {
  let doc = domWindow.document;
  let styleSheetAttr = `href="${url}" type="text/css"`;
  let styleSheet = doc.createProcessingInstruction(
    "xml-stylesheet",
    styleSheetAttr
  );

  doc.insertBefore(styleSheet, doc.documentElement);

  if (CACHED_STYLESHEETS.has(domWindow)) {
    CACHED_STYLESHEETS.get(domWindow).push(styleSheet);
  } else {
    CACHED_STYLESHEETS.set(domWindow, [styleSheet]);
  }
}

function ensureCssLoaded(domWindow) {
  if (CACHED_STYLESHEETS.has(domWindow)) {
    // This window already has autofill stylesheets.
    return;
  }

  insertStyleSheet(domWindow, "chrome://formautofill/content/formautofill.css");
  insertStyleSheet(
    domWindow,
    "chrome://formautofill/content/skin/autocomplete-item-shared.css"
  );
  insertStyleSheet(
    domWindow,
    "chrome://formautofill/content/skin/autocomplete-item.css"
  );
}

function isAvailable() {
  let availablePref = Services.prefs.getCharPref(
    "extensions.formautofill.available"
  );
  if (availablePref == "on") {
    return true;
  } else if (availablePref == "detect") {
    let region = Services.prefs.getCharPref("browser.search.region", "");
    let supportedCountries = Services.prefs
      .getCharPref("extensions.formautofill.supportedCountries")
      .split(",");
    if (
      !Services.prefs.getBoolPref("extensions.formautofill.supportRTL") &&
      Services.locale.isAppLocaleRTL
    ) {
      return false;
    }
    return supportedCountries.includes(region);
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

    let aomStartup = Cc[
      "@mozilla.org/addons/addon-manager-startup;1"
    ].getService(Ci.amIAddonManagerStartup);
    const manifestURI = Services.io.newURI(
      "manifest.json",
      null,
      this.extension.rootURI
    );
    this.chromeHandle = aomStartup.registerChrome(manifestURI, [
      ["content", "formautofill", "chrome/content/"],
    ]);

    // Until we move to fluent (bug 1446164), we're stuck with
    // chrome.manifest for handling localization since its what the
    // build system can handle for localized repacks.
    if (this.extension.rootURI instanceof Ci.nsIJARURI) {
      this.autofillManifest = this.extension.rootURI.JARFile.QueryInterface(
        Ci.nsIFileURL
      ).file;
    } else if (this.extension.rootURI instanceof Ci.nsIFileURL) {
      this.autofillManifest = this.extension.rootURI.file;
    }

    if (this.autofillManifest) {
      Components.manager.addBootstrappedManifestLocation(this.autofillManifest);
    } else {
      Cu.reportError(
        "Cannot find formautofill chrome.manifest for registring translated strings"
      );
    }

    if (!isAvailable()) {
      Services.prefs.clearUserPref("dom.forms.autocomplete.formautofill");
      // reset the sync related prefs incase the feature was previously available
      // but isn't now.
      Services.prefs.clearUserPref("services.sync.engine.addresses.available");
      Services.prefs.clearUserPref(
        "services.sync.engine.creditcards.available"
      );
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
    Services.prefs.setBoolPref(
      "services.sync.engine.addresses.available",
      true
    );
    if (FormAutofill.isAutofillCreditCardsAvailable) {
      Services.prefs.setBoolPref(
        "services.sync.engine.creditcards.available",
        true
      );
    } else {
      Services.prefs.clearUserPref(
        "services.sync.engine.creditcards.available"
      );
    }

    // Listen for the autocomplete popup message
    // or the form submitted message (which may trigger a
    // doorhanger) to lazily append our stylesheets related
    // to the autocomplete feature.
    AutoCompleteParent.addPopupStateListener(ensureCssLoaded);
    FormAutofillParent.addMessageObserver(this);
    this.onFormSubmitted = (data, window) => ensureCssLoaded(window);

    FormAutofillStatus.init();

    ChromeUtils.registerWindowActor("FormAutofill", {
      parent: {
        moduleURI: "resource://autofill/FormAutofillParent.jsm",
      },
      child: {
        moduleURI: "resource://autofill/FormAutofillChild.jsm",
        events: {
          focusin: {},
          DOMFormBeforeSubmit: {},
        },
      },
      allFrames: true,
    });
  }

  onShutdown(isAppShutdown) {
    if (isAppShutdown) {
      return;
    }

    resProto.setSubstitution(RESOURCE_HOST, null);

    this.chromeHandle.destruct();
    this.chromeHandle = null;

    if (this.autofillManifest) {
      Components.manager.removeBootstrappedManifestLocation(
        this.autofillManifest
      );
    }

    ChromeUtils.unregisterWindowActor("FormAutofill");

    AutoCompleteParent.removePopupStateListener(ensureCssLoaded);
    FormAutofillParent.removeMessageObserver(this);

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
