/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.importGlobalProperties(["URLSearchParams"]);

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/AppConstants.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "LaterRun",
                                  "resource:///modules/LaterRun.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "RecentWindow",
                                  "resource:///modules/RecentWindow.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ShellService",
                                  "resource:///modules/ShellService.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "WindowsUIUtils",
                                   "@mozilla.org/windows-ui-utils;1", "nsIWindowsUIUtils");

const nsISupports            = Components.interfaces.nsISupports;

const nsIBrowserDOMWindow    = Components.interfaces.nsIBrowserDOMWindow;
const nsIBrowserHandler      = Components.interfaces.nsIBrowserHandler;
const nsIBrowserHistory      = Components.interfaces.nsIBrowserHistory;
const nsIChannel             = Components.interfaces.nsIChannel;
const nsICommandLine         = Components.interfaces.nsICommandLine;
const nsICommandLineHandler  = Components.interfaces.nsICommandLineHandler;
const nsIContentHandler      = Components.interfaces.nsIContentHandler;
const nsIDocShellTreeItem    = Components.interfaces.nsIDocShellTreeItem;
const nsIDOMChromeWindow     = Components.interfaces.nsIDOMChromeWindow;
const nsIDOMWindow           = Components.interfaces.nsIDOMWindow;
const nsIFileURL             = Components.interfaces.nsIFileURL;
const nsIInterfaceRequestor  = Components.interfaces.nsIInterfaceRequestor;
const nsINetUtil             = Components.interfaces.nsINetUtil;
const nsIPrefLocalizedString = Components.interfaces.nsIPrefLocalizedString;
const nsISupportsString      = Components.interfaces.nsISupportsString;
const nsIWebNavigation       = Components.interfaces.nsIWebNavigation;
const nsIWebNavigationInfo   = Components.interfaces.nsIWebNavigationInfo;
const nsICommandLineValidator = Components.interfaces.nsICommandLineValidator;

const NS_BINDING_ABORTED = Components.results.NS_BINDING_ABORTED;
const NS_ERROR_WONT_HANDLE_CONTENT = 0x805d0001;
const NS_ERROR_ABORT = Components.results.NS_ERROR_ABORT;

function shouldLoadURI(aURI) {
  if (aURI && !aURI.schemeIs("chrome"))
    return true;

  dump("*** Preventing external load of chrome: URI into browser window\n");
  dump("    Use --chrome <uri> instead\n");
  return false;
}

function resolveURIInternal(aCmdLine, aArgument) {
  var uri = aCmdLine.resolveURI(aArgument);
  var uriFixup = Services.uriFixup;

  if (!(uri instanceof nsIFileURL)) {
    return uriFixup.createFixupURI(aArgument,
                                   uriFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS);
  }

  try {
    if (uri.file.exists())
      return uri;
  }
  catch (e) {
    Components.utils.reportError(e);
  }

  // We have interpreted the argument as a relative file URI, but the file
  // doesn't exist. Try URI fixup heuristics: see bug 290782.

  try {
    uri = uriFixup.createFixupURI(aArgument, 0);
  }
  catch (e) {
    Components.utils.reportError(e);
  }

  return uri;
}

var gFirstWindow = false;

const OVERRIDE_NONE        = 0;
const OVERRIDE_NEW_PROFILE = 1;
const OVERRIDE_NEW_MSTONE  = 2;
const OVERRIDE_NEW_BUILD_ID = 3;
/**
 * Determines whether a home page override is needed.
 * Returns:
 *  OVERRIDE_NEW_PROFILE if this is the first run with a new profile.
 *  OVERRIDE_NEW_MSTONE if this is the first run with a build with a different
 *                      Gecko milestone (i.e. right after an upgrade).
 *  OVERRIDE_NEW_BUILD_ID if this is the first run with a new build ID of the
 *                        same Gecko milestone (i.e. after a nightly upgrade).
 *  OVERRIDE_NONE otherwise.
 */
