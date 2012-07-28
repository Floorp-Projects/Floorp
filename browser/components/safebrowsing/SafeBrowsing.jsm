/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["SafeBrowsing"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

const phishingList = "goog-phish-shavar";
const malwareList  = "goog-malware-shavar";

var debug = false;
function log(...stuff) {
  if (!debug)
    return;

  let msg = "SafeBrowsing: " + stuff.join(" ");
  Services.console.logStringMessage(msg);
  dump(msg + "\n");
}

var SafeBrowsing = {

  init: function() {
    if (this.initialized) {
      log("Already initialized");
      return;
    }

    Services.prefs.addObserver("browser.safebrowsing", this.readPrefs, false);
    this.readPrefs();

    this.initProviderURLs();

    // Initialize the list manager
    let listManager = Cc["@mozilla.org/url-classifier/listmanager;1"].
                      getService(Ci.nsIUrlListManager);

    listManager.setUpdateUrl(this.updateURL);
    // setKeyUrl has the side effect of fetching a key from the server.
    // This shouldn't happen if anti-phishing/anti-malware is disabled.
    if (this.phishingEnabled || this.malwareEnabled)
      listManager.setKeyUrl(this.keyURL);
    listManager.setGethashUrl(this.gethashURL);

    // Register our two types of tables
    listManager.registerTable(phishingList, false);
    listManager.registerTable(malwareList, false);
    this.addMozEntries();

    this.controlUpdateChecking();
    this.initialized = true;

    log("init() finished");
  },


  initialized:     false,
  phishingEnabled: false,
  malwareEnabled:  false,

  provName:              null,
  updateURL:             null,
  keyURL:                null,
  reportURL:             null,
  gethashURL:            null,
  reportGenericURL:      null,
  reportErrorURL:        null,
  reportPhishURL:        null,
  reportMalwareURL:      null,
  reportMalwareErrorURL: null,


  getReportURL: function(kind) {
    return this["report"  + kind + "URL"];
  },


  readPrefs: function() {
    log("reading prefs");

    debug = Services.prefs.getBoolPref("browser.safebrowsing.debug");
    this.phishingEnabled = Services.prefs.getBoolPref("browser.safebrowsing.enabled");
    this.malwareEnabled  = Services.prefs.getBoolPref("browser.safebrowsing.malware.enabled");

    // XXX The listManager backend gets confused if this is called before the
    // lists are registered. So only call it here when a pref changes, and not
    // when doing initialization. I expect to refactor this later, so pardon the hack.
    if (this.initialized)
      this.controlUpdateChecking();
  },


  initProviderURLs: function() {
    log("initializing provider URLs");

    // XXX remove this as obsolete?
    let provID = Services.prefs.getIntPref("browser.safebrowsing.dataProvider");
    if (provID != 0) {
      Cu.reportError("unknown safebrowsing provider ID " + provID);
      return;
    }

    let basePref = "browser.safebrowsing.provider.0.";
    this.provName = Services.prefs.getCharPref(basePref + "name");

    // Urls used to get data from a provider
    this.updateURL  = this.getUrlPref(basePref + "updateURL");
    this.keyURL     = this.getUrlPref(basePref + "keyURL");
    this.reportURL  = this.getUrlPref(basePref + "reportURL");
    this.gethashURL = this.getUrlPref(basePref + "gethashURL");

    // Urls to HTML report pages
    this.reportGenericURL      = this.getUrlPref(basePref + "reportGenericURL");
    this.reportErrorURL        = this.getUrlPref(basePref + "reportErrorURL");
    this.reportPhishURL        = this.getUrlPref(basePref + "reportPhishURL");
    this.reportMalwareURL      = this.getUrlPref(basePref + "reportMalwareURL")
    this.reportMalwareErrorURL = this.getUrlPref(basePref + "reportMalwareErrorURL")
  },


  getUrlPref: function(prefName) {
    let MOZ_OFFICIAL_BUILD = false;
#ifdef OFFICIAL_BUILD
    MOZ_OFFICIAL_BUILD = true;
#endif

    let url = Services.prefs.getCharPref(prefName);

    let clientName = MOZ_OFFICIAL_BUILD ? "navclient-auto-ffox" : Services.appinfo.name;
    let clientVersion = Services.appinfo.version;

    // Parameter substitution
    // XXX: we should instead use nsIURLFormatter here.
    url = url.replace(/\{moz:locale\}/g,  this.getLocale());
    url = url.replace(/\{moz:client\}/g,  clientName);
    url = url.replace(/\{moz:buildid\}/g, Services.appinfo.appBuildID);
    url = url.replace(/\{moz:version\}/g, clientVersion);

    log(prefName, "is", url);
    return url;
  },


  getLocale: function() {
    const localePref = "general.useragent.locale";

    let locale = Services.prefs.getCharPref(localePref);
    try {
      // Dumb. This API only works if pref is localized or has a user value.
      locale = Services.prefs.getComplexValue(localePref, Ci.nsIPrefLocalizedString).data;
    } catch (e) { }

    return locale;
  },


  controlUpdateChecking: function() {
    log("phishingEnabled:", this.phishingEnabled, "malwareEnabled:", this.malwareEnabled);

    let listManager = Cc["@mozilla.org/url-classifier/listmanager;1"].
                      getService(Ci.nsIUrlListManager);

    if (this.phishingEnabled)
      listManager.enableUpdate(phishingList);
    else
      listManager.disableUpdate(phishingList);

    if (this.malwareEnabled)
      listManager.enableUpdate(malwareList);
    else
      listManager.disableUpdate(malwareList);
  },


  addMozEntries: function() {
    // Add test entries to the DB.
    // XXX this should really just be done by DB itself for all moz apps.
    const phishURL   = "mozilla.org/firefox/its-a-trap.html";
    const malwareURL = "mozilla.org/firefox/its-an-attack.html";

    let update = "n:1000\ni:test-malware-simple\nad:1\n" +
                 "a:1:32:" + malwareURL.length + "\n" +
                 malwareURL;
    update += "n:1000\ni:test-phish-simple\nad:1\n" +
              "a:1:32:" + phishURL.length + "\n" +
              phishURL;
    log("addMozEntries:", update);

    let db = Cc["@mozilla.org/url-classifier/dbservice;1"].
             getService(Ci.nsIUrlClassifierDBService);

    // nsIUrlClassifierUpdateObserver
    let dummyListener = {
      updateUrlRequested: function() { },
      streamFinished:     function() { },
      updateError:        function() { },
      updateSuccess:      function() { }
    };

    try {
      db.beginUpdate(dummyListener, "test-malware-simple,test-phish-simple", "");
      db.beginStream("", "");
      db.updateStream(update);
      db.finishStream();
      db.finishUpdate();
    } catch(ex) {
      // beginUpdate will throw harmlessly if there's an existing update in progress, ignore failures.
      log("addMozEntries failed!", ex);
    }
  },
};
