/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  HeadlessShell: "resource:///modules/HeadlessShell.jsm",
  LaterRun: "resource:///modules/LaterRun.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  ShellService: "resource:///modules/ShellService.jsm",
  UpdatePing: "resource://gre/modules/UpdatePing.jsm"
});
XPCOMUtils.defineLazyServiceGetter(this, "WindowsUIUtils",
  "@mozilla.org/windows-ui-utils;1", "nsIWindowsUIUtils");

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

  if (!(uri instanceof Ci.nsIFileURL)) {
    return uriFixup.createFixupURI(aArgument,
                                   uriFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS);
  }

  try {
    if (uri.file.exists())
      return uri;
  } catch (e) {
    Cu.reportError(e);
  }

  // We have interpreted the argument as a relative file URI, but the file
  // doesn't exist. Try URI fixup heuristics: see bug 290782.

  try {
    uri = uriFixup.createFixupURI(aArgument, 0);
  } catch (e) {
    Cu.reportError(e);
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
  var savedmstone = prefb.getCharPref("browser.startup.homepage_override.mstone", "");

  if (savedmstone == "ignore")
    return OVERRIDE_NONE;

  var mstone = Services.appinfo.platformVersion;

  var savedBuildID = prefb.getCharPref("browser.startup.homepage_override.buildID", "");

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
  var um = Cc["@mozilla.org/updates/update-manager;1"]
             .getService(Ci.nsIUpdateManager);
  // The active update should be present when this code is called. If for
  // whatever reason it isn't fallback to the latest update in the update
  // history.
  if (um.activeUpdate) {
    var update = um.activeUpdate
                   .QueryInterface(Ci.nsIPropertyBag);
  } else {
    // If the updates.xml file is deleted then getUpdateAt will throw.
    try {
      update = um.getUpdateAt(0)
                 .QueryInterface(Ci.nsIPropertyBag);
    } catch (e) {
      Cu.reportError("Unable to find update: " + e);
      return defaultOverridePage;
    }
  }

  let actions = update.getProperty("actions");
  // When the update doesn't specify actions fallback to the original behavior
  // of displaying the default override page.
  if (!actions)
    return defaultOverridePage;

  // The existence of silent or the non-existence of showURL in the actions both
  // mean that an override page should not be displayed.
  if (actions.includes("silent") || !actions.includes("showURL"))
    return "";

  // If a policy was set to not allow the update.xml-provided
  // URL to be used, use the default fallback (which will also
  // be provided by the policy).
  if (!Services.policies.isAllowed("postUpdateCustomPage")) {
    return defaultOverridePage;
  }

  return update.getProperty("openURL") || defaultOverridePage;
}

/**
 * Open a browser window. If this is the initial launch, this function will
 * attempt to use the navigator:blank window opened by nsBrowserGlue.js during
 * early startup.
 *
 * @param cmdLine
 *        The nsICommandLine object given to nsICommandLineHandler's handle
 *        method.
 *        Used to check if we are processing the command line for the initial launch.
 * @param urlOrUrlList (optional)
 *        When omitted, the browser window will be opened with the default
 *        arguments, which will usually load the homepage.
 *        This can be a JS array of urls provided as strings, each url will be
 *        loaded in a tab. postData will be ignored in this case.
 *        This can be a single url to load in the new window, provided as a string.
 *        postData will be used in this case if provided.
 * @param postData (optional)
 *        An nsIInputStream object to use as POST data when loading the provided
 *        url, or null.
 * @param forcePrivate (optional)
 *        Boolean. If set to true, the new window will be a private browsing one.
 */