function needHomepageOverride(prefb) {
  var savedmstone = null;
  try {
    savedmstone = prefb.getCharPref("browser.startup.homepage_override.mstone");
  } catch (e) {}

  if (savedmstone == "ignore")
    return OVERRIDE_NONE;

  var mstone = Services.appinfo.platformVersion;

  var savedBuildID = null;
  try {
    savedBuildID = prefb.getCharPref("browser.startup.homepage_override.buildID");
  } catch (e) {}

  var buildID = Services.appinfo.platformBuildID;

  if (mstone != savedmstone) {
    // Bug 462254. Previous releases had a default pref to suppress the EULA
    // agreement if the platform's installer had already shown one. Now with
    // about:rights we've removed the EULA stuff and default pref, but we need
    // a way to make existing profiles retain the default that we removed.
    if (savedmstone)
      prefb.setBoolPref("browser.rights.3.shown", true);

    prefb.setCharPref("browser.startup.homepage_override.mstone", mstone);
    prefb.setCharPref("browser.startup.homepage_override.buildID", buildID);
    return (savedmstone ? OVERRIDE_NEW_MSTONE : OVERRIDE_NEW_PROFILE);
  }

  if (buildID != savedBuildID) {
    prefb.setCharPref("browser.startup.homepage_override.buildID", buildID);
    return OVERRIDE_NEW_BUILD_ID;
  }

  return OVERRIDE_NONE;
}

/**
 * Gets the override page for the first run after the application has been
 * updated.
 * @param  defaultOverridePage
 *         The default override page.
 * @return The override page.
 */
function getPostUpdateOverridePage(defaultOverridePage) {
  var um = Components.classes["@mozilla.org/updates/update-manager;1"]
                     .getService(Components.interfaces.nsIUpdateManager);
  try {
    // If the updates.xml file is deleted then getUpdateAt will throw.
    var update = um.getUpdateAt(0)
                   .QueryInterface(Components.interfaces.nsIPropertyBag);
  } catch (e) {
    // This should never happen.
    Components.utils.reportError("Unable to find update: " + e);
    return defaultOverridePage;
  }

  let actions = update.getProperty("actions");
  // When the update doesn't specify actions fallback to the original behavior
  // of displaying the default override page.
  if (!actions)
    return defaultOverridePage;

  // The existence of silent or the non-existence of showURL in the actions both
  // mean that an override page should not be displayed.
  if (actions.indexOf("silent") != -1 || actions.indexOf("showURL") == -1)
    return "";

  return update.getProperty("openURL") || defaultOverridePage;
}

// Flag used to indicate that the arguments to openWindow can be passed directly.
const NO_EXTERNAL_URIS = 1;

function openWindow(parent, url, target, features, args, noExternalArgs) {
  if (noExternalArgs == NO_EXTERNAL_URIS) {
    // Just pass in the defaultArgs directly
    var argstring;
    if (args) {
      argstring = Components.classes["@mozilla.org/supports-string;1"]
                            .createInstance(nsISupportsString);
      argstring.data = args;
    }

    return Services.ww.openWindow(parent, url, target, features, argstring);
  }

  // Pass an array to avoid the browser "|"-splitting behavior.
  var argArray = Components.classes["@mozilla.org/supports-array;1"]
                    .createInstance(Components.interfaces.nsISupportsArray);

  // add args to the arguments array
  var stringArgs = null;
  if (args instanceof Array) // array
    stringArgs = args;
  else if (args) // string
    stringArgs = [args];

  if (stringArgs) {
    // put the URIs into argArray
    var uriArray = Components.classes["@mozilla.org/supports-array;1"]
                       .createInstance(Components.interfaces.nsISupportsArray);
    stringArgs.forEach(function (uri) {
      var sstring = Components.classes["@mozilla.org/supports-string;1"]
                              .createInstance(nsISupportsString);
      sstring.data = uri;
      uriArray.AppendElement(sstring);
    });
    argArray.AppendElement(uriArray);
  } else {
    argArray.AppendElement(null);
  }

  // Pass these as null to ensure that we always trigger the "single URL"
  // behavior in browser.js's gBrowserInit.onLoad (which handles the window
  // arguments)
  argArray.AppendElement(null); // charset
  argArray.AppendElement(null); // referer
  argArray.AppendElement(null); // postData
  argArray.AppendElement(null); // allowThirdPartyFixup

  return Services.ww.openWindow(parent, url, target, features, argArray);
}

