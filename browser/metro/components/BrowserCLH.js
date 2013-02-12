/* -*- Mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const nsIBrowserSearchService = Components.interfaces.nsIBrowserSearchService;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

function openWindow(aParent, aURL, aTarget, aFeatures, aArgs) {
  let argString = null;
  if (aArgs && !(aArgs instanceof Ci.nsISupportsArray)) {
    argString = Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
    argString.data = aArgs;
  }

  return Services.ww.openWindow(aParent, aURL, aTarget, aFeatures, argString || aArgs);
}

function resolveURIInternal(aCmdLine, aArgument) {
  let uri = aCmdLine.resolveURI(aArgument);

  if (!(uri instanceof Ci.nsIFileURL))
    return uri;

  try {
    if (uri.file.exists())
      return uri;
  } catch (e) {
    Cu.reportError(e);
  }

  try {
    let urifixup = Cc["@mozilla.org/docshell/urifixup;1"].getService(Ci.nsIURIFixup);
    uri = urifixup.createFixupURI(aArgument, 0);
  } catch (e) {
    Cu.reportError(e);
  }

  return uri;
}

/**
 * Determines whether a home page override is needed.
 * Returns:
 *  "new profile" if this is the first run with a new profile.
 *  "new version" if this is the first run with a build with a different
 *                      Gecko milestone (i.e. right after an upgrade).
 *  "none" otherwise.
 */
function needHomepageOverride() {
  let savedmstone = null;
  try {
    savedmstone = Services.prefs.getCharPref("browser.startup.homepage_override.mstone");
  } catch (e) {}

  if (savedmstone == "ignore")
    return "none";

#expand    let ourmstone = "__MOZ_APP_VERSION__";

  if (ourmstone != savedmstone) {
    Services.prefs.setCharPref("browser.startup.homepage_override.mstone", ourmstone);

    return (savedmstone ? "new version" : "new profile");
  }

  return "none";
}

function getHomePage() {
  let url = "about:start";
  try {
    url = Services.prefs.getComplexValue("browser.startup.homepage", Ci.nsIPrefLocalizedString).data;
  } catch (e) { }

  return url;
}

function showPanelWhenReady(aWindow, aPage) {
  aWindow.addEventListener("UIReadyDelayed", function(aEvent) {
    aWindow.PanelUI.show(aPage);
  }, false);
}

function haveSystemLocale() {
  let localeService = Cc["@mozilla.org/intl/nslocaleservice;1"].getService(Ci.nsILocaleService);
  let systemLocale = localeService.getSystemLocale().getCategory("NSILOCALE_CTYPE");
  return isLocaleAvailable(systemLocale);
}

function checkCurrentLocale() {
  if (Services.prefs.prefHasUserValue("general.useragent.locale")) {
    // if the user has a compatible locale from a different buildid, we need to update
    var buildID = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULAppInfo).platformBuildID;
    let localeBuildID = Services.prefs.getCharPref("extensions.compatability.locales.buildid");
    if (buildID != localeBuildID)
      return false;

    let currentLocale = Services.prefs.getCharPref("general.useragent.locale");
    return isLocaleAvailable(currentLocale);
  }
  return true;
}

function isLocaleAvailable(aLocale) {
  let chrome = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(Ci.nsIXULChromeRegistry);
  chrome.QueryInterface(Ci.nsIToolkitChromeRegistry);
  let availableLocales = chrome.getLocalesForPackage("browser");
 
  let locale = aLocale.split("-")[0];
  let localeRegEx = new RegExp("^" + locale);
 
  while (availableLocales.hasMore()) {
    let locale = availableLocales.getNext();
    if (localeRegEx.test(locale))
      return true;
  }
  return false;
}

function BrowserCLH() { }

