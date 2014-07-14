# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "RecentWindow",
                                  "resource:///modules/RecentWindow.jsm");

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
const nsIPrefBranch          = Components.interfaces.nsIPrefBranch;
const nsIPrefLocalizedString = Components.interfaces.nsIPrefLocalizedString;
const nsISupportsString      = Components.interfaces.nsISupportsString;
const nsIURIFixup            = Components.interfaces.nsIURIFixup;
const nsIWebNavigation       = Components.interfaces.nsIWebNavigation;
const nsIWindowMediator      = Components.interfaces.nsIWindowMediator;
const nsIWindowWatcher       = Components.interfaces.nsIWindowWatcher;
const nsIWebNavigationInfo   = Components.interfaces.nsIWebNavigationInfo;
const nsIBrowserSearchService = Components.interfaces.nsIBrowserSearchService;
const nsICommandLineValidator = Components.interfaces.nsICommandLineValidator;

const NS_BINDING_ABORTED = Components.results.NS_BINDING_ABORTED;
const NS_ERROR_WONT_HANDLE_CONTENT = 0x805d0001;
const NS_ERROR_ABORT = Components.results.NS_ERROR_ABORT;

const URI_INHERITS_SECURITY_CONTEXT = Components.interfaces.nsIHttpProtocolHandler
                                        .URI_INHERITS_SECURITY_CONTEXT;

function shouldLoadURI(aURI) {
  if (aURI && !aURI.schemeIs("chrome"))
    return true;

  dump("*** Preventing external load of chrome: URI into browser window\n");
  dump("    Use -chrome <uri> instead\n");
  return false;
}