function openPreferences() {
  var sa = Components.classes["@mozilla.org/supports-array;1"]
                     .createInstance(Components.interfaces.nsISupportsArray);

  var wuri = Components.classes["@mozilla.org/supports-string;1"]
                       .createInstance(Components.interfaces.nsISupportsString);
  wuri.data = "about:preferences";

  sa.AppendElement(wuri);

  Services.ww.openWindow(null, gBrowserContentHandler.chromeURL,
                         "_blank",
                         "chrome,dialog=no,all",
                         sa);
}

function logSystemBasedSearch(engine) {
  var countId = (engine.identifier || ("other-" + engine.name)) + ".system";
  var count = Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS");
  count.add(countId);
}

function doSearch(searchTerm, cmdLine) {
  var engine = Services.search.defaultEngine;
  logSystemBasedSearch(engine);

  var submission = engine.getSubmission(searchTerm, null, "system");

  // fill our nsISupportsArray with uri-as-wstring, null, null, postData
  var sa = Components.classes["@mozilla.org/supports-array;1"]
                     .createInstance(Components.interfaces.nsISupportsArray);

  var wuri = Components.classes["@mozilla.org/supports-string;1"]
                       .createInstance(Components.interfaces.nsISupportsString);
  wuri.data = submission.uri.spec;

  sa.AppendElement(wuri);
  sa.AppendElement(null);
  sa.AppendElement(null);
  sa.AppendElement(submission.postData);

  // XXXbsmedberg: use handURIToExistingBrowser to obey tabbed-browsing
  // preferences, but need nsIBrowserDOMWindow extensions

  return Services.ww.openWindow(null, gBrowserContentHandler.chromeURL,
                                "_blank",
                                "chrome,dialog=no,all" +
                                gBrowserContentHandler.getFeatures(cmdLine),
                                sa);
}