function openBrowserWindow(cmdLine, urlOrUrlList, postData = null,
                           forcePrivate = false) {
  let chromeURL = Services.prefs.getCharPref("browser.chromeURL");

  let args;
  if (!urlOrUrlList) {
    // Just pass in the defaultArgs directly
    args = [gBrowserContentHandler.defaultArgs];
  } else if (Array.isArray(urlOrUrlList)) {
    // Passing an nsIArray for the url disables the "|"-splitting behavior.
    let uriArray = Cc["@mozilla.org/array;1"]
                     .createInstance(Ci.nsIMutableArray);
    urlOrUrlList.forEach(function(uri) {
      var sstring = Cc["@mozilla.org/supports-string;1"]
                      .createInstance(Ci.nsISupportsString);
      sstring.data = uri;
      uriArray.appendElement(sstring);
    });
    args = [uriArray];
  } else {
    // Always pass at least 3 arguments to avoid the "|"-splitting behavior,
    // ie. avoid the loadOneOrMoreURIs function.
    args = [urlOrUrlList,
            null, // charset
            null, // referer
            postData];
  }

  if (cmdLine.state == Ci.nsICommandLine.STATE_INITIAL_LAUNCH) {
    let win = Services.wm.getMostRecentWindow("navigator:blank");
    if (win) {
      // Remove the windowtype of our blank window so that we don't close it
      // later on when seeing cmdLine.preventDefault is true.
      win.document.documentElement.removeAttribute("windowtype");

      if (forcePrivate) {
        win.QueryInterface(Ci.nsIInterfaceRequestor)
           .getInterface(Ci.nsIWebNavigation)
           .QueryInterface(Ci.nsILoadContext)
           .usePrivateBrowsing = true;
      }

      win.location = chromeURL;
      win.arguments = args; // <-- needs to be a plain JS array here.

      return;
    }
  }

  // We can't provide arguments to openWindow as a JS array.
  if (!urlOrUrlList) {
    // If we have a single string guaranteed to not contain '|' we can simply
    // wrap it in an nsISupportsString object.
    let [url] = args;
    args = Cc["@mozilla.org/supports-string;1"]
             .createInstance(Ci.nsISupportsString);
    args.data = url;
  } else {
    // Otherwise, pass an nsIArray.
    if (args.length > 1) {
      let string = Cc["@mozilla.org/supports-string;1"]
                     .createInstance(Ci.nsISupportsString);
      string.data = args[0];
      args[0] = string;
    }
    let array = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
    args.forEach(a => { array.appendElement(a); });
    args = array;
  }

  let features = "chrome,dialog=no,all" + gBrowserContentHandler.getFeatures(cmdLine);
  if (forcePrivate)
    features += ",private";

  Services.ww.openWindow(null, chromeURL, "_blank", features, args);
}

function openPreferences(cmdLine, extraArgs) {
  if (extraArgs && extraArgs.origin) {
    Services.telemetry.getHistogramById("FX_PREFERENCES_OPENED_VIA").add(extraArgs.origin);
  } else {
    Services.telemetry.getHistogramById("FX_PREFERENCES_OPENED_VIA").add("other");
  }
  openBrowserWindow(cmdLine, "about:preferences");
}

function doSearch(searchTerm, cmdLine) {
  var engine = Services.search.defaultEngine;
  var countId = (engine.identifier || ("other-" + engine.name)) + ".system";
  var count = Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS");
  count.add(countId);

  var submission = engine.getSubmission(searchTerm, null, "system");

  // XXXbsmedberg: use handURIToExistingBrowser to obey tabbed-browsing
  // preferences, but need nsIBrowserDOMWindow extensions
  openBrowserWindow(cmdLine, submission.uri.spec, submission.postData);
}

