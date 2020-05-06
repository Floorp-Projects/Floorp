/* Copyright 2012 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

"use strict";

var EXPORTED_SYMBOLS = ["PdfJs"];

const PREF_PREFIX = "pdfjs";
const PREF_DISABLED = PREF_PREFIX + ".disabled";
const PREF_MIGRATION_VERSION = PREF_PREFIX + ".migrationVersion";
const PREF_PREVIOUS_ACTION = PREF_PREFIX + ".previousHandler.preferredAction";
const PREF_PREVIOUS_ASK =
  PREF_PREFIX + ".previousHandler.alwaysAskBeforeHandling";
const PREF_ISDEFAULT_CACHE_STATE = PREF_PREFIX + ".enabledCache.state";
const TOPIC_PDFJS_HANDLER_CHANGED = "pdfjs:handlerChanged";
const PDF_CONTENT_TYPE = "application/pdf";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var Svc = {};
XPCOMUtils.defineLazyServiceGetter(
  Svc,
  "mime",
  "@mozilla.org/mime;1",
  "nsIMIMEService"
);
XPCOMUtils.defineLazyServiceGetter(
  Svc,
  "handlerService",
  "@mozilla.org/uriloader/handler-service;1",
  "nsIHandlerService"
);
ChromeUtils.defineModuleGetter(
  this,
  "PdfjsChromeUtils",
  "resource://pdf.js/PdfjsChromeUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PdfJsDefaultPreferences",
  "resource://pdf.js/PdfJsDefaultPreferences.jsm"
);

function initializeDefaultPreferences() {
  var defaultBranch = Services.prefs.getDefaultBranch(PREF_PREFIX + ".");
  var defaultValue;
  for (var key in PdfJsDefaultPreferences) {
    defaultValue = PdfJsDefaultPreferences[key];
    switch (typeof defaultValue) {
      case "boolean":
        defaultBranch.setBoolPref(key, defaultValue);
        break;
      case "number":
        defaultBranch.setIntPref(key, defaultValue);
        break;
      case "string":
        defaultBranch.setCharPref(key, defaultValue);
        break;
    }
  }
}

// We're supposed to get this type of thing from the OS, and generally we do.
// But doing so is expensive, so on startup paths we can use this to make the
// handler service get and store the Right Thing (it just goes into a JSON
// file) and avoid the performance issues.
const gPdfFakeHandlerInfo = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIMIMEInfo]),
  getFileExtensions() {
    return ["pdf"];
  },
  possibleApplicationHandlers: Cc["@mozilla.org/array;1"].createInstance(
    Ci.nsIMutableArray
  ),
  extensionExists(ext) {
    return ext == "pdf";
  },
  alwaysAskBeforeHandling: false,
  preferredAction: Ci.nsIHandlerInfo.handleInternally,
  type: PDF_CONTENT_TYPE,
};

var PdfJs = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver]),
  _initialized: false,
  _cachedIsDefault: true,

  init: function init(isNewProfile) {
    if (
      Services.appinfo.processType !== Services.appinfo.PROCESS_TYPE_DEFAULT
    ) {
      throw new Error(
        "PdfJs.init should only get called in the parent process."
      );
    }
    PdfjsChromeUtils.init();
    this.initPrefs();
    this.checkIsDefault(isNewProfile);
  },

  initPrefs: function initPrefs() {
    if (this._initialized) {
      return;
    }
    this._initialized = true;

    if (this._prefDisabled) {
      this._unbecomeHandler();
    } else {
      this._migrate();
    }

    // Listen for when a different pdf handler is chosen.
    Services.obs.addObserver(this, TOPIC_PDFJS_HANDLER_CHANGED);

    initializeDefaultPreferences();
  },

  uninit: function uninit() {
    if (this._initialized) {
      Services.obs.removeObserver(this, TOPIC_PDFJS_HANDLER_CHANGED);
      this._initialized = false;
    }
  },

  _migrate: function migrate() {
    const VERSION = 2;
    var currentVersion = Services.prefs.getIntPref(PREF_MIGRATION_VERSION, 0);
    if (currentVersion >= VERSION) {
      return;
    }
    // Make pdf.js the default pdf viewer on the first migration.
    if (currentVersion < 1) {
      this._becomeHandler();
    }
    if (currentVersion < 2) {
      // cleaning up of unused database preference (see #3994)
      Services.prefs.clearUserPref(PREF_PREFIX + ".database");
    }
    Services.prefs.setIntPref(PREF_MIGRATION_VERSION, VERSION);
  },

  _becomeHandler: function _becomeHandler() {
    // Normally, this happens in the first run at some point, where the
    // handler service doesn't have any info on user preferences yet.
    // Don't bother storing old defaults in this case, as they're
    // meaningless anyway.
    if (!Svc.handlerService.exists(gPdfFakeHandlerInfo)) {
      // Store the requisite info into the DB, and nothing else:
      Svc.handlerService.store(gPdfFakeHandlerInfo);
    } else {
      let handlerInfo = Svc.mime.getFromTypeAndExtension(
        PDF_CONTENT_TYPE,
        "pdf"
      );
      let prefs = Services.prefs;
      if (
        handlerInfo.preferredAction !== Ci.nsIHandlerInfo.handleInternally &&
        handlerInfo.alwaysAskBeforeHandling !== false
      ) {
        // Store the previous settings of preferredAction and
        // alwaysAskBeforeHandling in case we need to revert them in a hotfix that
        // would turn pdf.js off.
        prefs.setIntPref(PREF_PREVIOUS_ACTION, handlerInfo.preferredAction);
        prefs.setBoolPref(
          PREF_PREVIOUS_ASK,
          handlerInfo.alwaysAskBeforeHandling
        );
      }

      // Change and save mime handler settings.
      handlerInfo.alwaysAskBeforeHandling = false;
      handlerInfo.preferredAction = Ci.nsIHandlerInfo.handleInternally;
      Svc.handlerService.store(handlerInfo);
    }
  },

  _unbecomeHandler: function _unbecomeHandler() {
    let handlerInfo = Svc.mime.getFromTypeAndExtension(PDF_CONTENT_TYPE, "pdf");
    if (handlerInfo.preferredAction === Ci.nsIHandlerInfo.handleInternally) {
      // If PDFJS is disabled, but we're still marked to handleInternally,
      // either put it back to what it was, or remove it.
      if (Services.prefs.prefHasUserValue(PREF_PREVIOUS_ACTION)) {
        handlerInfo.preferredAction = Services.prefs.getIntPref(
          PREF_PREVIOUS_ACTION
        );
        handlerInfo.alwaysAskBeforeHandling = Services.prefs.getBoolPref(
          PREF_PREVIOUS_ASK
        );
        Svc.handlerService.store(handlerInfo);
      } else {
        Svc.handlerService.remove(handlerInfo);
        // Clear migration pref so the handler comes back if reenabled
        Services.prefs.clearIntPref(PREF_MIGRATION_VERSION);
      }
    }
  },

  /**
   * @param isNewProfile used to decide whether we need to check the
   *                     handler service to see if the user configured
   *                     pdfs differently. If we're on a new profile,
   *                     there's no need to check.
   */
  _isDefault(isNewProfile) {
    let { processType, PROCESS_TYPE_DEFAULT } = Services.appinfo;
    if (processType !== PROCESS_TYPE_DEFAULT) {
      throw new Error(
        "isDefault should only get called in the parent process."
      );
    }

    if (this._prefDisabled) {
      return false;
    }

    // Don't bother with the handler service on a new profile:
    if (isNewProfile) {
      return true;
    }
    // Check if the 'application/pdf' preview handler is configured properly.
    let handlerInfo = Svc.mime.getFromTypeAndExtension(PDF_CONTENT_TYPE, "pdf");
    return (
      !handlerInfo.alwaysAskBeforeHandling &&
      handlerInfo.preferredAction == Ci.nsIHandlerInfo.handleInternally
    );
  },

  checkIsDefault(isNewProfile) {
    this._cachedIsDefault = this._isDefault(isNewProfile);
    Services.prefs.setBoolPref(
      PREF_ISDEFAULT_CACHE_STATE,
      this._cachedIsDefault
    );
  },

  cachedIsDefault() {
    return this._cachedIsDefault;
  },

  // nsIObserver
  observe(aSubject, aTopic, aData) {
    this.checkIsDefault();
  },
};

XPCOMUtils.defineLazyPreferenceGetter(
  PdfJs,
  "_prefDisabled",
  PREF_DISABLED,
  false,
  () => PdfJs.checkIsDefault()
);