function nsBrowserContentHandler() {
}
nsBrowserContentHandler.prototype = {
  classID: Components.ID("{5d0ce354-df01-421a-83fb-7ead0990c24e}"),

  _xpcom_factory: {
    createInstance: function bch_factory_ci(outer, iid) {
      if (outer)
        throw Components.results.NS_ERROR_NO_AGGREGATION;
      return gBrowserContentHandler.QueryInterface(iid);
    }
  },

  /* helper functions */

  mChromeURL : null,

  get chromeURL() {
    if (this.mChromeURL) {
      return this.mChromeURL;
    }

    this.mChromeURL = Services.prefs.getCharPref("browser.chromeURL");

    return this.mChromeURL;
  },

  /* nsISupports */
  QueryInterface : XPCOMUtils.generateQI([nsICommandLineHandler,
                                          nsIBrowserHandler,
                                          nsIContentHandler,
                                          nsICommandLineValidator]),

  /* nsICommandLineHandler */
  handle : function bch_handle(cmdLine) {
    if (cmdLine.handleFlag("browser", false)) {
      // Passing defaultArgs, so use NO_EXTERNAL_URIS
      openWindow(null, this.chromeURL, "_blank",
                 "chrome,dialog=no,all" + this.getFeatures(cmdLine),
                 this.defaultArgs, NO_EXTERNAL_URIS);
      cmdLine.preventDefault = true;
    }

    // In the past, when an instance was not already running, the -remote
    // option returned an error code. Any script or application invoking the
    // -remote option is expected to be handling this case, otherwise they
    // wouldn't be doing anything when there is no Firefox already running.
    // Making the -remote option always return an error code makes those
    // scripts or applications handle the situation as if Firefox was not
    // already running.
    if (cmdLine.handleFlag("remote", true)) {
      throw NS_ERROR_ABORT;
    }

    var uriparam;
    try {
      while ((uriparam = cmdLine.handleFlagWithParam("new-window", false))) {
        let uri = resolveURIInternal(cmdLine, uriparam);
        if (!shouldLoadURI(uri))
          continue;
        openWindow(null, this.chromeURL, "_blank",
                   "chrome,dialog=no,all" + this.getFeatures(cmdLine),
                   uri.spec);
        cmdLine.preventDefault = true;
      }
    }
    catch (e) {
      Components.utils.reportError(e);
    }

    try {
      while ((uriparam = cmdLine.handleFlagWithParam("new-tab", false))) {
        let uri = resolveURIInternal(cmdLine, uriparam);
        handURIToExistingBrowser(uri, nsIBrowserDOMWindow.OPEN_NEWTAB, cmdLine);
        cmdLine.preventDefault = true;
      }
    }
    catch (e) {
      Components.utils.reportError(e);
    }

    var chromeParam = cmdLine.handleFlagWithParam("chrome", false);
    if (chromeParam) {

      // Handle old preference dialog URLs.
      if (chromeParam == "chrome://browser/content/pref/pref.xul" ||
          chromeParam == "chrome://browser/content/preferences/preferences.xul") {
        openPreferences();
        cmdLine.preventDefault = true;
      } else try {
        let resolvedURI = resolveURIInternal(cmdLine, chromeParam);
        let isLocal = uri => {
          let localSchemes = new Set(["chrome", "file", "resource"]);
          if (uri instanceof Components.interfaces.nsINestedURI) {
            uri = uri.QueryInterface(Components.interfaces.nsINestedURI).innerMostURI;
          }
          return localSchemes.has(uri.scheme);
        };
        if (isLocal(resolvedURI)) {
          // If the URI is local, we are sure it won't wrongly inherit chrome privs
          var features = "chrome,dialog=no,all" + this.getFeatures(cmdLine);
          openWindow(null, resolvedURI.spec, "_blank", features);
          cmdLine.preventDefault = true;
        } else {
          dump("*** Preventing load of web URI as chrome\n");
          dump("    If you're trying to load a webpage, do not pass --chrome.\n");
        }
      }
      catch (e) {
        Components.utils.reportError(e);
      }
    }
    if (cmdLine.handleFlag("preferences", false)) {
      openPreferences();
      cmdLine.preventDefault = true;
    }
    if (cmdLine.handleFlag("silent", false))
      cmdLine.preventDefault = true;

    try {
      var privateWindowParam = cmdLine.handleFlagWithParam("private-window", false);
      if (privateWindowParam) {
        let resolvedURI = resolveURIInternal(cmdLine, privateWindowParam);
        handURIToExistingBrowser(resolvedURI, nsIBrowserDOMWindow.OPEN_NEWTAB, cmdLine, true);
        cmdLine.preventDefault = true;
      }
    } catch (e) {
      if (e.result != Components.results.NS_ERROR_INVALID_ARG) {
        throw e;
      }
      // NS_ERROR_INVALID_ARG is thrown when flag exists, but has no param.
      if (cmdLine.handleFlag("private-window", false)) {
        openWindow(null, this.chromeURL, "_blank",
          "chrome,dialog=no,private,all" + this.getFeatures(cmdLine),
          "about:privatebrowsing");
        cmdLine.preventDefault = true;
      }
    }

    var searchParam = cmdLine.handleFlagWithParam("search", false);
    if (searchParam) {
      doSearch(searchParam, cmdLine);
      cmdLine.preventDefault = true;
    }

    // The global PB Service consumes this flag, so only eat it in per-window
    // PB builds.
    if (cmdLine.handleFlag("private", false)) {
      PrivateBrowsingUtils.enterTemporaryAutoStartMode();
    }

    var fileParam = cmdLine.handleFlagWithParam("file", false);
    if (fileParam) {
      var file = cmdLine.resolveFile(fileParam);
      var fileURI = Services.io.newFileURI(file);
      openWindow(null, this.chromeURL, "_blank",
                 "chrome,dialog=no,all" + this.getFeatures(cmdLine),
                 fileURI.spec);
      cmdLine.preventDefault = true;
    }

    if (AppConstants.platform  == "win") {
      // Handle "? searchterm" for Windows Vista start menu integration
      for (var i = cmdLine.length - 1; i >= 0; --i) {
        var param = cmdLine.getArgument(i);
        if (param.match(/^\? /)) {
          cmdLine.removeArguments(i, i);
          cmdLine.preventDefault = true;

          searchParam = param.substr(2);
          doSearch(searchParam, cmdLine);
        }
      }
    }
  },

  get helpInfo() {
    let info =
              "  --browser          Open a browser window.\n" +
              "  --new-window <url> Open <url> in a new window.\n" +
              "  --new-tab <url>    Open <url> in a new tab.\n" +
              "  --private-window <url> Open <url> in a new private window.\n";
    if (AppConstants.platform == "win") {
      info += "  --preferences      Open Options dialog.\n";
    } else {
      info += "  --preferences      Open Preferences dialog.\n";
    }
    info += "  --search <term>    Search <term> with your default search engine.\n";
    return info;
  },

  /* nsIBrowserHandler */

  get defaultArgs() {
    var prefb = Services.prefs;

    if (!gFirstWindow) {
      gFirstWindow = true;
      if (PrivateBrowsingUtils.isInTemporaryAutoStartMode) {
        return "about:privatebrowsing";
      }
    }

    var override;
    var overridePage = "";
    var additionalPage = "";
    var willRestoreSession = false;
    try {
      // Read the old value of homepage_override.mstone before
      // needHomepageOverride updates it, so that we can later add it to the
      // URL if we do end up showing an overridePage. This makes it possible
      // to have the overridePage's content vary depending on the version we're
      // upgrading from.
      let old_mstone = "unknown";
      try {
        old_mstone = Services.prefs.getCharPref("browser.startup.homepage_override.mstone");
      } catch (ex) {}
      override = needHomepageOverride(prefb);
      if (override != OVERRIDE_NONE) {
        switch (override) {
          case OVERRIDE_NEW_PROFILE:
            // New profile.
            overridePage = Services.urlFormatter.formatURLPref("startup.homepage_welcome_url");
            additionalPage = Services.urlFormatter.formatURLPref("startup.homepage_welcome_url.additional");
            // Turn on 'later run' pages for new profiles.
            LaterRun.enabled = true;
            break;
          case OVERRIDE_NEW_MSTONE:
            // Check whether we will restore a session. If we will, we assume
            // that this is an "update" session. This does not take crashes
            // into account because that requires waiting for the session file
            // to be read. If a crash occurs after updating, before restarting,
            // we may open the startPage in addition to restoring the session.
            var ss = Components.classes["@mozilla.org/browser/sessionstartup;1"]
                               .getService(Components.interfaces.nsISessionStartup);
            willRestoreSession = ss.isAutomaticRestoreEnabled();

            overridePage = Services.urlFormatter.formatURLPref("startup.homepage_override_url");
            if (prefb.prefHasUserValue("app.update.postupdate"))
              overridePage = getPostUpdateOverridePage(overridePage);

            overridePage = overridePage.replace("%OLD_VERSION%", old_mstone);
            break;
        }
      }
    } catch (ex) {}

    // formatURLPref might return "about:blank" if getting the pref fails
    if (overridePage == "about:blank")
      overridePage = "";

    // Temporary override page for users who are running Firefox on Windows 10 for their first time.
    let platformVersion = Services.sysinfo.getProperty("version");
    if (AppConstants.platform == "win" &&
        Services.vc.compare(platformVersion, "10") == 0 &&
        !Services.prefs.getBoolPref("browser.usedOnWindows10")) {
      Services.prefs.setBoolPref("browser.usedOnWindows10", true);
      let firstUseOnWindows10URL = Services.urlFormatter.formatURLPref("browser.usedOnWindows10.introURL");

      if (firstUseOnWindows10URL && firstUseOnWindows10URL.length) {
        additionalPage = firstUseOnWindows10URL;
        if (override == OVERRIDE_NEW_PROFILE) {
          additionalPage += "&utm_content=firstrun";
        }
      }
    }

    if (!additionalPage) {
      additionalPage = LaterRun.getURL() || "";
    }

    if (additionalPage && additionalPage != "about:blank") {
      if (overridePage) {
        overridePage += "|" + additionalPage;
      } else {
        overridePage = additionalPage;
      }
    }

    var startPage = "";
    try {
      var choice = prefb.getIntPref("browser.startup.page");
      if (choice == 1 || choice == 3)
        startPage = this.startPage;
    } catch (e) {
      Components.utils.reportError(e);
    }

    if (startPage == "about:blank")
      startPage = "";

    // Only show the startPage if we're not restoring an update session.
    if (overridePage && startPage && !willRestoreSession)
      return overridePage + "|" + startPage;

    return overridePage || startPage || "about:blank";
  },

  get startPage() {
    var uri = Services.prefs.getComplexValue("browser.startup.homepage",
                                             nsIPrefLocalizedString).data;
    if (!uri) {
      Services.prefs.clearUserPref("browser.startup.homepage");
      uri = Services.prefs.getComplexValue("browser.startup.homepage",
                                           nsIPrefLocalizedString).data;
    }
    return uri;
  },

  mFeatures : null,

  getFeatures : function bch_features(cmdLine) {
    if (this.mFeatures === null) {
      this.mFeatures = "";

      try {
        var width = cmdLine.handleFlagWithParam("width", false);
        var height = cmdLine.handleFlagWithParam("height", false);

        if (width)
          this.mFeatures += ",width=" + width;
        if (height)
          this.mFeatures += ",height=" + height;
      }
      catch (e) {
      }

      // The global PB Service consumes this flag, so only eat it in per-window
      // PB builds.
      if (PrivateBrowsingUtils.isInTemporaryAutoStartMode) {
        this.mFeatures = ",private";
      }
    }

    return this.mFeatures;
  },

  /* nsIContentHandler */

  handleContent : function bch_handleContent(contentType, context, request) {
    try {
      var webNavInfo = Components.classes["@mozilla.org/webnavigation-info;1"]
                                 .getService(nsIWebNavigationInfo);
      if (!webNavInfo.isTypeSupported(contentType, null)) {
        throw NS_ERROR_WONT_HANDLE_CONTENT;
      }
    } catch (e) {
      throw NS_ERROR_WONT_HANDLE_CONTENT;
    }

    request.QueryInterface(nsIChannel);
    handURIToExistingBrowser(request.URI,
      nsIBrowserDOMWindow.OPEN_DEFAULTWINDOW, null);
    request.cancel(NS_BINDING_ABORTED);
  },

  /* nsICommandLineValidator */
  validate : function bch_validate(cmdLine) {
    // Other handlers may use osint so only handle the osint flag if the url
    // flag is also present and the command line is valid.
    var osintFlagIdx = cmdLine.findFlag("osint", false);
    var urlFlagIdx = cmdLine.findFlag("url", false);
    if (urlFlagIdx > -1 && (osintFlagIdx > -1 ||
        cmdLine.state == nsICommandLine.STATE_REMOTE_EXPLICIT)) {
      var urlParam = cmdLine.getArgument(urlFlagIdx + 1);
      if (cmdLine.length != urlFlagIdx + 2 || /firefoxurl:/.test(urlParam))
        throw NS_ERROR_ABORT;
      var isDefault = false;
      try {
        var url = Services.urlFormatter.formatURLPref("app.support.baseURL") +
                  "win10-default-browser";
        if (urlParam == url) {
          isDefault = ShellService.isDefaultBrowser(false, false);
        }
      } catch (ex) {}
      if (isDefault) {
        // Firefox is already the default HTTP handler.
        // We don't have to show the instruction page.
        throw NS_ERROR_ABORT;
      }
      cmdLine.handleFlag("osint", false)
    }
  },
};
var gBrowserContentHandler = new nsBrowserContentHandler();

