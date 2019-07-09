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
const PREF_ENABLED_CACHE_STATE = PREF_PREFIX + ".enabledCache.state";
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

var PdfJs = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver]),
  _initialized: false,

  init: function init() {
    if (
      Services.appinfo.processType !== Services.appinfo.PROCESS_TYPE_DEFAULT
    ) {
      throw new Error(
        "PdfJs.init should only get called in the parent process."
      );
    }
    PdfjsChromeUtils.init();
    this.initPrefs();

    Services.ppmm.sharedData.set("pdfjs.enabled", this.checkEnabled());
  },

  earlyInit() {
    // Note: Please keep this in sync with the duplicated logic in
    // BrowserGlue.jsm.
    Services.ppmm.sharedData.set("pdfjs.enabled", this.checkEnabled());
  },

  initPrefs: function initPrefs() {
    if (this._initialized) {
      return;
    }
    this._initialized = true;

    if (!Services.prefs.getBoolPref(PREF_DISABLED, true)) {
      this._migrate();
    }

    // Listen for when pdf.js is completely disabled or a different pdf handler
    // is chosen.
    Services.prefs.addObserver(PREF_DISABLED, this);
    Services.obs.addObserver(this, TOPIC_PDFJS_HANDLER_CHANGED);

    initializeDefaultPreferences();
  },

  uninit: function uninit() {
    if (this._initialized) {
      Services.prefs.removeObserver(PREF_DISABLED, this);
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
    let handlerInfo = Svc.mime.getFromTypeAndExtension(PDF_CONTENT_TYPE, "pdf");
    let prefs = Services.prefs;
    if (
      handlerInfo.preferredAction !== Ci.nsIHandlerInfo.handleInternally &&
      handlerInfo.preferredAction !== false
    ) {
      // Store the previous settings of preferredAction and
      // alwaysAskBeforeHandling in case we need to revert them in a hotfix that
      // would turn pdf.js off.
      prefs.setIntPref(PREF_PREVIOUS_ACTION, handlerInfo.preferredAction);
      prefs.setBoolPref(PREF_PREVIOUS_ASK, handlerInfo.alwaysAskBeforeHandling);
    }

    let handlerService = Cc[
      "@mozilla.org/uriloader/handler-service;1"
    ].getService(Ci.nsIHandlerService);

    // Change and save mime handler settings.
    handlerInfo.alwaysAskBeforeHandling = false;
    handlerInfo.preferredAction = Ci.nsIHandlerInfo.handleInternally;
    handlerService.store(handlerInfo);
  },

  _isEnabled: function _isEnabled() {
    let { processType, PROCESS_TYPE_DEFAULT } = Services.appinfo;
    if (processType !== PROCESS_TYPE_DEFAULT) {
      throw new Error(
        "isEnabled should only get called in the parent process."
      );
    }

    if (Services.prefs.getBoolPref(PREF_DISABLED, true)) {
      return false;
    }

    // Check if the 'application/pdf' preview handler is configured properly.
    return PdfjsChromeUtils.isDefaultHandlerApp();
  },

  checkEnabled: function checkEnabled() {
    let isEnabled = this._isEnabled();
    // This will be updated any time we observe a dependency changing, since
    // updateRegistration internally calls enabled.
    Services.prefs.setBoolPref(PREF_ENABLED_CACHE_STATE, isEnabled);
    return isEnabled;
  },

  // nsIObserver
  observe: function observe(aSubject, aTopic, aData) {
    if (
      Services.appinfo.processType !== Services.appinfo.PROCESS_TYPE_DEFAULT
    ) {
      throw new Error(
        "Only the parent process should be observing PDF handler changes."
      );
    }

    Services.ppmm.sharedData.set("pdfjs.enabled", this.checkEnabled());
  },
};