BrowserCLH.prototype = {
  //
  // nsICommandLineHandler
  //
  handle: function fs_handle(aCmdLine) {
#ifdef DEBUG
    for (var idx = 0; idx <  aCmdLine.length; idx++) {
      dump(aCmdLine.getArgument(idx) + "\n");
    }
#endif
    // Instantiate the search service so the search engine cache is created now
    // instead when the application is running. The install process will register
    // this component by using the -silent command line flag, thereby creating
    // the cache during install, not runtime.
    // NOTE: This code assumes this CLH is run before the nsDefaultCLH, which
    // consumes the "-silent" flag.
    if (aCmdLine.findFlag("silent", false) > -1) {
      let searchService = Services.search;
      let autoComplete = Cc["@mozilla.org/autocomplete/search;1?name=history"].
                         getService(Ci.nsIAutoCompleteSearch);
      return;
    }

    // Handle chrome windows loaded via commandline
    let chromeParam = aCmdLine.handleFlagWithParam("chrome", false);
    if (chromeParam) {
      try {
        // Only load URIs which do not inherit chrome privs
        let features = "chrome,dialog=no,all";
        let uri = resolveURIInternal(aCmdLine, chromeParam);
        let netutil = Cc["@mozilla.org/network/util;1"].getService(Ci.nsINetUtil);
        if (!netutil.URIChainHasFlags(uri, Ci.nsIHttpProtocolHandler.URI_INHERITS_SECURITY_CONTEXT)) {
          openWindow(null, uri.spec, "_blank", features, null);

          // Stop the normal commandline processing from continuing
          aCmdLine.preventDefault = true;
        }
      } catch (e) {
        Cu.reportError(e);
      }
      return;
    }

    // Check and remove the alert flag here, but we'll handle it a bit later - see below
    let alertFlag = aCmdLine.handleFlagWithParam("alert", false);

    // Check and remove the webapp param
    let appFlag = aCmdLine.handleFlagWithParam("webapp", false);
    let appURI;
    if (appFlag)
      appURI = resolveURIInternal(aCmdLine, appFlag);

    // Keep an array of possible URL arguments
    let uris = [];

    // Check for the "url" flag
    let uriFlag = aCmdLine.handleFlagWithParam("url", false);
    if (uriFlag) {
      let uri = resolveURIInternal(aCmdLine, uriFlag);
      if (uri)
        uris.push(uri);
    }

    // Check for the "search" flag
    let searchParam = aCmdLine.handleFlagWithParam("search", false);
    if (searchParam) {
      var ss = Components.classes["@mozilla.org/browser/search-service;1"]
                         .getService(nsIBrowserSearchService);
      var submission = ss.defaultEngine.getSubmission(searchParam.replace("\"", "", "g"));
      uris.push(submission.uri);
    }

    for (let i = 0; i < aCmdLine.length; i++) {
      let arg = aCmdLine.getArgument(i);
      if (!arg || arg[0] == '-')
        continue;

      let uri = resolveURIInternal(aCmdLine, arg);
      if (uri)
        uris.push(uri);
    }

    // Open the main browser window, if we don't already have one
    let browserWin;
    try {
      let localeWin = Services.wm.getMostRecentWindow("navigator:localepicker");
      if (localeWin) {
        localeWin.focus();
        aCmdLine.preventDefault = true;
        return;
      }

      browserWin = Services.wm.getMostRecentWindow("navigator:browser");
      if (!browserWin) {
        // Default to the saved homepage
        let defaultURL = getHomePage();

        // Override the default if we have a URL passed on command line
        if (uris.length > 0) {
          defaultURL = uris[0].spec;
          uris = uris.slice(1);
        }

        // Show the locale selector if we have a new profile, or if the selected locale is no longer compatible
        let showLocalePicker = Services.prefs.getBoolPref("browser.firstrun.show.localepicker");
        if ((needHomepageOverride() == "new profile" && showLocalePicker && !haveSystemLocale())) { // || !checkCurrentLocale()) {
          browserWin = openWindow(null, "chrome://browser/content/localePicker.xul", "_blank", "chrome,dialog=no,all", defaultURL);
          aCmdLine.preventDefault = true;
          return;
        }

        browserWin = openWindow(null, "chrome://browser/content/browser.xul", "_blank", "chrome,dialog=no,all", defaultURL);
      }

      browserWin.focus();

      // Stop the normal commandline processing from continuing. We just opened the main browser window
      aCmdLine.preventDefault = true;
    } catch (e) {
      Cu.reportError(e);
    }

    // Assumption: All remaining command line arguments have been sent remotely (browser is already running)
    // Action: Open any URLs we find into an existing browser window

    // First, get a browserDOMWindow object
    while (!browserWin.browserDOMWindow)
      Services.tm.currentThread.processNextEvent(true);

    // Open any URIs into new tabs
    for (let i = 0; i < uris.length; i++)
      browserWin.browserDOMWindow.openURI(uris[i], null, Ci.nsIBrowserDOMWindow.OPEN_NEWTAB, Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL);

    if (appURI)
      browserWin.browserDOMWindow.openURI(appURI, null, browserWin.OPEN_APPTAB, Ci.nsIBrowserDOMWindow.OPEN_NEW);

    // Handle the notification, if called from it
    if (alertFlag) {
      if (alertFlag == "update-app") {
        // Notification was already displayed and clicked, skip it next time
        Services.prefs.setBoolPref("app.update.skipNotification", true);

        var updateService = Cc["@mozilla.org/updates/update-service;1"].getService(Ci.nsIApplicationUpdateService);
        var updateTimerCallback = updateService.QueryInterface(Ci.nsITimerCallback);
        updateTimerCallback.notify(null);
      } else if (alertFlag.length >= 9 && alertFlag.substr(0, 9) == "download:") {
        showPanelWhenReady(browserWin, "downloads-container");
      }
    }
  },

  // QI
  QueryInterface: XPCOMUtils.generateQI([Ci.nsICommandLineHandler]),

  // XPCOMUtils factory
  classID: Components.ID("{be623d20-d305-11de-8a39-0800200c9a66}"),
};

var components = [ BrowserCLH ];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