function handURIToExistingBrowser(uri, location, cmdLine, forcePrivate)
{
  if (!shouldLoadURI(uri))
    return;

  // Unless using a private window is forced, open external links in private
  // windows only if we're in perma-private mode.
  var allowPrivate = forcePrivate || PrivateBrowsingUtils.permanentPrivateBrowsing;
  var navWin = RecentWindow.getMostRecentBrowserWindow({private: allowPrivate});
  if (!navWin) {
    // if we couldn't load it in an existing window, open a new one
    var features = "chrome,dialog=no,all" + gBrowserContentHandler.getFeatures(cmdLine);
    if (forcePrivate) {
      features += ",private";
    }
    openWindow(null, gBrowserContentHandler.chromeURL, "_blank", features, uri.spec);
    return;
  }

  var navNav = navWin.QueryInterface(nsIInterfaceRequestor)
                     .getInterface(nsIWebNavigation);
  var rootItem = navNav.QueryInterface(nsIDocShellTreeItem).rootTreeItem;
  var rootWin = rootItem.QueryInterface(nsIInterfaceRequestor)
                        .getInterface(nsIDOMWindow);
  var bwin = rootWin.QueryInterface(nsIDOMChromeWindow).browserDOMWindow;
  bwin.openURI(uri, null, location,
               nsIBrowserDOMWindow.OPEN_EXTERNAL);
}