function nsBrowserContentHandler() {
}
nsBrowserContentHandler.prototype = {
  classID: Components.ID("{5d0ce354-df01-421a-83fb-7ead0990c24e}"),

  _xpcom_factory: {
    createInstance: function bch_factory_ci(outer, iid) {
      if (outer)
        throw Cr.NS_ERROR_NO_AGGREGATION;
      return gBrowserContentHandler.QueryInterface(iid);
    }
  },

  /* nsISupports */
  QueryInterface: XPCOMUtils.generateQI([Ci.nsICommandLineHandler,
                                         Ci.nsIBrowserHandler,
                                         Ci.nsIContentHandler,
                                         Ci.nsICommandLineValidator]),

  /* nsICommandLineHandler */
  handle: function bch_handle(cmdLine) {
    if (cmdLine.handleFlag("browser", false)) {
      openBrowserWindow(cmdLine);
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
      throw Cr.NS_ERROR_ABORT;
    }

    var uriparam;
    try {
      while ((uriparam = cmdLine.handleFlagWithParam("new-window", false))) {
        let uri = resolveURIInternal(cmdLine, uriparam);
        if (!shouldLoadURI(uri))
          continue;
        openBrowserWindow(cmdLine, uri.spec);
        cmdLine.preventDefault = true;
      }
    } catch (e) {
      Cu.reportError(e);
    }

    try {
      while ((uriparam = cmdLine.handleFlagWithParam("new-tab", false))) {
        let uri = resolveURIInternal(cmdLine, uriparam);
        handURIToExistingBrowser(uri, Ci.nsIBrowserDOMWindow.OPEN_NEWTAB,
                                 cmdLine, false,
                                 Services.scriptSecurityManager.getSystemPrincipal());
        cmdLine.preventDefault = true;
      }
    } catch (e) {
      Cu.reportError(e);
    }

    var chromeParam = cmdLine.handleFlagWithParam("chrome", false);
    if (chromeParam) {

      // Handle old preference dialog URLs.
      if (chromeParam == "chrome://browser/content/pref/pref.xul" ||
          chromeParam == "chrome://browser/content/preferences/preferences.xul") {
        openPreferences(cmdLine, {origin: "commandLineLegacy"});
        cmdLine.preventDefault = true;
      } else try {
        let resolvedURI = resolveURIInternal(cmdLine, chromeParam);
        let isLocal = uri => {
          let localSchemes = new Set(["chrome", "file", "resource"]);
          if (uri instanceof Ci.nsINestedURI) {
            uri = uri.QueryInterface(Ci.nsINestedURI).innerMostURI;
          }
          return localSchemes.has(uri.scheme);
        };
        if (isLocal(resolvedURI)) {
          // If the URI is local, we are sure it won't wrongly inherit chrome privs
          let features = "chrome,dialog=no,all" + this.getFeatures(cmdLine);
          // Provide 1 null argument, as openWindow has a different behavior
          // when the arg count is 0.
          let argArray = Cc["@mozilla.org/array;1"]
                           .createInstance(Ci.nsIMutableArray);
          argArray.appendElement(null);
          Services.ww.openWindow(null, resolvedURI.spec, "_blank", features, argArray);
          cmdLine.preventDefault = true;
        } else {
          dump("*** Preventing load of web URI as chrome\n");
          dump("    If you're trying to load a webpage, do not pass --chrome.\n");
        }
      } catch (e) {
        Cu.reportError(e);
      }
    }
    if (cmdLine.handleFlag("preferences", false)) {
      openPreferences(cmdLine, {origin: "commandLineLegacy"});
      cmdLine.preventDefault = true;
    }
    if (cmdLine.handleFlag("silent", false))
      cmdLine.preventDefault = true;

    try {
      var privateWindowParam = cmdLine.handleFlagWithParam("private-window", false);
      if (privateWindowParam) {
        let forcePrivate = true;
        let resolvedURI;
        if (!PrivateBrowsingUtils.enabled) {
          // Load about:privatebrowsing in a normal tab, which will display an error indicating
          // access to private browsing has been disabled.
          forcePrivate = false;
          resolvedURI = Services.io.newURI("about:privatebrowsing");
        } else {
          resolvedURI = resolveURIInternal(cmdLine, privateWindowParam);
        }
        handURIToExistingBrowser(resolvedURI, Ci.nsIBrowserDOMWindow.OPEN_NEWTAB,
                                 cmdLine, forcePrivate,
                                 Services.scriptSecurityManager.getSystemPrincipal());
        cmdLine.preventDefault = true;
      }
    } catch (e) {
      if (e.result != Cr.NS_ERROR_INVALID_ARG) {
        throw e;
      }
      // NS_ERROR_INVALID_ARG is thrown when flag exists, but has no param.
      if (cmdLine.handleFlag("private-window", false)) {
        openBrowserWindow(cmdLine, "about:privatebrowsing", null,
                          PrivateBrowsingUtils.enabled);
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
    if (cmdLine.handleFlag("private", false) && PrivateBrowsingUtils.enabled) {
      PrivateBrowsingUtils.enterTemporaryAutoStartMode();
      if (cmdLine.state == Ci.nsICommandLine.STATE_INITIAL_LAUNCH) {
        let win = Services.wm.getMostRecentWindow("navigator:blank");
        if (win) {
          win.QueryInterface(Ci.nsIInterfaceRequestor)
             .getInterface(Ci.nsIWebNavigation)
             .QueryInterface(Ci.nsILoadContext)
             .usePrivateBrowsing = true;
        }
      }
    }
    if (cmdLine.handleFlag("setDefaultBrowser", false)) {
      ShellService.setDefaultBrowser(true, true);
    }

    var fileParam = cmdLine.handleFlagWithParam("file", false);
    if (fileParam) {
      var file = cmdLine.resolveFile(fileParam);
      var fileURI = Services.io.newFileURI(file);
      openBrowserWindow(cmdLine, fileURI.spec);
      cmdLine.preventDefault = true;
    }

    if (AppConstants.platform == "win") {
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
    info += "  --screenshot [<path>] Save screenshot to <path> or in working directory.\n";
    info += "  --window-size width[,height] Width and optionally height of screenshot.\n";
    info += "  --search <term>    Search <term> with your default search engine.\n";
    info += "  --setDefaultBrowser Set this app as the default browser.\n";
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
      let old_mstone = Services.prefs.getCharPref("browser.startup.homepage_override.mstone", "unknown");
      let old_buildId = Services.prefs.getCharPref("browser.startup.homepage_override.buildID", "unknown");
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
            var ss = Cc["@mozilla.org/browser/sessionstartup;1"]
                       .getService(Ci.nsISessionStartup);
            willRestoreSession = ss.isAutomaticRestoreEnabled();

            overridePage = Services.urlFormatter.formatURLPref("startup.homepage_override_url");
            if (prefb.prefHasUserValue("app.update.postupdate")) {
              overridePage = getPostUpdateOverridePage(overridePage);
              // Send the update ping to signal that the update was successful.
              UpdatePing.handleUpdateSuccess(old_mstone, old_buildId);
            }

            overridePage = overridePage.replace("%OLD_VERSION%", old_mstone);
            break;
          case OVERRIDE_NEW_BUILD_ID:
            if (prefb.prefHasUserValue("app.update.postupdate")) {
              // Send the update ping to signal that the update was successful.
              UpdatePing.handleUpdateSuccess(old_mstone, old_buildId);
            }
            break;
        }
      }
    } catch (ex) {}

    // formatURLPref might return "about:blank" if getting the pref fails
    if (overridePage == "about:blank")
      overridePage = "";

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
      Cu.reportError(e);
    }

    if (startPage == "about:blank")
      startPage = "";

    let skipStartPage = override == OVERRIDE_NEW_PROFILE &&
      prefb.getBoolPref("browser.startup.firstrunSkipsHomepage");
    // Only show the startPage if we're not restoring an update session and are
    // not set to skip the start page on this profile
    if (overridePage && startPage && !willRestoreSession && !skipStartPage)
      return overridePage + "|" + startPage;

    return overridePage || startPage || "about:blank";
  },

  get startPage() {
    var uri = Services.prefs.getComplexValue("browser.startup.homepage",
                                             Ci.nsIPrefLocalizedString).data;
    if (!uri) {
      Services.prefs.clearUserPref("browser.startup.homepage");
      uri = Services.prefs.getComplexValue("browser.startup.homepage",
                                           Ci.nsIPrefLocalizedString).data;
    }
    return uri;
  },

  mFeatures: null,

  getFeatures: function bch_features(cmdLine) {
    if (this.mFeatures === null) {
      this.mFeatures = "";

      try {
        var width = cmdLine.handleFlagWithParam("width", false);
        var height = cmdLine.handleFlagWithParam("height", false);

        if (width)
          this.mFeatures += ",width=" + width;
        if (height)
          this.mFeatures += ",height=" + height;
      } catch (e) {
      }

      // The global PB Service consumes this flag, so only eat it in per-window
      // PB builds.
      if (PrivateBrowsingUtils.isInTemporaryAutoStartMode) {
        this.mFeatures += ",private";
      }

      if (Services.prefs.getBoolPref("browser.suppress_first_window_animation") &&
          !Services.wm.getMostRecentWindow("navigator:browser")) {
        this.mFeatures += ",suppressanimation";
      }
    }

    return this.mFeatures;
  },

  /* nsIContentHandler */

  handleContent: function bch_handleContent(contentType, context, request) {
    const NS_ERROR_WONT_HANDLE_CONTENT = 0x805d0001;

    try {
      var webNavInfo = Cc["@mozilla.org/webnavigation-info;1"]
                         .getService(Ci.nsIWebNavigationInfo);
      if (!webNavInfo.isTypeSupported(contentType, null)) {
        throw NS_ERROR_WONT_HANDLE_CONTENT;
      }
    } catch (e) {
      throw NS_ERROR_WONT_HANDLE_CONTENT;
    }

    request.QueryInterface(Ci.nsIChannel);
    handURIToExistingBrowser(request.URI, Ci.nsIBrowserDOMWindow.OPEN_DEFAULTWINDOW,
                             null, false, request.loadInfo.triggeringPrincipal);
    request.cancel(Cr.NS_BINDING_ABORTED);
  },

  /* nsICommandLineValidator */
  validate: function bch_validate(cmdLine) {
    // Other handlers may use osint so only handle the osint flag if the url
    // flag is also present and the command line is valid.
    var osintFlagIdx = cmdLine.findFlag("osint", false);
    var urlFlagIdx = cmdLine.findFlag("url", false);
    if (urlFlagIdx > -1 && (osintFlagIdx > -1 ||
        cmdLine.state == Ci.nsICommandLine.STATE_REMOTE_EXPLICIT)) {
      var urlParam = cmdLine.getArgument(urlFlagIdx + 1);
      if (cmdLine.length != urlFlagIdx + 2 || /firefoxurl:/i.test(urlParam))
        throw Cr.NS_ERROR_ABORT;
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
        throw Cr.NS_ERROR_ABORT;
      }
      cmdLine.handleFlag("osint", false);
    }
  },
};
var gBrowserContentHandler = new nsBrowserContentHandler();