function resolveURIInternal(aCmdLine, aArgument) {
  var uri = aCmdLine.resolveURI(aArgument);
  var urifixup = Components.classes["@mozilla.org/docshell/urifixup;1"]
                           .getService(nsIURIFixup);

  if (!(uri instanceof nsIFileURL)) {
    return urifixup.createFixupURI(aArgument,
                                   urifixup.FIXUP_FLAG_FIX_SCHEME_TYPOS);
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
    uri = urifixup.createFixupURI(aArgument, 0);
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
  var wwatch = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                         .getService(nsIWindowWatcher);

  if (noExternalArgs == NO_EXTERNAL_URIS) {
    // Just pass in the defaultArgs directly
    var argstring;
    if (args) {
      argstring = Components.classes["@mozilla.org/supports-string;1"]
                            .createInstance(nsISupportsString);
      argstring.data = args;
    }

    return wwatch.openWindow(parent, url, target, features, argstring);
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

  return wwatch.openWindow(parent, url, target, features, argArray);
}

function openPreferences() {
  if (Services.prefs.getBoolPref("browser.preferences.inContent")) { 
    var sa = Components.classes["@mozilla.org/supports-array;1"]
                       .createInstance(Components.interfaces.nsISupportsArray);

    var wuri = Components.classes["@mozilla.org/supports-string;1"]
                         .createInstance(Components.interfaces.nsISupportsString);
    wuri.data = "about:preferences";

    sa.AppendElement(wuri);

    var wwatch = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                           .getService(nsIWindowWatcher);

    wwatch.openWindow(null, gBrowserContentHandler.chromeURL,
                      "_blank",
                      "chrome,dialog=no,all",
                      sa);
  } else {
    var features = "chrome,titlebar,toolbar,centerscreen,dialog=no";
    var url = "chrome://browser/content/preferences/preferences.xul";
    
    var win = getMostRecentWindow("Browser:Preferences");
    if (win) {
      win.focus();
    } else {
      openWindow(null, url, "_blank", features);
    }
  }
}

function getMostRecentWindow(aType) {
  var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                     .getService(nsIWindowMediator);
  return wm.getMostRecentWindow(aType);
}

function doSearch(searchTerm, cmdLine) {
  var ss = Components.classes["@mozilla.org/browser/search-service;1"]
                     .getService(nsIBrowserSearchService);

  var submission = ss.defaultEngine.getSubmission(searchTerm);

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

  var wwatch = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                         .getService(nsIWindowWatcher);

  return wwatch.openWindow(null, gBrowserContentHandler.chromeURL,
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

    var prefb = Components.classes["@mozilla.org/preferences-service;1"]
                          .getService(nsIPrefBranch);
    this.mChromeURL = prefb.getCharPref("browser.chromeURL");

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

    try {
      var remoteCommand = cmdLine.handleFlagWithParam("remote", true);
    }
    catch (e) {
      throw NS_ERROR_ABORT;
    }

    if (remoteCommand != null) {
      try {
        var a = /^\s*(\w+)\(([^\)]*)\)\s*$/.exec(remoteCommand);
        var remoteVerb;
        if (a) {
          remoteVerb = a[1].toLowerCase();
          var remoteParams = [];
          var sepIndex = a[2].lastIndexOf(",");
          if (sepIndex == -1)
            remoteParams[0] = a[2];
          else {
            remoteParams[0] = a[2].substring(0, sepIndex);
            remoteParams[1] = a[2].substring(sepIndex + 1);
          }
        }

        switch (remoteVerb) {
        case "openurl":
        case "openfile":
          // openURL(<url>)
          // openURL(<url>,new-window)
          // openURL(<url>,new-tab)

          // First param is the URL, second param (if present) is the "target"
          // (tab, window)
          var url = remoteParams[0];
          var target = nsIBrowserDOMWindow.OPEN_DEFAULTWINDOW;
          if (remoteParams[1]) {
            var targetParam = remoteParams[1].toLowerCase()
                                             .replace(/^\s*|\s*$/g, "");
            if (targetParam == "new-tab")
              target = nsIBrowserDOMWindow.OPEN_NEWTAB;
            else if (targetParam == "new-window")
              target = nsIBrowserDOMWindow.OPEN_NEWWINDOW;
            else {
              // The "target" param isn't one of our supported values, so
              // assume it's part of a URL that contains commas.
              url += "," + remoteParams[1];
            }
          }

          var uri = resolveURIInternal(cmdLine, url);
          handURIToExistingBrowser(uri, target, cmdLine);
          break;

        case "xfedocommand":
          // xfeDoCommand(openBrowser)
          if (remoteParams[0].toLowerCase() != "openbrowser")
            throw NS_ERROR_ABORT;

          // Passing defaultArgs, so use NO_EXTERNAL_URIS
          openWindow(null, this.chromeURL, "_blank",
                     "chrome,dialog=no,all" + this.getFeatures(cmdLine),
                     this.defaultArgs, NO_EXTERNAL_URIS);
          break;

        default:
          // Somebody sent us a remote command we don't know how to process:
          // just abort.
          throw "Unknown remote command.";
        }

        cmdLine.preventDefault = true;
      }
      catch (e) {
        Components.utils.reportError(e);
        // If we had a -remote flag but failed to process it, throw
        // NS_ERROR_ABORT so that the xremote code knows to return a failure
        // back to the handling code.
        throw NS_ERROR_ABORT;
      }
    }

    var uriparam;
    try {
      while ((uriparam = cmdLine.handleFlagWithParam("new-window", false))) {
        var uri = resolveURIInternal(cmdLine, uriparam);
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
        var uri = resolveURIInternal(cmdLine, uriparam);
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
          (Services.prefs.getBoolPref("browser.preferences.inContent") &&
           chromeParam == "chrome://browser/content/preferences/preferences.xul")) {
        openPreferences();
        cmdLine.preventDefault = true;
      } else try {
        // only load URIs which do not inherit chrome privs
        var features = "chrome,dialog=no,all" + this.getFeatures(cmdLine);
        var uri = resolveURIInternal(cmdLine, chromeParam);
        var netutil = Components.classes["@mozilla.org/network/util;1"]
                                .getService(nsINetUtil);
        if (!netutil.URIChainHasFlags(uri, URI_INHERITS_SECURITY_CONTEXT)) {
          openWindow(null, uri.spec, "_blank", features);
          cmdLine.preventDefault = true;
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
        var uri = resolveURIInternal(cmdLine, privateWindowParam);
        handURIToExistingBrowser(uri, nsIBrowserDOMWindow.OPEN_NEWTAB, cmdLine, true);
        cmdLine.preventDefault = true;
      }
    } catch (e if e.result == Components.results.NS_ERROR_INVALID_ARG) {
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
      var ios = Components.classes["@mozilla.org/network/io-service;1"]
                          .getService(Components.interfaces.nsIIOService);
      var uri = ios.newFileURI(file);
      openWindow(null, this.chromeURL, "_blank", 
                 "chrome,dialog=no,all" + this.getFeatures(cmdLine),
                 uri.spec);
      cmdLine.preventDefault = true;
    }

#ifdef XP_WIN
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
#endif
  },

  helpInfo : "  -browser           Open a browser window.\n" +
             "  -new-window  <url> Open <url> in a new window.\n" +
             "  -new-tab     <url> Open <url> in a new tab.\n" +
             "  -private-window <url> Open <url> in a new private window.\n" +
#ifdef XP_WIN
             "  -preferences       Open Options dialog.\n" +
#else
             "  -preferences       Open Preferences dialog.\n" +
#endif
             "  -search     <term> Search <term> with your default search engine.\n",

  /* nsIBrowserHandler */

  get defaultArgs() {
    var prefb = Components.classes["@mozilla.org/preferences-service;1"]
                          .getService(nsIPrefBranch);

    if (!gFirstWindow) {
      gFirstWindow = true;
      if (PrivateBrowsingUtils.isInTemporaryAutoStartMode) {
        return "about:privatebrowsing";
      }
    }

    var overridePage = "";
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
      let override = needHomepageOverride(prefb);
      if (override != OVERRIDE_NONE) {
        switch (override) {
          case OVERRIDE_NEW_PROFILE:
            // New profile.
            overridePage = Services.urlFormatter.formatURLPref("startup.homepage_welcome_url");
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

#ifdef XP_WIN
  _haveProfile: false,
#endif

  /* nsICommandLineHandler */
  handle : function dch_handle(cmdLine) {
    var urilist = [];

#ifdef XP_WIN
    // If we don't have a profile selected yet (e.g. the Profile Manager is
    // displayed) we will crash if we open an url and then select a profile. To
    // prevent this handle all url command line flags and set the command line's
    // preventDefault to true to prevent the display of the ui. The initial
    // command line will be retained when nsAppRunner calls LaunchChild though
    // urls launched after the initial launch will be lost.
    if (!this._haveProfile) {
      try {
        // This will throw when a profile has not been selected.
        var fl = Components.classes["@mozilla.org/file/directory_service;1"]
                           .getService(Components.interfaces.nsIProperties);
        var dir = fl.get("ProfD", Components.interfaces.nsILocalFile);
        this._haveProfile = true;
      }
      catch (e) {
        while ((ar = cmdLine.handleFlagWithParam("url", false))) { }
        cmdLine.preventDefault = true;
      }
    }
#endif

    try {
      var ar;
      while ((ar = cmdLine.handleFlagWithParam("url", false))) {
        var uri = resolveURIInternal(cmdLine, ar);
        urilist.push(uri);
      }
    }
    catch (e) {
      Components.utils.reportError(e);
    }

    count = cmdLine.length;

    for (i = 0; i < count; ++i) {
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

      var URLlist = urilist.filter(shouldLoadURI).map(function (u) u.spec);
      if (URLlist.length) {
        openWindow(null, gBrowserContentHandler.chromeURL, "_blank",
                   "chrome,dialog=no,all" + gBrowserContentHandler.getFeatures(cmdLine),
                   URLlist);
      }

    }
    else if (!cmdLine.preventDefault) {
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