function nsDefaultCommandLineHandler() {
}

nsDefaultCommandLineHandler.prototype = {
  classID: Components.ID("{47cd0651-b1be-4a0f-b5c4-10e5a573ef71}"),

  /* nsISupports */
  QueryInterface : function dch_QI(iid) {
    if (!iid.equals(nsISupports) &&
        !iid.equals(nsICommandLineHandler))
      throw Components.results.NS_ERROR_NO_INTERFACE;

    return this;
  },

  _haveProfile: false,

  /* nsICommandLineHandler */
  handle : function dch_handle(cmdLine) {
    // The -url flag is inserted by the operating system when the default
    // application handler is used. We check for default browser to remove
    // instances where users explicitly decide to "open with" the browser.
    // Note that users who launch firefox manually with the -url flag will
    // get erroneously counted.
    if (cmdLine.findFlag("url", false) &&
        ShellService.isDefaultBrowser(false, false)) {
      try {
        Services.telemetry.getHistogramById("FX_STARTUP_EXTERNAL_CONTENT_HANDLER").add();
      } catch (e) {}
    }

    var urilist = [];

    if (AppConstants.platform == "win") {
      // If we don't have a profile selected yet (e.g. the Profile Manager is
      // displayed) we will crash if we open an url and then select a profile. To
      // prevent this handle all url command line flags and set the command line's
      // preventDefault to true to prevent the display of the ui. The initial
      // command line will be retained when nsAppRunner calls LaunchChild though
      // urls launched after the initial launch will be lost.
      if (!this._haveProfile) {
        try {
          // This will throw when a profile has not been selected.
          var dir = Services.dirsvc.get("ProfD", Components.interfaces.nsILocalFile);
          this._haveProfile = true;
        }
        catch (e) {
          while ((ar = cmdLine.handleFlagWithParam("url", false))) { }
          cmdLine.preventDefault = true;
        }
      }
    }

    let redirectWinSearch = false;
    if (AppConstants.isPlatformAndVersionAtLeast("win", "10")) {
      redirectWinSearch = Services.prefs.getBoolPref("browser.search.redirectWindowsSearch");
    }

    try {
      var ar;
      while ((ar = cmdLine.handleFlagWithParam("url", false))) {
        var uri = resolveURIInternal(cmdLine, ar);

        // Searches in the Windows 10 task bar searchbox simply open the default browser
        // with a URL for a search on Bing. Here we extract the search term and use the
        // user's default search engine instead.
        var uriScheme = "", uriHost = "", uriPath = "";
        try {
          uriScheme = uri.scheme;
          uriHost = uri.host;
          uriPath = uri.path;
        } catch(e) {
        }

        // Most Windows searches are "https://www.bing.com/search...", but bug
        // 1182308 reports a Chinese edition of Windows 10 using
        // "http://cn.bing.com/search...", so be a bit flexible in what we match.
        if (redirectWinSearch &&
            (uriScheme == "http" || uriScheme == "https") &&
            uriHost.endsWith(".bing.com") && uriPath.startsWith("/search")) {
          try {
            var url = uri.QueryInterface(Components.interfaces.nsIURL);
            var params = new URLSearchParams(url.query);
            // We don't want to rewrite all Bing URLs coming from external apps. Look
            // for the magic URL parm that's present in searches from the task bar.
            // * Typed searches use "form=WNSGPH"
            // * Cortana voice searches use "FORM=WNSBOX" or direct results, or "FORM=WNSFC2"
            //   for "see more results on Bing.com")
            // * Cortana voice searches started from "Hey, Cortana" use "form=WNSHCO"
            //   or "form=WNSSSV" or "form=WNSSCX"
            var allowedParams = ["WNSGPH", "WNSBOX", "WNSFC2", "WNSHCO", "WNSSCX", "WNSSSV"];
            var formParam = params.get("form");
            if (!formParam) {
              formParam = params.get("FORM");
            }
            if (allowedParams.indexOf(formParam) != -1) {
              var term = params.get("q");
              var engine = Services.search.defaultEngine;
              logSystemBasedSearch(engine);
              var submission = engine.getSubmission(term, null, "system");
              uri = submission.uri;
            }
          } catch (e) {
            Components.utils.reportError("Couldn't redirect Windows search: " + e);
          }
        }

        urilist.push(uri);
      }
    }
    catch (e) {
      Components.utils.reportError(e);
    }

    for (let i = 0; i < cmdLine.length; ++i) {
      var curarg = cmdLine.getArgument(i);
      if (curarg.match(/^-/)) {
        Components.utils.reportError("Warning: unrecognized command line flag " + curarg + "\n");
        // To emulate the pre-nsICommandLine behavior, we ignore
        // the argument after an unrecognized flag.
        ++i;
      } else {
        try {
          urilist.push(resolveURIInternal(cmdLine, curarg));
        }
        catch (e) {
          Components.utils.reportError("Error opening URI '" + curarg + "' from the command line: " + e + "\n");
        }
      }
    }

    if (urilist.length) {
      if (cmdLine.state != nsICommandLine.STATE_INITIAL_LAUNCH &&
          urilist.length == 1) {
        // Try to find an existing window and load our URI into the
        // current tab, new tab, or new window as prefs determine.
        try {
          handURIToExistingBrowser(urilist[0], nsIBrowserDOMWindow.OPEN_DEFAULTWINDOW, cmdLine);
          return;
        }
        catch (e) {
        }
      }

      var URLlist = urilist.filter(shouldLoadURI).map(u => u.spec);
      if (URLlist.length) {
        openWindow(null, gBrowserContentHandler.chromeURL, "_blank",
                   "chrome,dialog=no,all" + gBrowserContentHandler.getFeatures(cmdLine),
                   URLlist);
      }

    }
    else if (!cmdLine.preventDefault) {
      if (AppConstants.isPlatformAndVersionAtLeast("win", "10") &&
          cmdLine.state != nsICommandLine.STATE_INITIAL_LAUNCH &&
          WindowsUIUtils.inTabletMode) {
        // In windows 10 tablet mode, do not create a new window, but reuse the existing one.
        let win = RecentWindow.getMostRecentBrowserWindow();
        if (win) {
          win.focus();
          return;
        }
      }
      // Passing defaultArgs, so use NO_EXTERNAL_URIS
      openWindow(null, gBrowserContentHandler.chromeURL, "_blank",
                 "chrome,dialog=no,all" + gBrowserContentHandler.getFeatures(cmdLine),
                 gBrowserContentHandler.defaultArgs, NO_EXTERNAL_URIS);
    }
  },

  helpInfo : "",
};

var components = [nsBrowserContentHandler, nsDefaultCommandLineHandler];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