function handURIToExistingBrowser(uri, location, cmdLine, forcePrivate, triggeringPrincipal) {
  if (!shouldLoadURI(uri))
    return;

  // Unless using a private window is forced, open external links in private
  // windows only if we're in perma-private mode.
  var allowPrivate = forcePrivate || PrivateBrowsingUtils.permanentPrivateBrowsing;
  var navWin = BrowserWindowTracker.getTopWindow({private: allowPrivate});
  if (!navWin) {
    // if we couldn't load it in an existing window, open a new one
    openBrowserWindow(cmdLine, uri.spec, null, forcePrivate);
    return;
  }

  var navNav = navWin.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIWebNavigation);
  var rootItem = navNav.QueryInterface(Ci.nsIDocShellTreeItem).rootTreeItem;
  var rootWin = rootItem.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIDOMWindow);
  var bwin = rootWin.QueryInterface(Ci.nsIDOMChromeWindow).browserDOMWindow;
  bwin.openURI(uri, null, location,
               Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL, triggeringPrincipal);
}

function nsDefaultCommandLineHandler() {
}

nsDefaultCommandLineHandler.prototype = {
  classID: Components.ID("{47cd0651-b1be-4a0f-b5c4-10e5a573ef71}"),

  /* nsISupports */
  QueryInterface: function dch_QI(iid) {
    if (!iid.equals(Ci.nsISupports) &&
        !iid.equals(Ci.nsICommandLineHandler))
      throw Cr.NS_ERROR_NO_INTERFACE;

    return this;
  },

  _haveProfile: false,

  /* nsICommandLineHandler */
  handle: function dch_handle(cmdLine) {
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
          Services.dirsvc.get("ProfD", Ci.nsIFile);
          this._haveProfile = true;
        } catch (e) {
          while ((ar = cmdLine.handleFlagWithParam("url", false)));
          cmdLine.preventDefault = true;
        }
      }
    }

    try {
      var ar;
      while ((ar = cmdLine.handleFlagWithParam("url", false))) {
        var uri = resolveURIInternal(cmdLine, ar);
        urilist.push(uri);
      }
    } catch (e) {
      Cu.reportError(e);
    }

    if (cmdLine.findFlag("screenshot", true) != -1) {
      HeadlessShell.handleCmdLineArgs(cmdLine, urilist.filter(shouldLoadURI).map(u => u.spec));
      return;
    }

    for (let i = 0; i < cmdLine.length; ++i) {
      var curarg = cmdLine.getArgument(i);
      if (curarg.match(/^-/)) {
        Cu.reportError("Warning: unrecognized command line flag " + curarg + "\n");
        // To emulate the pre-nsICommandLine behavior, we ignore
        // the argument after an unrecognized flag.
        ++i;
      } else {
        try {
          urilist.push(resolveURIInternal(cmdLine, curarg));
        } catch (e) {
          Cu.reportError("Error opening URI '" + curarg + "' from the command line: " + e + "\n");
        }
      }
    }

    if (urilist.length) {
      if (cmdLine.state != Ci.nsICommandLine.STATE_INITIAL_LAUNCH &&
          urilist.length == 1) {
        // Try to find an existing window and load our URI into the
        // current tab, new tab, or new window as prefs determine.
        try {
          handURIToExistingBrowser(urilist[0], Ci.nsIBrowserDOMWindow.OPEN_DEFAULTWINDOW,
                                   cmdLine, false,
                                   Services.scriptSecurityManager.getSystemPrincipal());
          return;
        } catch (e) {
        }
      }

      var URLlist = urilist.filter(shouldLoadURI).map(u => u.spec);
      if (URLlist.length) {
        openBrowserWindow(cmdLine, URLlist);
      }

    } else if (!cmdLine.preventDefault) {
      if (AppConstants.isPlatformAndVersionAtLeast("win", "10") &&
          cmdLine.state != Ci.nsICommandLine.STATE_INITIAL_LAUNCH &&
          WindowsUIUtils.inTabletMode) {
        // In windows 10 tablet mode, do not create a new window, but reuse the existing one.
        let win = BrowserWindowTracker.getTopWindow();
        if (win) {
          win.focus();
          return;
        }
      }
      openBrowserWindow(cmdLine);
    } else {
      // Need a better solution in the future to avoid opening the blank window
      // when command line parameters say we are not going to show a browser
      // window, but for now the blank window getting closed quickly (and
      // causing only a slight flicker) is better than leaving it open.
      let win = Services.wm.getMostRecentWindow("navigator:blank");
      if (win)
        win.close();
    }
  },

  helpInfo: "",
};

var components = [nsBrowserContentHandler, nsDefaultCommandLineHandler];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
