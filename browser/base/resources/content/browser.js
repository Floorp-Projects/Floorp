/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Blake Ross <blaker@netscape.com>
 *   Peter Annema <disttsc@bart.nl>
 *   Dean Tessman <dean_tessman@hotmail.com>
 *   Kevin Puetz (puetzk@iastate.edu)
 *   Ben Goodger <ben@netscape.com> 
 *   Pierre Chanial <pierrechanial@netscape.net>
 *   Jason Eager <jce2@po.cwru.edu>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const XREMOTESERVICE_CONTRACTID = "@mozilla.org/browser/xremoteservice;1";
var gURLBar = null;
var gProxyButton = null;
var gProxyFavIcon = null;
var gProxyDeck = null;
var gBookmarksService = null;
var gSearchService = null;
var gNavigatorBundle;
var gBrandBundle;
var gNavigatorRegionBundle;
var gBrandRegionBundle;
var gLastValidURLStr = "";
var gLastValidURL = null;
var gHaveUpdatedToolbarState = false;
var gClickSelectsAll = false;
var gIgnoreFocus = false;
var gIgnoreClick = false;

var pref = null;

var appCore = null;

//cached elements
var gBrowser = null;

// focused frame URL
var gFocusedURL = null;
var gFocusedDocument = null;

// Global variable that holds the nsContextMenu instance.
var gContextMenu = null;

// Global variable that caches the default search engine info
var gDefaultEngine = null;

var gBookmarkPopup = null;

const nsIWebNavigation = Components.interfaces.nsIWebNavigation;
var gPrintSettingsAreGlobal = true;
var gSavePrintSettings = true;
var gPrintSettings = null;
var gChromeState = null; // chrome state before we went into print preview
var gOldCloseHandler = null; // close handler before we went into print preview
var gInPrintPreviewMode = false;
var gWebProgress        = null;


// Pref listener constants
const gButtonPrefListener =
{
  domain: "browser.toolbars.showbutton",
  observe: function(subject, topic, prefName)
  {
    // verify that we're changing a button pref
    if (topic != "nsPref:changed")
      return;

    var buttonName = prefName.substr(this.domain.length+1);
    var buttonId = buttonName + "-button";
    var button = document.getElementById(buttonId);

    // We need to explicitly set "hidden" to "false"
    // in order for persistence to work correctly
    var show = pref.getBoolPref(prefName);
    if (show)
      button.setAttribute("hidden","false");
    else
      button.setAttribute("hidden", "true");

    // If all buttons before the separator are hidden, also hide the separator
    if (allLeftButtonsAreHidden())
      document.getElementById("home-bm-separator").setAttribute("hidden", "true");
    else
      document.getElementById("home-bm-separator").removeAttribute("hidden");
  }
};

const gTabStripPrefListener =
{
  domain: "browser.tabs.autoHide",
  observe: function(subject, topic, prefName)
  {
    // verify that we're changing the tab browser strip auto hide pref
    if (topic != "nsPref:changed")
      return;

    var stripVisibility = !pref.getBoolPref(prefName);
    if (gBrowser.mTabContainer.childNodes.length == 1) {
      gBrowser.setStripVisibilityTo(stripVisibility);
      pref.setBoolPref("browser.tabs.forceHide", false);
    }
  }
};

/**
* Pref listener handler functions.
* Both functions assume that observer.domain is set to 
* the pref domain we want to start/stop listening to.
*/
function addPrefListener(observer)
{
  try {
    var pbi = pref.QueryInterface(Components.interfaces.nsIPrefBranchInternal);
    pbi.addObserver(observer.domain, observer, false);
  } catch(ex) {
    dump("Failed to observe prefs: " + ex + "\n");
  }
}

function removePrefListener(observer)
{
  try {
    var pbi = pref.QueryInterface(Components.interfaces.nsIPrefBranchInternal);
    pbi.removeObserver(observer.domain, observer);
  } catch(ex) {
    dump("Failed to remove pref observer: " + ex + "\n");
  }
}

/**
* We can avoid adding multiple load event listeners and save some time by adding
* one listener that calls all real handlers.
*/

function loadEventHandlers(event)
{
  // Filter out events that are not about the document load we are interested in
  if (event.originalTarget == _content.document) {
    UpdateBookmarksLastVisitedDate(event);
    UpdateInternetSearchResults(event);
    checkForDirectoryListing();
    postURLToNativeWidget();
  }
}

/**
 * Determine whether or not the content area is displaying a page with frames,
 * and if so, toggle the display of the 'save frame as' menu item.
 **/
function getContentAreaFrameCount()
{
  var saveFrameItem = document.getElementById("savepage");
  if (!_content.frames.length || !isDocumentFrame(document.commandDispatcher.focusedWindow))
    saveFrameItem.setAttribute("hidden", "true");
  else
    saveFrameItem.removeAttribute("hidden");
}

// When a content area frame is focused, update the focused frame URL
function contentAreaFrameFocus()
{
  var focusedWindow = document.commandDispatcher.focusedWindow;
  if (isDocumentFrame(focusedWindow)) {
    gFocusedURL = focusedWindow.location.href;
    gFocusedDocument = focusedWindow.document;
  }
}

//////////////////////////////// BOOKMARKS ////////////////////////////////////

function UpdateBookmarksLastVisitedDate(event)
{
  var url = getWebNavigation().currentURI.spec;
  if (url) {
    // if the URL is bookmarked, update its "Last Visited" date
    if (!gBookmarksService)
      gBookmarksService = Components.classes["@mozilla.org/browser/bookmarks-service;1"]
                                    .getService(Components.interfaces.nsIBookmarksService);

    gBookmarksService.updateLastVisitedDate(url, _content.document.characterSet);
  }
}

function HandleBookmarkIcon(iconURL, addFlag)
{
  var url = getWebNavigation().currentURI.spec
  if (url) {
    // update URL with new icon reference
    if (!gBookmarksService)
      gBookmarksService = Components.classes["@mozilla.org/browser/bookmarks-service;1"]
                                    .getService(Components.interfaces.nsIBookmarksService);
    if (addFlag)    gBookmarksService.updateBookmarkIcon(url, iconURL);
    else            gBookmarksService.removeBookmarkIcon(url, iconURL);
  }
}

function UpdateInternetSearchResults(event)
{
  var url = getWebNavigation().currentURI.spec;
  if (url) {
    try {
      var autoOpenSearchPanel = 
        pref.getBoolPref("browser.search.opensidebarsearchpanel");

      if (autoOpenSearchPanel || isSearchPanelOpen())
      {
        if (!gSearchService)
          gSearchService = Components.classes["@mozilla.org/rdf/datasource;1?name=internetsearch"]
                                         .getService(Components.interfaces.nsIInternetSearchService);

        var searchInProgressFlag = gSearchService.FindInternetSearchResults(url);

        if (searchInProgressFlag) {
          if (autoOpenSearchPanel)
            RevealSearchPanel();
        }
      }
    } catch (ex) {
    }
  }
}

function getBrowser()
{
  if (!gBrowser)
    gBrowser = document.getElementById("content");
  return gBrowser;
}

function getHomePage()
{
  var url;
  try {
    url = pref.getComplexValue("browser.startup.homepage",
                               Components.interfaces.nsIPrefLocalizedString).data;
  } catch (e) {
  }

  // use this if we can't find the pref
  if (!url)
    url = gNavigatorRegionBundle.getString("homePageDefault");

  return url;
}

function UpdateBackForwardButtons()
{
  var backBroadcaster = document.getElementById("canGoBack");
  var forwardBroadcaster = document.getElementById("canGoForward");
  var webNavigation = getWebNavigation();

  // Avoid setting attributes on broadcasters if the value hasn't changed!
  // Remember, guys, setting attributes on elements is expensive!  They
  // get inherited into anonymous content, broadcast to other widgets, etc.!
  // Don't do it if the value hasn't changed! - dwh

  var backDisabled = backBroadcaster.hasAttribute("disabled");
  var forwardDisabled = forwardBroadcaster.hasAttribute("disabled");
  if (backDisabled == webNavigation.canGoBack) {
    if (backDisabled)
      backBroadcaster.removeAttribute("disabled");
    else
      backBroadcaster.setAttribute("disabled", true);
  }
  if (forwardDisabled == webNavigation.canGoForward) {
    if (forwardDisabled)
      forwardBroadcaster.removeAttribute("disabled");
    else
      forwardBroadcaster.setAttribute("disabled", true);
  }
}

// Function allLeftButtonsAreHidden
// Returns true if all the buttons left of the separator in the personal
// toolbar are hidden, false otherwise.
// Used by nsButtonPrefListener to hide the separator if needed
function allLeftButtonsAreHidden()
{
  var buttonNode = document.getElementById("PersonalToolbar").firstChild;
  while(buttonNode.tagName != "toolbarseparator") {
    if(!buttonNode.hasAttribute("hidden") || buttonNode.getAttribute("hidden") == "false")
      return false;
    buttonNode = buttonNode.nextSibling;
  }
  return true;
}

function RegisterTabOpenObserver()
{
  const observer = {
    observe: function(subject, topic, data)
    {
      if (topic != "open-new-tab-request" || subject != window)
        return;

      delayedOpenTab(data);
    }
  };

  const service = Components.classes["@mozilla.org/observer-service;1"]
    .getService(Components.interfaces.nsIObserverService);
  service.addObserver(observer, "open-new-tab-request", false);
}

function Startup()
{
  // init globals
  gNavigatorBundle = document.getElementById("bundle_navigator");
  gBrandBundle = document.getElementById("bundle_brand");
  gNavigatorRegionBundle = document.getElementById("bundle_navigator_region");
  gBrandRegionBundle = document.getElementById("bundle_brand_region");
  registerZoomManager();
  gBrowser = document.getElementById("content");
  gURLBar = document.getElementById("urlbar");
  
  SetPageProxyState("invalid", null);

  var webNavigation;
  try {
    // Create the browser instance component.
    appCore = Components.classes["@mozilla.org/appshell/component/browser/instance;1"]
                        .createInstance(Components.interfaces.nsIBrowserInstance);
    if (!appCore)
      throw "couldn't create a browser instance";

    // Get the preferences service
    var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                                .getService(Components.interfaces.nsIPrefService);
    pref = prefService.getBranch(null);

    webNavigation = getWebNavigation();
    if (!webNavigation)
      throw "no XBL binding for browser";
  } catch (e) {
    alert("Error launching browser window:" + e);
    window.close(); // Give up.
    return;
  }

  // Do all UI building here:

  // set home button tooltip text
  var homePage = getHomePage();
  if (homePage)
    document.getElementById("home-button").setAttribute("tooltiptext", homePage);

  // initialize observers and listeners
  window.XULBrowserWindow = new nsBrowserStatusHandler();

  addPrefListener(gButtonPrefListener); 
  addPrefListener(gTabStripPrefListener);

  window.browserContentListener =
    new nsBrowserContentListener(window, getBrowser());
  
  // Initialize browser instance..
  appCore.setWebShellWindow(window);

  // Add a capturing event listener to the content area
  // (rjc note: not the entire window, otherwise we'll get sidebar pane loads too!)
  //  so we'll be notified when onloads complete.
  var contentArea = document.getElementById("appcontent");
  contentArea.addEventListener("load", loadEventHandlers, true);
  contentArea.addEventListener("focus", contentAreaFrameFocus, true);

  var turboMode = false;
  // set default character set if provided
  if ("arguments" in window && window.arguments.length > 1 && window.arguments[1]) {
    if (window.arguments[1].indexOf("charset=") != -1) {
      var arrayArgComponents = window.arguments[1].split("=");
      if (arrayArgComponents) {
        //we should "inherit" the charset menu setting in a new window
        getMarkupDocumentViewer().defaultCharacterSet = arrayArgComponents[1];
      }
    } else if (window.arguments[1].indexOf("turbo=yes") != -1) {
      turboMode = true;
    }
  }

  //initConsoleListener();

  // XXXjag work-around for bug 113076
  // there's another bug where we throw an exception when getting
  // sessionHistory if it is null, which I'm exploiting here to
  // detect the situation described in bug 113076.
  // The same problem caused bug 139522, also worked around below.
  try {
    getBrowser().sessionHistory;
  } catch (e) {
    // sessionHistory wasn't set from the browser's constructor
    // so we'll just have to set it here.
 
    // Wire up session and global history before any possible
    // progress notifications for back/forward button updating
    webNavigation.sessionHistory = Components.classes["@mozilla.org/browser/shistory;1"]
                                             .createInstance(Components.interfaces.nsISHistory);

    // wire up global history.  the same applies here.
    var globalHistory = Components.classes["@mozilla.org/browser/global-history;1"]
                                  .getService(Components.interfaces.nsIGlobalHistory);
    getBrowser().docShell.QueryInterface(Components.interfaces.nsIDocShellHistory).globalHistory = globalHistory;

    const selectedBrowser = getBrowser().selectedBrowser;
    if (selectedBrowser.securityUI)
      selectedBrowser.securityUI.init(selectedBrowser.contentWindow);
  }

  // hook up UI through progress listener
  getBrowser().addProgressListener(window.XULBrowserWindow, Components.interfaces.nsIWebProgress.NOTIFY_ALL);

  // load appropriate initial page from commandline
  var isPageCycling = false;

  // page cycling for tinderbox tests
  if (!appCore.cmdLineURLUsed)
    isPageCycling = appCore.startPageCycler();

  // only load url passed in when we're not page cycling
  if (!isPageCycling) {
    var uriToLoad;

    // Check for window.arguments[0]. If present, use that for uriToLoad.
    if ("arguments" in window && window.arguments.length >= 1 && window.arguments[0])
      uriToLoad = window.arguments[0];
    
    if (uriToLoad && uriToLoad != "about:blank") {
      gURLBar.value = uriToLoad;
      if ("arguments" in window && window.arguments.length >= 3) {
        loadURI(uriToLoad, window.arguments[2]);
      } else {
        loadURI(uriToLoad);
      }
    }

    // Close the window now, if it's for turbo mode startup.
    if ( turboMode ) {
        // Set "command line used" flag.  If we don't do this, then when a cmd line url
        // for a "real* invocation comes in, we will override it with the "cmd line url"
        // from the turbo-mode process (i.e., the home page).
        appCore.cmdLineURLUsed = true;
        // For some reason, window.close() directly doesn't work, so do it in the future.
        window.setTimeout( "window.close()", 100 );
        return;
    }

    // Focus the content area unless we're loading a blank page
    var navBar = document.getElementById("nav-bar");
    if (uriToLoad == "about:blank" && !navBar.hidden && window.locationbar.visible)
      setTimeout(WindowFocusTimerCallback, 0, gURLBar);
    else
      setTimeout(WindowFocusTimerCallback, 0, _content);

    // Perform default browser checking (after window opens).
    setTimeout( checkForDefaultBrowser, 0 );

    // hook up remote support
    if (XREMOTESERVICE_CONTRACTID in Components.classes) {
      var remoteService;
      remoteService = Components.classes[XREMOTESERVICE_CONTRACTID]
                                .getService(Components.interfaces.nsIXRemoteService);
      remoteService.addBrowserInstance(window);

      RegisterTabOpenObserver();
    }
  }
  
  // called when we go into full screen, even if it is 
  // initiated by a web page script
  addEventListener("fullscreen", onFullScreen, false);

  // does clicking on the urlbar select its contents?
  gClickSelectsAll = pref.getBoolPref("browser.urlbar.clickSelectsAll");

  // now load bookmarks after a delay
  setTimeout(LoadBookmarksCallback, 0);
}

function LoadBookmarksCallback()
{
  try {
    if (!gBookmarksService)
      gBookmarksService = Components.classes["@mozilla.org/browser/bookmarks-service;1"]
                                    .getService(Components.interfaces.nsIBookmarksService);
    gBookmarksService.ReadBookmarks();
    // tickle personal toolbar to load personal toolbar items
    var personalToolbar = document.getElementById("NC:PersonalToolbarFolder");
    personalToolbar.builder.rebuild();
  } catch (e) {
  }
}

function WindowFocusTimerCallback(element)
{
  // This fuction is a redo of the fix for jag bug 91884
  var ww = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                     .getService(Components.interfaces.nsIWindowWatcher);
  if (window == ww.activeWindow) {
    element.focus();
  } else {
    // set the element in command dispatcher so focus will restore properly
    // when the window does become active
    if (element instanceof Components.interfaces.nsIDOMElement)
      document.commandDispatcher.focusedElement = element;
    else if (element instanceof Components.interfaces.nsIDOMWindow)
      document.commandDispatcher.focusedWindow = element;
  }
}

function BrowserFlushBookmarksAndHistory()
{
  // Flush bookmarks and history (used when window closes or is cached).
  try {
    // If bookmarks are dirty, flush 'em to disk
    var bmks = Components.classes["@mozilla.org/browser/bookmarks-service;1"]
                         .getService(Components.interfaces.nsIRDFRemoteDataSource);
    bmks.Flush();

    // give history a chance at flushing to disk also
    var history = Components.classes["@mozilla.org/browser/global-history;1"]
                            .getService(Components.interfaces.nsIRDFRemoteDataSource);
    history.Flush();
  } catch(ex) {
  }
}

function Shutdown()
{
  // remove remote support
  if (XREMOTESERVICE_CONTRACTID in Components.classes) {
    var remoteService;
    remoteService = Components.classes[XREMOTESERVICE_CONTRACTID]
                              .getService(Components.interfaces.nsIXRemoteService);
    remoteService.removeBrowserInstance(window);
  }

  try {
    getBrowser().removeProgressListener(window.XULBrowserWindow);
  } catch (ex) {
  }

  window.XULBrowserWindow.destroy();
  window.XULBrowserWindow = null;

  BrowserFlushBookmarksAndHistory();

  // unregister us as a pref listener
  removePrefListener(gButtonPrefListener);
  removePrefListener(gTabStripPrefListener);

  window.browserContentListener.close();
  // Close the app core.
  if (appCore)
    appCore.close();
}

function Translate()
{
  var service = pref.getCharPref("browser.translation.service");
  var serviceDomain = pref.getCharPref("browser.translation.serviceDomain");
  var targetURI = getWebNavigation().currentURI.spec;

  // if we're already viewing a translated page, then just reload
  if (targetURI.indexOf(serviceDomain) >= 0)
    BrowserReload();
  else {
    loadURI(service + escape(targetURI));
  }
}

function gotoHistoryIndex(aEvent)
{
  var index = aEvent.target.getAttribute("index");
  if (!index)
    return false;
  try {
    getWebNavigation().gotoIndex(index);
  }
  catch(ex) {
    return false;
  }
  return true;

}

function BrowserBack()
{
  try {
    getWebNavigation().goBack();
  }
  catch(ex) {
  }
}

function BrowserForward()
{
  try {
    getWebNavigation().goForward();
  }
  catch(ex) {
  }
}

function BrowserBackMenu(event)
{
  return FillHistoryMenu(event.target, "back");
}

function BrowserForwardMenu(event)
{
  return FillHistoryMenu(event.target, "forward");
}

function BrowserStop()
{
  try {
    const stopFlags = nsIWebNavigation.STOP_ALL;
    getWebNavigation().stop(stopFlags);
  }
  catch(ex) {
  }
}

function BrowserReload()
{
  const reloadFlags = nsIWebNavigation.LOAD_FLAGS_NONE;
  return BrowserReloadWithFlags(reloadFlags);
}

function BrowserReloadSkipCache()
{
  // Bypass proxy and cache.
  const reloadFlags = nsIWebNavigation.LOAD_FLAGS_BYPASS_PROXY | nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE;
  return BrowserReloadWithFlags(reloadFlags);
}

function BrowserHome()
{
  var homePage = getHomePage();
  loadURI(homePage);
}

function OpenBookmarkGroup(element, datasource)
{
  if (!datasource)
    return;
    
  var id = element.getAttribute("id");
  var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"]
                          .getService(Components.interfaces.nsIRDFService);
  var resource = rdf.GetResource(id, true);
  OpenBookmarkGroupFromResource(resource, datasource, rdf);
}

function OpenBookmarkGroupFromResource(resource, datasource, rdf) {
  var urlResource = rdf.GetResource("http://home.netscape.com/NC-rdf#URL");
  var rdfContainer = Components.classes["@mozilla.org/rdf/container;1"].getService(Components.interfaces.nsIRDFContainer);
  rdfContainer.Init(datasource, resource);
  var containerChildren = rdfContainer.GetElements();
  var tabPanels = gBrowser.mPanelContainer.childNodes;
  var tabCount = tabPanels.length;
  var index = 0;
  for (; containerChildren.hasMoreElements(); ++index) {
    var res = containerChildren.getNext().QueryInterface(Components.interfaces.nsIRDFResource);
    var target = datasource.GetTarget(res, urlResource, true);
    if (target) {
      var uri = target.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
      gBrowser.addTab(uri);
    }
  }  
  
  if (index == 0)
    return; // If the bookmark group was completely invalid, just bail.
     
  // Select the first tab in the group.
  var tabs = gBrowser.mTabContainer.childNodes;
  gBrowser.selectedTab = tabs[tabCount];
}

function OpenBookmarkURL(node, datasources)
{
  if (node.getAttribute("group") == "true")
    OpenBookmarkGroup(node, datasources);
    
  if (node.getAttribute("container") == "true")
    return;

  var url = node.getAttribute("id");
  if (!url) // if empty url (most likely a normal menu item like "Manage Bookmarks",
    return; // don't bother loading it
  try {
    // add support for IE favorites under Win32, and NetPositive URLs under BeOS
    if (datasources) {
      var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"]
                          .getService(Components.interfaces.nsIRDFService);
      var src = rdf.GetResource(url, true);
      var prop = rdf.GetResource("http://home.netscape.com/NC-rdf#URL", true);
      var target = datasources.GetTarget(src, prop, true);
      if (target) {
        target = target.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
        if (target)
          url = target;
      }
    }
  } catch (ex) {
    return;
  }

  // Ignore "NC:" urls.
  if (url.substring(0, 3) == "NC:")
    return;

  // Check if we have a browser window
  if (_content) {
    loadURI(url);
    _content.focus();
  }
  else
    openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no", url);
}

function addGroupmarkAs()
{
  BookmarksUtils.addBookmarkForTabBrowser(gBrowser, true);
}

function addBookmarkAs(aBrowser)
{
  const browsers = aBrowser.browsers;
  if (browsers.length > 1)
    BookmarksUtils.addBookmarkForTabBrowser(aBrowser);
  else
    BookmarksUtils.addBookmarkForBrowser(aBrowser.webNavigation, true);
}

function updateGroupmarkMenuitem(id)
{
  const disabled = gBrowser.browsers.length == 1;
  document.getElementById(id).setAttribute("disabled", disabled);
}

function readRDFString(aDS,aRes,aProp)
{
  var n = aDS.GetTarget(aRes, aProp, true);
  return n ? n.QueryInterface(Components.interfaces.nsIRDFLiteral).Value : "";
}


function ensureDefaultEnginePrefs(aRDF,aDS) 
{
  var mPrefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
  var defaultName = mPrefs.getComplexValue("browser.search.defaultenginename", Components.interfaces.nsIPrefLocalizedString).data;
  var kNC_Root = aRDF.GetResource("NC:SearchEngineRoot");
  var kNC_child = aRDF.GetResource("http://home.netscape.com/NC-rdf#child");
  var kNC_Name = aRDF.GetResource("http://home.netscape.com/NC-rdf#Name");
          
  var arcs = aDS.GetTargets(kNC_Root, kNC_child, true);
  while (arcs.hasMoreElements()) {
    var engineRes = arcs.getNext().QueryInterface(Components.interfaces.nsIRDFResource);       
    var name = readRDFString(aDS, engineRes, kNC_Name);
    if (name == defaultName)
      mPrefs.setCharPref("browser.search.defaultengine", engineRes.Value);
  }
}

function ensureSearchPref()
{
  var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);
  var ds = rdf.GetDataSource("rdf:internetsearch");
  var mPrefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
  var kNC_Name = rdf.GetResource("http://home.netscape.com/NC-rdf#Name");
  var defaultEngine;
  try {
    defaultEngine = mPrefs.getCharPref("browser.search.defaultengine");
  } catch(ex) {
    ensureDefaultEnginePrefs(rdf, ds);
    defaultEngine = mPrefs.getCharPref("browser.search.defaultengine");
  }
}

function getSearchUrl(attr)
{
  var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService); 
  var ds = rdf.GetDataSource("rdf:internetsearch"); 
  var kNC_Root = rdf.GetResource("NC:SearchEngineRoot");
  var mPrefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
  var defaultEngine = mPrefs.getCharPref("browser.search.defaultengine");
  var engineRes = rdf.GetResource(defaultEngine);
  var prop = "http://home.netscape.com/NC-rdf#" + attr;
  var kNC_attr = rdf.GetResource(prop);
  var searchURL = readRDFString(ds, engineRes, kNC_attr);
  return searchURL;
}

function QualifySearchTerm()
{
  // If the text in the URL bar is the same as the currently loaded
  // page's URL then treat this as an empty search term.  This way
  // the user is taken to the search page where s/he can enter a term.
  if (window.XULBrowserWindow.userTyped.value)
    return document.getElementById("urlbar").value;
  return "";
}

function OpenSearch(tabName, forceDialogFlag, searchStr, newWindowFlag)
{
  //This function needs to be split up someday.

  var autoOpenSearchPanel = false;
  var defaultSearchURL = null;
  var fallbackDefaultSearchURL = gNavigatorRegionBundle.getString("fallbackDefaultSearchURL");
  ensureSearchPref()
  //Check to see if search string contains "://" or "ftp." or white space.
  //If it does treat as url and match for pattern
  
  var urlmatch= /(:\/\/|^ftp\.)[^ \S]+$/ 
  var forceAsURL = urlmatch.test(searchStr);

  try {
    autoOpenSearchPanel = pref.getBoolPref("browser.search.opensidebarsearchpanel");
    defaultSearchURL = pref.getComplexValue("browser.search.defaulturl",
                                            Components.interfaces.nsIPrefLocalizedString).data;
  } catch (ex) {
  }

  // Fallback to a default url (one that we can get sidebar search results for)
  if (!defaultSearchURL)
    defaultSearchURL = fallbackDefaultSearchURL;

  if (!searchStr) {
    BrowserSearchInternet();
  } else {

    //Check to see if location bar field is a url
    //If it is a url go to URL.  A Url is "://" or "." as commented above
    //Otherwise search on entry
    if (forceAsURL) {
       BrowserLoadURL()
    } else {
      var searchMode = 0;
      try {
        searchMode = pref.getIntPref("browser.search.powermode");
      } catch(ex) {
      }

      if (forceDialogFlag || searchMode == 1) {
        // Use a single search dialog
        var windowManager = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                                      .getService(Components.interfaces.nsIWindowMediator);

        var searchWindow = windowManager.getMostRecentWindow("search:window");
        if (!searchWindow) {
          openDialog("chrome://communicator/content/search/search.xul", "SearchWindow", "dialog=no,close,chrome,resizable", tabName, searchStr);
        } else {
          // Already had one, focus it and load the page
          searchWindow.focus();

          if ("loadPage" in searchWindow)
            searchWindow.loadPage(tabName, searchStr);
        }
      } else {
        if (searchStr) {
          var escapedSearchStr = escape(searchStr);
          defaultSearchURL += escapedSearchStr;
          var searchDS = Components.classes["@mozilla.org/rdf/datasource;1?name=internetsearch"]
                                   .getService(Components.interfaces.nsIInternetSearchService);

          searchDS.RememberLastSearchText(escapedSearchStr);
          try {
            var searchEngineURI = pref.getCharPref("browser.search.defaultengine");
            if (searchEngineURI) {          
              var searchURL = getSearchUrl("actionButton");
              if (searchURL) {
                defaultSearchURL = searchURL + escapedSearchStr; 
              } else {
                searchURL = searchDS.GetInternetSearchURL(searchEngineURI, escapedSearchStr, 0, 0, {value:0});
                if (searchURL)
                  defaultSearchURL = searchURL;
              }
            }
          } catch (ex) {
          }

          if (!newWindowFlag)
            loadURI(defaultSearchURL);
          else
            window.open(defaultSearchURL, "_blank");
        }
      }
    }
  }

  // should we try and open up the sidebar to show the "Search Results" panel?
  if (autoOpenSearchPanel)
    RevealSearchPanel();
}

function RevealSearchPanel()
{
  var searchPanel = document.getElementById("urn:sidebar:panel:search");
  if (searchPanel)
    SidebarSelectPanel(searchPanel, true, true); // lives in sidebarOverlay.js
}

function isSearchPanelOpen()
{
  return ( !sidebar_is_hidden()    && 
           !sidebar_is_collapsed() && 
           SidebarGetLastSelectedPanel() == "urn:sidebar:panel:search"
         );
}

function BrowserSearchInternet()
{
  try {
    var searchEngineURI = pref.getCharPref("browser.search.defaultengine");
    if (searchEngineURI) {          
      var searchRoot = getSearchUrl("searchForm");
      if (searchRoot) {
        loadURI(searchRoot);
        return;
      } else {
        // Get a search URL and guess that the front page of the site has a search form.
        var searchDS = Components.classes["@mozilla.org/rdf/datasource;1?name=internetsearch"]
                                 .getService(Components.interfaces.nsIInternetSearchService);
        searchURL = searchDS.GetInternetSearchURL(searchEngineURI, "ABC", 0, 0, {value:0});
        if (searchURL) {
          searchRoot = searchURL.match(/[a-z]+:\/\/[a-z.-]+/);
          if (searchRoot) {
            loadURI(searchRoot + "/");
            return;
          }
        }
      }
    }
  } catch (ex) {
  }

  // Fallback if the stuff above fails: use the hard-coded search engine
  loadURI(gNavigatorRegionBundle.getString("otherSearchURL"));
}


//Note: BrowserNewEditorWindow() was moved to globalOverlay.xul and renamed to NewEditorWindow()

function BrowserOpenWindow()
{
  //opens a window where users can select a web location to open
  openDialog("chrome://communicator/content/openLocation.xul", "_blank", "chrome,modal,titlebar", window);
}

function BrowserOpenTab()
{
  if (!gInPrintPreviewMode) {
    gBrowser.selectedTab = gBrowser.addTab('about:blank');
    setTimeout("gURLBar.focus();", 0); 
  }
}

/* Called from the openLocation dialog. This allows that dialog to instruct
   its opener to open a new window and then step completely out of the way.
   Anything less byzantine is causing horrible crashes, rather believably,
   though oddly only on Linux. */
function delayedOpenWindow(chrome,flags,url)
{
  setTimeout("openDialog('"+chrome+"','_blank','"+flags+"','"+url+"')", 10);
}

/* Required because the tab needs time to set up its content viewers and get the load of
   the URI kicked off before becoming the active content area. */
function delayedOpenTab(url)
{
  setTimeout(function(aTabElt) { getBrowser().selectedTab = aTabElt; }, 0, getBrowser().addTab(url));
}

function BrowserOpenFileWindow()
{
  // Get filepicker component.
  try {
    const nsIFilePicker = Components.interfaces.nsIFilePicker;
    var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
    fp.init(window, gNavigatorBundle.getString("openFile"), nsIFilePicker.modeOpen);
    fp.appendFilters(nsIFilePicker.filterAll | nsIFilePicker.filterText | nsIFilePicker.filterImages |
                     nsIFilePicker.filterXML | nsIFilePicker.filterHTML);

    if (fp.show() == nsIFilePicker.returnOK)
      openTopWin(fp.fileURL.spec);
  } catch (ex) {
  }
}

// Set up a lame hack to avoid opening two bookmarks.
// Could otherwise happen with two Ctrl-B's in a row.
var gDisableBookmarks = false;
function enableBookmarks()
{
  gDisableBookmarks = false;
}

function BrowserEditBookmarks()
{
  // Use a single sidebar bookmarks dialog
  var windowManager = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                                .getService(Components.interfaces.nsIWindowMediator);

  var bookmarksWindow = windowManager.getMostRecentWindow("bookmarks:manager");

  if (bookmarksWindow) {
    bookmarksWindow.focus();
  } else {
    // while disabled, don't open new bookmarks window
    if (!gDisableBookmarks) {
      gDisableBookmarks = true;

      open("chrome://communicator/content/bookmarks/bookmarks.xul", "_blank",
        "chrome,extrachrome,menubar,resizable,scrollbars,status,toolbar");
      setTimeout(enableBookmarks, 2000);
    }
  }
}

function BrowserCloseTabOrWindow()
{
  var browser = getBrowser();
  if (browser && browser.localName == 'tabbrowser' && browser.mTabContainer.childNodes.length > 1) {
    // Just close up a tab.
    browser.removeCurrentTab();
    return;
  }

  BrowserCloseWindow();
}

function BrowserCloseWindow() 
{
  // This code replicates stuff in Shutdown().  It is here because
  // window.screenX and window.screenY have real values.  We need
  // to fix this eventually but by replicating the code here, we
  // provide a means of saving position (it just requires that the
  // user close the window via File->Close (vs. close box).
  
  // Get the current window position/size.
  var x = window.screenX;
  var y = window.screenY;
  var h = window.outerHeight;
  var w = window.outerWidth;

  // Store these into the window attributes (for persistence).
  var win = document.getElementById( "main-window" );
  win.setAttribute( "x", x );
  win.setAttribute( "y", y );
  win.setAttribute( "height", h );
  win.setAttribute( "width", w );

  window.close();
}

function loadURI(uri, referrer)
{
  try {
    getWebNavigation().loadURI(uri, nsIWebNavigation.LOAD_FLAGS_NONE, referrer, null, null);
  } catch (e) {
  }
}

function BrowserLoadURL(aTriggeringEvent)
{
  var url = gURLBar.value;
  if (url.match(/^view-source:/)) {
    BrowserViewSourceOfURL(url.replace(/^view-source:/, ""), null, null);
  } else {
    if (pref && pref.getBoolPref("browser.tabs.opentabfor.urlbar") &&
        getBrowser().localName == "tabbrowser" &&
        aTriggeringEvent && 'ctrlKey' in aTriggeringEvent &&
        aTriggeringEvent.ctrlKey) {
      var t = getBrowser().addTab(getShortcutOrURI(url)); // open link in new tab
      getBrowser().selectedTab = t;
    }
    else  
      loadURI(getShortcutOrURI(url));
    _content.focus();
  }
}

function getShortcutOrURI(url)
{
  // rjc: added support for URL shortcuts (3/30/1999)
  try {
    if (!gBookmarksService)
      gBookmarksService = Components.classes["@mozilla.org/browser/bookmarks-service;1"]
                                    .getService(Components.interfaces.nsIBookmarksService);

    var shortcutURL = gBookmarksService.resolveKeyword(url);
    if (!shortcutURL) {
      // rjc: add support for string substitution with shortcuts (4/4/2000)
      //      (see bug # 29871 for details)
      var aOffset = url.indexOf(" ");
      if (aOffset > 0) {
        var cmd = url.substr(0, aOffset);
        var text = url.substr(aOffset+1);
        shortcutURL = gBookmarksService.resolveKeyword(cmd);
        if (shortcutURL && text) {
          aOffset = shortcutURL.indexOf("%s");
          if (aOffset >= 0)
            shortcutURL = shortcutURL.substr(0, aOffset) + text + shortcutURL.substr(aOffset+2);
          else
            shortcutURL = null;
        }
      }
    }

    if (shortcutURL)
      url = shortcutURL;

  } catch (ex) {
  }
  return url;
}

function readFromClipboard()
{
  var url;

  try {
    // Get clipboard.
    var clipboard = Components.classes["@mozilla.org/widget/clipboard;1"]
                              .getService(Components.interfaces.nsIClipboard);

    // Create tranferable that will transfer the text.
    var trans = Components.classes["@mozilla.org/widget/transferable;1"]
                          .createInstance(Components.interfaces.nsITransferable);

    trans.addDataFlavor("text/unicode");
    clipboard.getData(trans, clipboard.kSelectionClipboard);

    var data = {};
    var dataLen = {};
    trans.getTransferData("text/unicode", data, dataLen);

    if (data) {
      data = data.value.QueryInterface(Components.interfaces.nsISupportsWString);
      url = data.data.substring(0, dataLen.value / 2);
    }
  } catch (ex) {
  }

  return url;
}

function OpenMessenger()
{
  open("chrome://messenger/content/messenger.xul", "_blank",
    "chrome,extrachrome,menubar,resizable,status,toolbar");
}

function OpenAddressbook()
{
  open("chrome://messenger/content/addressbook/addressbook.xul", "_blank",
    "chrome,extrachrome,menubar,resizable,status,toolbar");
}

function BrowserViewSourceOfDocument(aDocument)
{
  var docCharset;
  var pageCookie;
  var webNav;

  // Get the document charset
  docCharset = "charset=" + aDocument.characterSet;

  // Get the nsIWebNavigation associated with the document
  try {
      var win;
      var ifRequestor;

      // Get the DOMWindow for the requested document.  If the DOMWindow
      // cannot be found, then just use the _content window...
      //
      // XXX:  This is a bit of a hack...
      win = aDocument.defaultView;
      if (win == window) {
        win = _content;
      }
      ifRequestor = win.QueryInterface(Components.interfaces.nsIInterfaceRequestor);

      webNav = ifRequestor.getInterface(Components.interfaces.nsIWebNavigation);
  } catch(err) {
      // If nsIWebNavigation cannot be found, just get the one for the whole
      // window...
      webNav = getWebNavigation();
  }
  //
  // Get the 'PageDescriptor' for the current document. This allows the
  // view-source to access the cached copy of the content rather than
  // refetching it from the network...
  //
  try{
    var PageLoader = webNav.QueryInterface(Components.interfaces.nsIWebPageDescriptor);

    pageCookie = PageLoader.currentDescriptor;
  } catch(err) {
    // If no page descriptor is available, just use the view-source URL...
  }

  BrowserViewSourceOfURL(webNav.currentURI.spec, docCharset, pageCookie);
}

function BrowserViewSourceOfURL(url, charset, pageCookie)
{
  // try to open a view-source window while inheriting the charset (if any)
  openDialog("chrome://navigator/content/viewSource.xul",
             "_blank",
             "scrollbars,resizable,chrome,dialog=no",
             url, charset, pageCookie);
}

// doc=null for regular page info, doc=owner document for frame info.
function BrowserPageInfo(doc)
{
  window.openDialog("chrome://navigator/content/pageInfo.xul",
                    "_blank",
                    "chrome,dialog=no",
                    doc);
}

function hiddenWindowStartup()
{
  // focus the hidden window
  window.focus();

  // Disable menus which are not appropriate
  var disabledItems = ['cmd_close', 'Browser:SendPage', 'Browser:EditPage', 'Browser:PrintSetup', /*'Browser:PrintPreview',*/
                       'Browser:Print', 'canGoBack', 'canGoForward', 'Browser:Home', 'Browser:AddBookmark', 'cmd_undo',
                       'cmd_redo', 'cmd_cut', 'cmd_copy','cmd_paste', 'cmd_delete', 'cmd_selectAll', 'menu_textZoom'];
  for (var id in disabledItems) {
    var broadcaster = document.getElementById(disabledItems[id]);
    if (broadcaster)
      broadcaster.setAttribute("disabled", "true");
  }
}

// Initialize the LeakDetector class.
function LeakDetector(verbose)
{
  this.verbose = verbose;
}

const NS_LEAKDETECTOR_CONTRACTID = "@mozilla.org/xpcom/leakdetector;1";

if (NS_LEAKDETECTOR_CONTRACTID in Components.classes) {
  try {
    LeakDetector.prototype = Components.classes[NS_LEAKDETECTOR_CONTRACTID]
                                       .createInstance(Components.interfaces.nsILeakDetector);
  } catch (err) {
    LeakDetector.prototype = Object.prototype;
  }
} else {
  LeakDetector.prototype = Object.prototype;
}

var leakDetector = new LeakDetector(false);

// Dumps current set of memory leaks.
function dumpMemoryLeaks()
{
  leakDetector.dumpLeaks();
}

// Traces all objects reachable from the chrome document.
function traceChrome()
{
  leakDetector.traceObject(document, leakDetector.verbose);
}

// Traces all objects reachable from the content document.
function traceDocument()
{
  // keep the chrome document out of the dump.
  leakDetector.markObject(document, true);
  leakDetector.traceObject(_content, leakDetector.verbose);
  leakDetector.markObject(document, false);
}

// Controls whether or not we do verbose tracing.
function traceVerbose(verbose)
{
  leakDetector.verbose = (verbose == "true");
}

var consoleListener = {
  observe: function (aMsgObject)
  {
    const nsIScriptError = Components.interfaces.nsIScriptError;
    var scriptError = aMsgObject.QueryInterface(nsIScriptError);
    var isWarning = scriptError.flags & nsIScriptError.warningFlag != 0;
    if (!isWarning) {
      var statusbarDisplay = document.getElementById("statusbar-display");
      statusbarDisplay.setAttribute("error", "true");
      statusbarDisplay.addEventListener("click", loadErrorConsole, true);
      statusbarDisplay.label = gNavigatorBundle.getString("jserror");
      this.isShowingError = true;
    }
  },

  // whether or not an error alert is being displayed
  isShowingError: false
};

function initConsoleListener()
{
  /**
   * XXX - console launch hookup requires some work that I'm not sure
   * how to do.
   *
   *       1) ideally, the notification would disappear when the
   *       document that had the error was flushed. how do I know when
   *       this happens? All the nsIScriptError object I get tells me
   *       is the URL. Where is it located in the content area?
   *       2) the notification service should not display chrome
   *       script errors.  web developers and users are not interested
   *       in the failings of our shitty, exception unsafe js. One
   *       could argue that this should also extend to the console by
   *       default (although toggle-able via setting for chrome
   *       authors) At any rate, no status indication should be given
   *       for chrome script errors.
   *
   *       As a result I am commenting out this for the moment.
   *

  var consoleService = Components.classes["@mozilla.org/consoleservice;1"]
                                 .getService(Components.interfaces.nsIConsoleService);

  if (consoleService)
    consoleService.registerListener(consoleListener);
  */
}

function loadErrorConsole(aEvent)
{
  if (aEvent.detail == 2)
    toJavaScriptConsole();
}

function clearErrorNotification()
{
  var statusbarDisplay = document.getElementById("statusbar-display");
  statusbarDisplay.removeAttribute("error");
  statusbarDisplay.removeEventListener("click", loadErrorConsole, true);
  consoleListener.isShowingError = false;
}

const NS_URLWIDGET_CONTRACTID = "@mozilla.org/urlwidget;1";
var urlWidgetService = null;
if (NS_URLWIDGET_CONTRACTID in Components.classes) {
  urlWidgetService = Components.classes[NS_URLWIDGET_CONTRACTID]
                               .getService(Components.interfaces.nsIUrlWidget);
}

//Posts the currently displayed url to a native widget so third-party apps can observe it.
function postURLToNativeWidget()
{
  if (urlWidgetService) {
    var url = getWebNavigation().currentURI.spec;
    try {
      urlWidgetService.SetURLToHiddenControl(url, window);
    } catch(ex) {
    }
  }
}

function checkForDirectoryListing()
{
  if ( "HTTPIndex" in _content &&
       _content.HTTPIndex instanceof Components.interfaces.nsIHTTPIndex ) {
    _content.defaultCharacterset = getMarkupDocumentViewer().defaultCharacterSet;
  }
}

/**
 * Use Stylesheet functions.
 *     Written by Tim Hill (bug 6782)
 *     Frameset handling by Neil Rashbrook <neil@parkwaycc.co.uk>
 **/
function getStyleSheetArray(frame)
{
  var styleSheets = frame.document.styleSheets;
  var styleSheetsArray = new Array(styleSheets.length);
  for (var i = 0; i < styleSheets.length; i++) {
    styleSheetsArray[i] = styleSheets[i];
  }
  return styleSheetsArray;
}

function getAllStyleSheets(frameset)
{
  var styleSheetsArray = getStyleSheetArray(frameset);
  for (var i = 0; i < frameset.frames.length; i++) {
    var frameSheets = getAllStyleSheets(frameset.frames[i]);
    styleSheetsArray = styleSheetsArray.concat(frameSheets);
  }
  return styleSheetsArray;
}

function stylesheetFillPopup(menuPopup)
{
  var itemNoOptStyles = menuPopup.firstChild;
  while (itemNoOptStyles.nextSibling)
    menuPopup.removeChild(itemNoOptStyles.nextSibling);

  var noOptionalStyles = true;
  var styleSheets = getAllStyleSheets(window._content);
  var currentStyleSheets = [];

  for (var i = 0; i < styleSheets.length; ++i) {
    var currentStyleSheet = styleSheets[i];

    if (currentStyleSheet.title) {
      if (!currentStyleSheet.disabled)
        noOptionalStyles = false;

      var lastWithSameTitle = null;
      if (currentStyleSheet.title in currentStyleSheets)
        lastWithSameTitle = currentStyleSheets[currentStyleSheet.title];

      if (!lastWithSameTitle) {
        var menuItem = document.createElement("menuitem");
        menuItem.setAttribute("type", "radio");
        menuItem.setAttribute("label", currentStyleSheet.title);
        menuItem.setAttribute("data", currentStyleSheet.title);
        menuItem.setAttribute("checked", !currentStyleSheet.disabled);
        menuPopup.appendChild(menuItem);
        currentStyleSheets[currentStyleSheet.title] = menuItem;
      } else {
        if (currentStyleSheet.disabled)
          lastWithSameTitle.removeAttribute("checked");
      }
    }
  }
  itemNoOptStyles.setAttribute("checked", noOptionalStyles);
}

function stylesheetInFrame(frame, title) {
  var docStyleSheets = frame.document.styleSheets;

  for (var i = 0; i < docStyleSheets.length; ++i) {
    if (docStyleSheets[i].title == title)
      return true;
  }
  return false;
}

function stylesheetSwitchFrame(frame, title) {
  var docStyleSheets = frame.document.styleSheets;

  for (var i = 0; i < docStyleSheets.length; ++i) {
    var docStyleSheet = docStyleSheets[i];

    if (docStyleSheet.title)
      docStyleSheet.disabled = (docStyleSheet.title != title);
    else if (docStyleSheet.disabled)
      docStyleSheet.disabled = false;
  }
}

function stylesheetSwitchAll(frameset, title) {
  if (!title || stylesheetInFrame(frameset, title)) {
    stylesheetSwitchFrame(frameset, title);
  }
  for (var i = 0; i < frameset.frames.length; i++) {
    stylesheetSwitchAll(frameset.frames[i], title);
  }
}

function applyTheme(themeName)
{
  var id = themeName.getAttribute('id'); 
  var name=id.substring('urn:mozilla.skin.'.length, id.length);
  if (!name)
    return;

  var chromeRegistry = Components.classes["@mozilla.org/chrome/chrome-registry;1"]
    .getService(Components.interfaces.nsIXULChromeRegistry);

  var oldTheme = false;
  try {
    oldTheme = !chromeRegistry.checkThemeVersion(name);
  }
  catch(e) {
  }

  var str = Components.classes["@mozilla.org/supports-wstring;1"]
                      .createInstance(Components.interfaces.nsISupportsWString);

  var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);
  if (oldTheme) {
    var title = gNavigatorBundle.getString("oldthemetitle");
    var message = gNavigatorBundle.getString("oldTheme");

    message = message.replace(/%theme_name%/, themeName.getAttribute("displayName"));
    message = message.replace(/%brand%/g, gBrandBundle.getString("brandShortName"));

    if (promptService.confirm(window, title, message)){
      var inUse = chromeRegistry.isSkinSelected(name, true);

      chromeRegistry.uninstallSkin( name, true );
      // XXX - this sucks and should only be temporary.

      str.data = true;
      pref.setComplexValue("general.skins.removelist." + name,
                           Components.interfaces.nsISupportsWString, str);
      
      if (inUse)
        chromeRegistry.refreshSkins();
    }

    return;
  }

 // XXX XXX BAD BAD BAD BAD !! XXX XXX                                         
 // we STILL haven't fixed editor skin switch problems                         
 // hacking around it yet again                                                

 str.data = name;
 pref.setComplexValue("general.skins.selectedSkin", Components.interfaces.nsISupportsWString, str);
 
 // shut down quicklaunch so the next launch will have the new skin
 var appShell = Components.classes['@mozilla.org/appshell/appShellService;1'].getService();
 appShell = appShell.QueryInterface(Components.interfaces.nsIAppShellService);
 try {
   appShell.nativeAppSupport.isServerMode = false;
 }
 catch(ex) {
 }
 
 if (promptService) {                                                          
   var dialogTitle = gNavigatorBundle.getString("switchskinstitle");           
   var brandName = gBrandBundle.getString("brandShortName");                   
   var msg = gNavigatorBundle.getFormattedString("switchskins", [brandName]);  
   promptService.alert(window, dialogTitle, msg);                              
 }                                                                             
}

function getNewThemes()
{
  loadURI(gBrandRegionBundle.getString("getNewThemesURL"));
}

function URLBarFocusHandler(aEvent)
{
  if (gIgnoreFocus)
    gIgnoreFocus = false;
  else if (gClickSelectsAll)
    gURLBar.select();
}

function URLBarMouseDownHandler(aEvent)
{
  if (gURLBar.hasAttribute("focused")) {
    gIgnoreClick = true;
  } else {
    gIgnoreFocus = true;
    gIgnoreClick = false;
    gURLBar.setSelectionRange(0, 0);
  }
}

function URLBarClickHandler(aEvent)
{
  if (!gIgnoreClick && gClickSelectsAll && gURLBar.selectionStart == gURLBar.selectionEnd)
    gURLBar.select();
}

// This function gets the "windows hooks" service and has it check its setting
// This will do nothing on platforms other than Windows.
function checkForDefaultBrowser()
{
  const NS_WINHOOKS_CONTRACTID = "@mozilla.org/winhooks;1";
  var dialogShown = false;
  if (NS_WINHOOKS_CONTRACTID in Components.classes) {
    try {
      dialogShown = Components.classes[NS_WINHOOKS_CONTRACTID]
                      .getService(Components.interfaces.nsIWindowsHooks)
                      .checkSettings(window);
    } catch(e) {
    }

    if (dialogShown)  
    {
      // Force the sidebar to build since the windows 
      // integration dialog may have come up.
      SidebarRebuild();
    }
  }
}

function ShowAndSelectContentsOfURLBar()
{
  var navBar = document.getElementById("nav-bar");
  
  // If it's hidden, show it.
  if (navBar.getAttribute("hidden") == "true")
    goToggleToolbar('nav-bar','cmd_viewnavbar');

  if (gURLBar.value)
    gURLBar.select();
  else
    gURLBar.focus();
}

// If "ESC" is pressed in the url bar, we replace the urlbar's value with the url of the page
// and highlight it, unless it is about:blank, where we reset it to "".
function handleURLBarRevert()
{
  var url = getWebNavigation().currentURI.spec;
  var throbberElement = document.getElementById("navigator-throbber");

  var isScrolling = gURLBar.userAction == "scrolling";
  
  // don't revert to last valid url unless page is NOT loading
  // and user is NOT key-scrolling through autocomplete list
  if (!throbberElement.hasAttribute("busy") && !isScrolling) {
    if (url != "about:blank") { 
      gURLBar.value = url;
      gURLBar.select();
      SetPageProxyState("valid", null); // XXX Build a URI and pass it in here.
    } else { //if about:blank, urlbar becomes ""
      gURLBar.value = "";
    }
  }

  // tell widget to revert to last typed text only if the user
  // was scrolling when they hit escape
  return isScrolling; 
}

function handleURLBarCommand(aUserAction, aTriggeringEvent)
{
  try { 
    addToUrlbarHistory();
  } catch (ex) {
    // Things may go wrong when adding url to session history,
    // but don't let that interfere with the loading of the url.
  }
  
  BrowserLoadURL(aTriggeringEvent); 
}

function UpdatePageProxyState()
{
  if (gURLBar.value != gLastValidURLStr)
    SetPageProxyState("invalid", null);
}

function SetPageProxyState(aState, aURI)
{
  if (!gProxyButton)
    gProxyButton = document.getElementById("page-proxy-button");
  if (!gProxyFavIcon)
    gProxyFavIcon = document.getElementById("page-proxy-favicon");
  if (!gProxyDeck)
    gProxyDeck = document.getElementById("page-proxy-deck");

  gProxyButton.setAttribute("pageproxystate", aState);

  if (aState == "valid") {
    gLastValidURLStr = gURLBar.value;
    gURLBar.addEventListener("input", UpdatePageProxyState, false);
    if (gBrowser.shouldLoadFavIcon(aURI)) {
      var favStr = gBrowser.buildFavIconString(aURI);
      if (favStr != gProxyFavIcon.src) {
        gBrowser.loadFavIcon(aURI, "src", gProxyFavIcon);
        gProxyDeck.selectedIndex = 0;
      }
      else gProxyDeck.selectedIndex = 1;
    }
    else {
      gProxyDeck.selectedIndex = 0;
      gProxyFavIcon.removeAttribute("src");
    }
  } else if (aState == "invalid") {
    gURLBar.removeEventListener("input", UpdatePageProxyState, false);
    gProxyDeck.selectedIndex = 0;
  }
}

function PageProxyDragGesture(aEvent)
{
  if (gProxyButton.getAttribute("pageproxystate") == "valid") {
    nsDragAndDrop.startDrag(aEvent, proxyIconDNDObserver);
    return true;
  }
  return false;
}

function updateComponentBarBroadcaster()
{ 
  var compBarBroadcaster = document.getElementById('cmd_viewcomponentbar');
  var taskBarBroadcaster = document.getElementById('cmd_viewtaskbar');
  var compBar = document.getElementById('component-bar');
  if (taskBarBroadcaster.getAttribute('checked') == 'true') {
    compBarBroadcaster.removeAttribute('disabled');
    if (compBar.getAttribute('hidden') != 'true')
      compBarBroadcaster.setAttribute('checked', 'true');
  }
  else {
    compBarBroadcaster.setAttribute('disabled', 'true');
    compBarBroadcaster.removeAttribute('checked');
  }
}

function updateToolbarStates(toolbarMenuElt)
{
  if (!gHaveUpdatedToolbarState) {
    var mainWindow = document.getElementById("main-window");
    if (mainWindow.hasAttribute("chromehidden")) {
      gHaveUpdatedToolbarState = true;
      var i;
      for (i = 0; i < toolbarMenuElt.childNodes.length; ++i)
        document.getElementById(toolbarMenuElt.childNodes[i].getAttribute("observes")).removeAttribute("checked");
      var toolbars = document.getElementsByTagName("toolbar");
      for (i = 0; i < toolbars.length; ++i) {
        if (toolbars[i].getAttribute("class").indexOf("chromeclass") != -1)
          toolbars[i].setAttribute("hidden", "true");
      }
      var statusbars = document.getElementsByTagName("statusbar");
      for (i = 0; i < statusbars.length; ++i) {
        if (statusbars[i].getAttribute("class").indexOf("chromeclass") != -1)
          statusbars[i].setAttribute("hidden", "true");
      }
      mainWindow.removeAttribute("chromehidden");
    }
  }
  updateComponentBarBroadcaster();

  const tabbarMenuItem = document.getElementById("menuitem_showhide_tabbar");
  // Make show/hide menu item reflect current state
  const visibility = gBrowser.getStripVisibility();
  tabbarMenuItem.setAttribute("checked", visibility);

  // Don't allow the tab bar to be shown/hidden when more than one tab is open
  // or when we have 1 tab and the autoHide pref is set
  const disabled = gBrowser.browsers.length > 1 ||
                   pref.getBoolPref("browser.tabs.autoHide");
  tabbarMenuItem.setAttribute("disabled", disabled);
}

function showHideTabbar()
{
  const visibility = gBrowser.getStripVisibility();
  pref.setBoolPref("browser.tabs.forceHide", visibility);
  gBrowser.setStripVisibilityTo(!visibility);
}

// Fill in tooltips for personal toolbar
function FillInPTTooltip(tipElement)
{

  var title = tipElement.label;
  var url = tipElement.statusText;

  if (!title && !url) {
    // bail out early if there is nothing to show
    return false;
  }

  var tooltipTitle = document.getElementById("ptTitleText");
  var tooltipUrl = document.getElementById("ptUrlText"); 

  if (title && title != url) {
    tooltipTitle.removeAttribute("hidden");
    tooltipTitle.setAttribute("value", title);
  } else  {
    tooltipTitle.setAttribute("hidden", "true");
  }

  if (url) {
    tooltipUrl.removeAttribute("hidden");
    tooltipUrl.setAttribute("value", url);
  } else {
    tooltipUrl.setAttribute("hidden", "true");
  }

  return true; // show tooltip
}

function BrowserFullScreen()
{
  window.fullScreen = !window.fullScreen;
}

function onFullScreen()
{
  FullScreen.toggle();
}

// Set up a lame hack to avoid opening two bookmarks.
// Could otherwise happen with two Ctrl-B's in a row.
var gDisableHistory = false;
function enableHistory() {
  gDisableHistory = false;
}

function toHistory()
{
  // Use a single sidebar history dialog

  var cwindowManager = Components.classes['@mozilla.org/appshell/window-mediator;1'].getService();
  var iwindowManager = Components.interfaces.nsIWindowMediator;
  var windowManager  = cwindowManager.QueryInterface(iwindowManager);

  var historyWindow = windowManager.getMostRecentWindow('history:manager');

  if (historyWindow) {
    //debug("Reuse existing history window");
    historyWindow.focus();
  } else {
    //debug("Open a new history dialog");

    if (true == gDisableHistory) {
      //debug("Recently opened one. Wait a little bit.");
      return;
    }
    gDisableHistory = true;

    window.open( "chrome://communicator/content/history/history.xul", "_blank",
        "chrome,extrachrome,menubar,resizable,scrollbars,status,toolbar" );
    setTimeout(enableHistory, 2000);
  }

}

function checkTheme()
{
  var theSkinKids = document.getElementById("theme");
  var chromeRegistry = Components.classes["@mozilla.org/chrome/chrome-registry;1"]
    .getService(Components.interfaces.nsIXULChromeRegistry);
  for (var i = 0; i < theSkinKids.childNodes.length; ++i) {
    var child = theSkinKids.childNodes[i];
    var id=child.getAttribute("id");
    if (id.length > 0) {
      var themeName = id.substring('urn:mozilla:skin:'.length, id.length);       
      var selected = chromeRegistry.isSkinSelected(themeName, true);
      if (selected == Components.interfaces.nsIChromeRegistry.FULL) {
        var menuitem=document.getElementById(id);
        menuitem.setAttribute("checked", true);
        break;
      }
    }
  } 
}

function getWebNavigation()
{
  try {
    return getBrowser().webNavigation;
  } catch (e) {
    return null;
  }
}

function BrowserReloadWithFlags(reloadFlags)
{
  /* First, we'll try to use the session history object to reload so 
   * that framesets are handled properly. If we're in a special 
   * window (such as view-source) that has no session history, fall 
   * back on using the web navigation's reload method.
   */

  var webNav = getWebNavigation();
  try {
    var sh = webNav.sessionHistory;
    if (sh)
      webNav = sh.QueryInterface(Components.interfaces.nsIWebNavigation);
  } catch (e) {
  }

  try {
    webNav.reload(reloadFlags);
  } catch (e) {
  }
}

function toggleAffectedChrome(aHide)
{
  // chrome to toggle includes:
  //   (*) menubar
  //   (*) navigation bar
  //   (*) personal toolbar
  //   (*) tab browser ``strip''
  //   (*) sidebar

  if (!gChromeState)
    gChromeState = new Object;
  var navToolbox = document.getElementById("navigator-toolbox");
  navToolbox.hidden = aHide;
  var theTabbrowser = document.getElementById("content"); 

  // sidebar states map as follows:
  //   was-hidden    => hide/show nothing
  //   was-collapsed => hide/show only the splitter
  //   was-shown     => hide/show the splitter and the box
  if (aHide)
  {
    // going into print preview mode
    if (sidebar_is_collapsed())
    {
      gChromeState.sidebar = "was-collapsed";
    }
    else if (sidebar_is_hidden())
      gChromeState.sidebar = "was-hidden";
    else 
    {
      gChromeState.sidebar = "was-visible";
    }
    document.getElementById("sidebar-box").hidden = true;
    document.getElementById("sidebar-splitter").hidden = true;
    //deal with tab browser
    gChromeState.hadTabStrip = theTabbrowser.getStripVisibility();
    theTabbrowser.setStripVisibilityTo(false);
  }
  else
  {
    // restoring normal mode (i.e., leaving print preview mode)
    //restore tab browser
    theTabbrowser.setStripVisibilityTo(gChromeState.hadTabStrip);
    if (gChromeState.sidebar == "was-collapsed" ||
        gChromeState.sidebar == "was-visible")
      document.getElementById("sidebar-splitter").hidden = false;
    if (gChromeState.sidebar == "was-visible")
      document.getElementById("sidebar-box").hidden = false;
  }

  // if we are unhiding and sidebar used to be there rebuild it
  if (!aHide && gChromeState.sidebar == "was-visible")
    SidebarRebuild();
}

function showPrintPreviewToolbar()
{
  toggleAffectedChrome(true);
  const kXULNS = 
    "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

  var printPreviewTB = document.createElementNS(kXULNS, "toolbar");
  printPreviewTB.setAttribute("printpreview", true);
  printPreviewTB.setAttribute("id", "print-preview-toolbar");

  var navToolbox = document.getElementById("navigator-toolbox");
  navToolbox.parentNode.insertBefore(printPreviewTB, navToolbox);
}

function BrowserExitPrintPreview()
{
  gInPrintPreviewMode = false;

  var browser = getBrowser();
  browser.setAttribute("handleCtrlPageUpDown", "true");

  // exit print preview galley mode in content area
  var ifreq = _content.QueryInterface(
    Components.interfaces.nsIInterfaceRequestor);
  var webBrowserPrint = ifreq.getInterface(
    Components.interfaces.nsIWebBrowserPrint);     
  webBrowserPrint.exitPrintPreview(); 
  _content.focus();

  // remove the print preview toolbar
  var navToolbox = document.getElementById("navigator-toolbox");
  var printPreviewTB = document.getElementById("print-preview-toolbar");
  navToolbox.parentNode.removeChild(printPreviewTB);

  // restore chrome to original state
  toggleAffectedChrome(false);

  // restore old onclose handler if we found one before previewing
  var mainWin = document.getElementById("main-window");
  mainWin.setAttribute("onclose", gOldCloseHandler);
}

function GetPrintSettings()
{
  var prevPS = gPrintSettings;

  try {
    if (gPrintSettings == null) {
      var pref = Components.classes["@mozilla.org/preferences-service;1"]
                           .getService(Components.interfaces.nsIPrefBranch);
      if (pref) {
        gPrintSettingsAreGlobal = pref.getBoolPref("print.use_global_printsettings", false);
        gSavePrintSettings = pref.getBoolPref("print.save_print_settings", false);
      }

      var psService = Components.classes["@mozilla.org/gfx/printsettings-service;1"]
                                        .getService(Components.interfaces.nsIPrintSettingsService);
      if (gPrintSettingsAreGlobal) {
        gPrintSettings = psService.globalPrintSettings;        
        if (gSavePrintSettings) {
          psService.initPrintSettingsFromPrefs(gPrintSettings, false, gPrintSettings.kInitSaveNativeData);
        }
      } else {
        gPrintSettings = psService.newPrintSettings;
      }
    }
  } catch (e) {
    dump("GetPrintSettings "+e);
  }

  return gPrintSettings;
}

// This observer is called once the progress dialog has been "opened"
var gPrintPreviewObs = {
    observe: function(aSubject, aTopic, aData)
    {
      setTimeout(FinishPrintPreview, 0);
    },

    QueryInterface : function(iid)
    {
     if (iid.equals(Components.interfaces.nsIObserver) || iid.equals(Components.interfaces.nsISupportsWeakReference))
      return this;
     
     throw Components.results.NS_NOINTERFACE;
    }
};

function BrowserPrintPreview()
{
  var ifreq;
  var webBrowserPrint;  
  try {
    ifreq = _content.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
    webBrowserPrint = ifreq.getInterface(Components.interfaces.nsIWebBrowserPrint);     
    gPrintSettings = GetPrintSettings();

  } catch (e) {
    // Pressing cancel is expressed as an NS_ERROR_ABORT return value,
    // causing an exception to be thrown which we catch here.
    // Unfortunately this will also consume helpful failures, so add a
    // dump(e); // if you need to debug
  }

  // Here we get the PrintingPromptService tso we can display the PP Progress from script
  // For the browser implemented via XUL with the PP toolbar we cannot let it be
  // automatically opened from the print engine because the XUL scrollbars in the PP window
  // will layout before the content window and a crash will occur.
  //
  // Doing it all from script, means it lays out before hand and we can let printing do it's own thing
  gWebProgress = new Object();

  var printPreviewParams    = new Object();
  var notifyOnOpen          = new Object();
  var printingPromptService = Components.classes["@mozilla.org/embedcomp/printingprompt-service;1"]
                                  .getService(Components.interfaces.nsIPrintingPromptService);
  if (printingPromptService) {
    // just in case we are already printing, 
    // an error code could be returned if the Prgress Dialog is already displayed
    try {
      printingPromptService.showProgress(this, webBrowserPrint, gPrintSettings, gPrintPreviewObs, false, gWebProgress, 
                                         printPreviewParams, notifyOnOpen);
      if (printPreviewParams.value) {
        var webNav = getWebNavigation();
        printPreviewParams.value.docTitle = webNav.document.title;
        printPreviewParams.value.docURL   = webNav.currentURI.spec;
      }

      // this tells us whether we should continue on with PP or 
      // wait for the callback via the observer
      if (!notifyOnOpen.value.valueOf() || gWebProgress.value == null) {
        FinishPrintPreview();
      }
    } catch (e) {
      FinishPrintPreview();
    }
  }
}

function FinishPrintPreview()
{
  gInPrintPreviewMode = true;

  var browser = getBrowser();
  try {
    var ifreq = _content.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
    var webBrowserPrint = ifreq.getInterface(Components.interfaces.nsIWebBrowserPrint);     
    if (webBrowserPrint) {
      gPrintSettings = GetPrintSettings();
      webBrowserPrint.printPreview(gPrintSettings, null, gWebProgress.value);
    }

    browser.setAttribute("handleCtrlPageUpDown", "false");

    var mainWin = document.getElementById("main-window");

    // save previous close handler to restoreon exiting print preview mode
    if (mainWin.hasAttribute("onclose"))
      gOldCloseHandler = mainWin.getAttribute("onclose");
    else
      gOldCloseHandler = null;
    mainWin.setAttribute("onclose", "BrowserExitPrintPreview(); return false;");
 
    // show the toolbar after we go into print preview mode so
    // that we can initialize the toolbar with total num pages
    showPrintPreviewToolbar();

    _content.focus();
  } catch (e) {
    // Pressing cancel is expressed as an NS_ERROR_ABORT return value,
    // causing an exception to be thrown which we catch here.
    // Unfortunately this will also consume helpful failures, so add a
    // dump(e); // if you need to debug
  }
}


function BrowserPrintSetup()
{
  var didOK = false;
  try {
    var ifreq = _content.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
    var webBrowserPrint = ifreq.getInterface(Components.interfaces.nsIWebBrowserPrint);     
    if (webBrowserPrint) {
      gPrintSettings = GetPrintSettings();
    }

    didOK = goPageSetup(window, gPrintSettings);  // from utilityOverlay.js
    if (didOK) {  // from utilityOverlay.js

      if (webBrowserPrint) {
        if (gPrintSettingsAreGlobal && gSavePrintSettings) {
          var psService = Components.classes["@mozilla.org/gfx/printsettings-service;1"]
                                            .getService(Components.interfaces.nsIPrintSettingsService);
          psService.savePrintSettingsToPrefs(gPrintSettings, false, gPrintSettings.kInitSaveNativeData);
        }
      }
    }
  } catch (e) {
    dump("BrowserPrintSetup "+e);
  }
  return didOK;
}

function BrowserPrint()
{
  try {
    var ifreq = _content.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
    var webBrowserPrint = ifreq.getInterface(Components.interfaces.nsIWebBrowserPrint);     
    if (webBrowserPrint) {
      gPrintSettings = GetPrintSettings();
      webBrowserPrint.print(gPrintSettings, null);
    }
  } catch (e) {
    // Pressing cancel is expressed as an NS_ERROR_ABORT return value,
    // causing an exception to be thrown which we catch here.
    // Unfortunately this will also consume helpful failures, so add a
    // dump(e); // if you need to debug
  }
}

function BrowserSetDefaultCharacterSet(aCharset)
{
  // no longer needed; set when setting Force; see bug 79608
}

function BrowserSetForcedCharacterSet(aCharset)
{
  var docCharset = getBrowser().docShell.QueryInterface(
                            Components.interfaces.nsIDocCharset);
  docCharset.charset = aCharset;
  BrowserReloadWithFlags(nsIWebNavigation.LOAD_FLAGS_CHARSET_CHANGE);
}

function BrowserSetForcedDetector()
{
  getBrowser().documentCharsetInfo.forcedDetector = true;
}

function BrowserFind()
{
  var focusedWindow = document.commandDispatcher.focusedWindow;
  if (!focusedWindow || focusedWindow == window)
    focusedWindow = window._content;

  findInPage(getBrowser(), window._content, focusedWindow)
}

function BrowserFindAgain()
{
    var focusedWindow = document.commandDispatcher.focusedWindow;
    if (!focusedWindow || focusedWindow == window)
      focusedWindow = window._content;

  findAgainInPage(getBrowser(), window._content, focusedWindow)
}

function BrowserCanFindAgain()
{
  return canFindAgainInPage();
}

function getMarkupDocumentViewer()
{
  return getBrowser().markupDocumentViewer;
}

/**
 * Content area tooltip.
 * XXX - this must move into XBL binding/equiv! Do not want to pollute
 *       navigator.js with functionality that can be encapsulated into
 *       browser widget. TEMPORARY!
 *
 * NOTE: Any changes to this routine need to be mirrored in ChromeListener::FindTitleText()
 *       (located in mozilla/embedding/browser/webBrowser/nsDocShellTreeOwner.cpp)
 *       which performs the same function, but for embedded clients that
 *       don't use a XUL/JS layer. It is important that the logic of
 *       these two routines be kept more or less in sync.
 *       (pinkerton)
 **/
function FillInHTMLTooltip(tipElement)
{
  var retVal = false;
  if (tipElement.namespaceURI == "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul")
    return retVal;

  const XLinkNS = "http://www.w3.org/1999/xlink";


  var titleText = null;
  var XLinkTitleText = null;
  
  while (!titleText && !XLinkTitleText && tipElement) {
    if (tipElement.nodeType == Node.ELEMENT_NODE) {
      titleText = tipElement.getAttribute("title");
      XLinkTitleText = tipElement.getAttributeNS(XLinkNS, "title");
    }
    tipElement = tipElement.parentNode;
  }

  var texts = [titleText, XLinkTitleText];
  var tipNode = document.getElementById("aHTMLTooltip");

  for (var i = 0; i < texts.length; ++i) {
    var t = texts[i];
    if (t && t.search(/\S/) >= 0) {
      tipNode.setAttribute("label", t);
      retVal = true;
    }
  }

  return retVal;
}

var gBookmarksShell = null;

///////////////////////////////////////////////////////////////////////////////
// Class which defines methods for a bookmarks UI implementation based around
// a toolbar. Subclasses BookmarksBase in bookmarksOverlay.js. Some methods
// are required by the base class, others are for event handling. Window specific
// glue code should go into the BookmarksWindow class in bookmarks.js
function BookmarksToolbar (aID)
{
  this.id = aID;
}

BookmarksToolbar.prototype = {
  __proto__: BookmarksUIElement.prototype,

  /////////////////////////////////////////////////////////////////////////////
  // Personal Toolbar Specific Stuff
  
  get db ()
  {
    return this.element.database;
  },

  get element ()
  {
    return document.getElementById(this.id);
  },

  /////////////////////////////////////////////////////////////////////////////
  // This method constructs a menuitem for a context menu for the given command.
  // This is implemented by the client so that it can intercept menuitem naming
  // as appropriate.
  createMenuItem: function (aDisplayName, aCommandName, aItemNode)
  {
    const kXULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    var xulElement = document.createElementNS(kXULNS, "menuitem");
    xulElement.setAttribute("cmd", aCommandName);
    var cmd = "cmd_" + aCommandName.substring(NC_NS_CMD.length)
    xulElement.setAttribute("command", cmd);
    
    switch (aCommandName) {
    case NC_NS_CMD + "bm_open":
      xulElement.setAttribute("label", aDisplayName);
      xulElement.setAttribute("default", "true");
      break;
    case NC_NS_CMD + "bm_openfolder":
      xulElement.setAttribute("default", "true");
      if (aItemNode.localName == "hbox") 
        // Don't show an "Open Folder" item for clicks on the toolbar itself.
        return null;
    default:
      xulElement.setAttribute("label", aDisplayName);
      break;
    }
    return xulElement;
  },

  // Command implementation
  commands: {
    openFolder: function (aSelectedItem)
    {
      var mbo = aSelectedItem.boxObject.QueryInterface(Components.interfaces.nsIMenuBoxObject);
      mbo.openMenu(true);
    },

    editCell: function (aSelectedItem, aXXXLameAssIndex)
    {
      goDoCommand("cmd_bm_properties");
      return; // Disable Inline Edit for now. See bug 77125 for why this is being disabled
              // on the personal toolbar for the moment. 

      if (aSelectedItem.getAttribute("editable") != "true")
        return;
      var property = "http://home.netscape.com/NC-rdf#Name";
      aSelectedItem.setMode("edit");
      aSelectedItem.addObserver(this.postModifyCallback, "accept", 
                                [gBookmarksShell, aSelectedItem, property]);
    },

    ///////////////////////////////////////////////////////////////////////////
    // Called after an inline-edit cell has left inline-edit mode, and data
    // needs to be modified in the datasource.
    postModifyCallback: function (aParams)
    {
      aParams[0].propertySet(aParams[1].id, aParams[2], aParams[3]);
    },

    ///////////////////////////////////////////////////////////////////////////
    // Creates a dummy item that can be placed in edit mode to retrieve data
    // to create new bookmarks/folders.
    createBookmarkItem: function (aMode, aSelectedItem)
    {
      /////////////////////////////////////////////////////////////////////////
      // HACK HACK HACK HACK HACK         
      // Disable Inline-Edit for now and just use a dialog. 
      
      // XXX - most of this is just copy-pasted from the other two folder
      //       creation functions. Yes it's ugly, but it'll do the trick for 
      //       now as this is in no way intended to be a long-term solution.

      const kPromptSvcContractID = "@mozilla.org/embedcomp/prompt-service;1";
      const kPromptSvcIID = Components.interfaces.nsIPromptService;
      const kPromptSvc = Components.classes[kPromptSvcContractID].getService(kPromptSvcIID);
      
      var defaultValue  = gBookmarksShell.getLocaleString("ile_newfolder");
      var dialogTitle   = gBookmarksShell.getLocaleString("newfolder_dialog_title");
      var dialogMsg     = gBookmarksShell.getLocaleString("newfolder_dialog_msg");
      var stringValue   = { value: defaultValue };
      if (kPromptSvc.prompt(window, dialogTitle, dialogMsg, stringValue, null, { value: 0 })) {
        var relativeNode = aSelectedItem || gBookmarksShell.element;
        var parentNode = relativeNode ? gBookmarksShell.findRDFNode(relativeNode, false) : gBookmarksShell.element;

        var args = [{ property: NC_NS + "parent",
                      resource: parentNode.id },
                    { property: NC_NS + "Name",
                      literal:  stringValue.value }];
        
        const kBMDS = gBookmarksShell.RDF.GetDataSource("rdf:bookmarks");
        var relId = relativeNode ? relativeNode.id : "NC:PersonalToolbarFolder";
        BookmarksUtils.doBookmarksCommand(relId, NC_NS_CMD + "newfolder", args);
      }
      
      return; 
      
      // HACK HACK HACK HACK HACK         
      /////////////////////////////////////////////////////////////////////////
      
      const kXULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
      var dummyButton = document.createElementNS(kXULNS, "menubutton");
      dummyButton = gBookmarksShell.createBookmarkFolderDecorations(dummyButton);
      dummyButton.setAttribute("class", "button-toolbar bookmark-item");

      dummyButton.setAttribute("label", gBookmarksShell.getLocaleString("ile_newfolder") + "  ");
      // By default, create adjacent to the selected button. If there is no button after
      // the selected button, or the target is the toolbar itself, just append. 
      var bIsButton = aSelectedItem.localName == "button" || aSelectedItem.localName == "menubutton";
      if (aSelectedItem.nextSibling && bIsButton)
        aSelectedItem.parentNode.insertBefore(dummyButton, aSelectedItem.nextSibling);
      else
        (bIsButton ? aSelectedItem.parentNode : aSelectedItem).appendChild(dummyButton);

      gBookmarksShell._focusElt = document.commandDispatcher.focusedElement;
      dummyButton.setMode("edit");
      // |aSelectedItem| will be the node we create the new folder relative to. 
      dummyButton.addObserver(this.onEditFolderName, "accept", 
                              [dummyButton, aSelectedItem, dummyButton]);
      dummyButton.addObserver(this.onEditFolderName, "reject", 
                              [dummyButton, aSelectedItem, dummyButton]);
    },

    ///////////////////////////////////////////////////////////////////////////
    // Edit folder name & update the datasource if name is valid
    onEditFolderName: function (aParams, aTopic)
    {
      // Because the toolbar has no concept of selection, this function
      // is much simpler than the one in bookmarksTree.js. However it may
      // become more complex if pink ever lets me put context menus on menus ;) 
      var name = aParams[3];
      var dummyButton = aParams[2];
      var relativeNode = aParams[1];
      var parentNode = gBookmarksShell.findRDFNode(relativeNode, false);

      dummyButton.parentNode.removeChild(dummyButton);

      if (!gBookmarksShell.commands.validateNameAndTopic(name, aTopic, relativeNode, dummyButton))
        return;

      parentNode = relativeNode.parentNode;
      if (relativeNode.localName == "hbox") {
        parentNode = relativeNode;
        relativeNode = (gBookmarksShell.commands.nodeIsValidType(relativeNode) && 
                        relativeNode.lastChild) || relativeNode;
      }

      var args = [{ property: NC_NS + "parent",
                    resource: parentNode.id },
                  { property: NC_NS + "Name",
                    literal:  name }];

      BookmarksUtils.doBookmarksCommand(relativeNode.id, NC_NS_CMD + "newfolder", args);
      // We need to do this because somehow focus shifts and no commands 
      // operate any more. 
      //gBookmarksShell._focusElt.focus();
    },

    nodeIsValidType: function (aNode)
    {
      switch (aNode.localName) {
      case "button":
      case "menubutton":
      // case "menu":
      // case "menuitem":
        return true;
      }
      return false;
    },

    ///////////////////////////////////////////////////////////////////////////
    // Performs simple validation on what the user has entered:
    //  1) prevents entering an empty string
    //  2) in the case of a canceled operation, remove the dummy item and
    //     restore selection.
    validateNameAndTopic: function (aName, aTopic, aOldSelectedItem, aDummyItem)
    {
      // Don't allow user to enter an empty string "";
      if (!aName) return false;

      // If the user hit escape, go no further.
      return !(aTopic == "reject");
    }
  },

  _focusElt: null,

  /////////////////////////////////////////////////////////////////////////////
  // Evaluates an event to determine whether or not it affords opening a tree
  // item. Typically, this is when the left mouse button is used, and provided
  // the click-rate matches that specified by our owning tree class. For example,
  // some trees open an item when double clicked (bookmarks/history windows) and
  // others on a single click (sidebar panels).
  isValidOpenEvent: function (aEvent)
  {
    return !(aEvent.type == "click" &&
             (aEvent.button != 0 || aEvent.detail != this.openClickCount))
  },

  /////////////////////////////////////////////////////////////////////////////
  // For the given selection, selects the best adjacent element. This method is
  // useful when an action such as a cut or a deletion is performed on a
  // selection, and focus/selection needs to be restored after the operation
  // is performed.
  getNextElement: function (aElement)
  {
    if (aElement.nextSibling)
      return aElement.nextSibling;
    else if (aElement.previousSibling)
      return aElement.previousSibling;
    else
      return aElement.parentNode;
  },

  selectElement: function (aElement)
  {
  },

  //////////////////////////////////////////////////////////////////////////////
  // Add the treeitem element specified by aURI to the tree's current selection.
  addItemToSelection: function (aURI)
  {
  },

  /////////////////////////////////////////////////////////////////////////////
  // Return a set of DOM nodes that represents the current item in the Bookmarks
  // Toolbar. This is always |document.popupNode|.
  getSelection: function ()
  {
    return [document.popupNode];
  },

  /////////////////////////////////////////////////////////////////////////////
  // Return a set of DOM nodes that represent the selection in the tree widget.
  // This method is takes a node parameter which is the popupNode for the
  // document. If the popupNode is not contained by the selection, the
  // popupNode is selected and the new selection returned.
  getContextSelection: function (aItemNode)
  {
    return [aItemNode];
  },

  getSelectedFolder: function ()
  {
    return "NC:PersonalToolbarFolder";
  },

  /////////////////////////////////////////////////////////////////////////////
  // For a given start DOM element, find the enclosing DOM element that contains
  // the template builder RDF resource decorations (id, ref, etc). In the 
  // Toolbar case, this is always the popup node (until we're proven wrong ;)
  findRDFNode: function (aStartNode, aIncludeStartNodeFlag)
  {
    var temp = aStartNode;
    while (temp && temp.localName != (aIncludeStartNodeFlag ? "toolbarbutton" : "hbox")) 
      temp = temp.parentNode;
    return temp || this.element;
  },

  selectFolderItem: function (aFolderURI, aItemURI, aAdditiveFlag)
  {
    var folder = document.getElementById(aFolderURI);
    var kids = ContentUtils.childByLocalName(folder, "treechildren");
    if (!kids) return;

    var item = kids.firstChild;
    while (item) {
      if (item.id == aItemURI) break;
      item = item.nextSibling;
    }
    if (!item) return;

    this.tree[aAdditiveFlag ? "addItemToSelection" : "selectItem"](item);
  },

  /////////////////////////////////////////////////////////////////////////////
  // Command handling & Updating.
  controller: {
    supportsCommand: function (aCommand)
    {
      switch(aCommand) {
      case "cmd_bm_undo":
      case "cmd_bm_redo":
        return false;
      case "cmd_bm_cut":
      case "cmd_bm_copy":
      case "cmd_bm_paste":
      case "cmd_bm_delete":
      case "cmd_bm_selectAll":
      case "cmd_bm_open":
      case "cmd_bm_openfolder":
      case "cmd_bm_openinnewwindow":
      case "cmd_bm_newbookmark":
      case "cmd_bm_newfolder":
      case "cmd_bm_newseparator":
      case "cmd_bm_find":
      case "cmd_bm_properties":
      case "cmd_bm_rename":
      case "cmd_bm_setnewbookmarkfolder":
      case "cmd_bm_setpersonaltoolbarfolder":
      case "cmd_bm_setnewsearchfolder":
      case "cmd_bm_import":
      case "cmd_bm_export":
      case "cmd_bm_fileBookmark":
        return true;
      default:
        return false;
      }
    },

    isCommandEnabled: function (aCommand)
    {
      switch(aCommand) {
      case "cmd_bm_undo":
      case "cmd_bm_redo":
        return false;
      case "cmd_bm_paste":
        var cp = gBookmarksShell.canPaste();
        return cp;
      case "cmd_bm_cut":
      case "cmd_bm_copy":
      case "cmd_bm_delete":
        return document.popupNode && document.popupNode.id != "NC:PersonalToolbarFolder";
      case "cmd_bm_selectAll":
        return false;
      case "cmd_bm_open":
        var seln = gBookmarksShell.getSelection();
        return document.popupNode != null && seln[0].getAttributeNS(RDF_NS, "type") == NC_NS + "Bookmark";
      case "cmd_bm_openfolder":
        seln = gBookmarksShell.getSelection();
        return document.popupNode != null && seln[0].getAttributeNS(RDF_NS, "type") == NC_NS + "Folder";
      case "cmd_bm_openinnewwindow":
        return true;
      case "cmd_bm_find":
      case "cmd_bm_newbookmark":
      case "cmd_bm_newfolder":
      case "cmd_bm_newseparator":
      case "cmd_bm_import":
      case "cmd_bm_export":
        return true;
      case "cmd_bm_properties":
      case "cmd_bm_rename":
        return document.popupNode != null;
      case "cmd_bm_setnewbookmarkfolder":
        seln = gBookmarksShell.getSelection();
        if (!seln.length) return false;
        var folderType = seln[0].getAttributeNS(RDF_NS, "type") == (NC_NS + "Folder");
        return document.popupNode && seln[0].id != "NC:NewBookmarkFolder" && folderType;
      case "cmd_bm_setpersonaltoolbarfolder":
        seln = gBookmarksShell.getSelection();
        if (!seln.length) return false;
        folderType = seln[0].getAttributeNS(RDF_NS, "type") == (NC_NS + "Folder");
        return document.popupNode && seln[0].id != "NC:PersonalToolbarFolder" && folderType;
      case "cmd_bm_setnewsearchfolder":
        seln = gBookmarksShell.getSelection();
        if (!seln.length) return false;
        folderType = seln[0].getAttributeNS(RDF_NS, "type") == (NC_NS + "Folder");
        return document.popupNode && seln[0].id != "NC:NewSearchFolder" && folderType;
      case "cmd_bm_fileBookmark":
        seln = gBookmarksShell.getSelection();
        return seln.length > 0;
      default:
        return false;
      }
    },

    doCommand: function (aCommand)
    {
      switch(aCommand) {
      case "cmd_bm_undo":
      case "cmd_bm_redo":
        break;
      case "cmd_bm_paste":
      case "cmd_bm_copy":
      case "cmd_bm_cut":
      case "cmd_bm_delete":
      case "cmd_bm_newbookmark":
      case "cmd_bm_newfolder":
      case "cmd_bm_newseparator":
      case "cmd_bm_properties":
      case "cmd_bm_rename":
      case "cmd_bm_open":
      case "cmd_bm_openfolder":
      case "cmd_bm_openinnewwindow":
      case "cmd_bm_setnewbookmarkfolder":
      case "cmd_bm_setpersonaltoolbarfolder":
      case "cmd_bm_setnewsearchfolder":
      case "cmd_bm_find":
      case "cmd_bm_import":
      case "cmd_bm_export":
      case "cmd_bm_fileBookmark":
        gBookmarksShell.execCommand(aCommand.substring("cmd_".length));
        break;
      case "cmd_bm_selectAll":
        break;
      }
    },

    onEvent: function (aEvent)
    {
    },

    onCommandUpdate: function ()
    {
    }
  }
};

function BM_navigatorLoad(aEvent)
{
  if (!gBookmarksShell) {
    gBookmarksShell = new BookmarksToolbar("NC:PersonalToolbarFolder");
    controllers.appendController(gBookmarksShell.controller);
    removeEventListener("load", BM_navigatorLoad, false);
  }
}


// An interim workaround for 101131 - Bookmarks Toolbar button nonfunctional.
// This simply checks to see if the bookmark menu is empty (aside from static
// items) when it is opened and if it is, prompts a rebuild. 
// The best fix for this is more time consuming, and relies on document
// <template>s without content (referencing a remote <template/> by id) 
// be noted as 'waiting' for a template to load from somewhere. When the 
// ::Merge function in nsXULDocument is called and a template node inserted, 
// the id of the template to be inserted is looked up in the map of waiting
// references, and then the template builder hooked up. 
function checkBookmarksMenuTemplateBuilder()
{
  var lastStaticSeparator = document.getElementById("lastStaticSeparator");
  if (!lastStaticSeparator.nextSibling) {
    var button = document.getElementById("bookmarks-button");
    button.builder.rebuild();
  }
}

addEventListener("load", BM_navigatorLoad, false);

function _RDF(aType)
  {
    return "http://www.w3.org/1999/02/22-rdf-syntax-ns#" + aType;
  }
function NC_RDF(aType)
  {
    return "http://home.netscape.com/NC-rdf#" + aType;
  }

var RDFUtils = {
  getResource: function(aString)
    {
      return this.rdf.GetResource(aString, true);
    },

  getTarget: function(aDS, aSourceID, aPropertyID)
    {
      var source = this.getResource(aSourceID);
      var property = this.getResource(aPropertyID);
      return aDS.GetTarget(source, property, true);
    },

  getValueFromResource: function(aResource)
    {
      aResource = aResource.QueryInterface(Components.interfaces.nsIRDFResource);
      return aResource ? aResource.Value : null;
    },
  _rdf: null,
  get rdf() {
    if (!this._rdf) {
      this._rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"]
                            .getService(Components.interfaces.nsIRDFService);
    }
    return this._rdf;
  }
}

var proxyIconDNDObserver = {
  onDragStart: function (aEvent, aXferData, aDragAction)
    {
      var urlBar = document.getElementById("urlbar");

      // XXX - do we want to allow the user to set a blank page to their homepage?
      //       if so then we want to modify this a little to set about:blank as
      //       the homepage in the event of an empty urlbar.
      if (!urlBar.value) return;

      var urlString = urlBar.value + "\n" + window._content.document.title;
      var htmlString = "<a href=\"" + urlBar.value + "\">" + urlBar.value + "</a>";

      aXferData.data = new TransferData();
      aXferData.data.addDataForFlavour("text/x-moz-url", urlString);
      aXferData.data.addDataForFlavour("text/unicode", urlBar.value);
      aXferData.data.addDataForFlavour("text/html", htmlString);
    }
}

var homeButtonObserver = {
  onDragStart: function (aEvent, aXferData, aDragAction)
    {
      var homepage = nsPreferences.getLocalizedUnicharPref("browser.startup.homepage", "about:blank");

      if (homepage)
        {
          // XXX find a readable title string for homepage, perhaps do a history lookup.
          var htmlString = "<a href=\"" + homepage + "\">" + homepage + "</a>";
          aXferData.data = new TransferData();
          aXferData.data.addDataForFlavour("text/x-moz-url", homepage + "\n" + homepage);
          aXferData.data.addDataForFlavour("text/html", htmlString);
          aXferData.data.addDataForFlavour("text/unicode", homepage);
        }
    },

  onDrop: function (aEvent, aXferData, aDragSession)
    {
      var url = transferUtils.retrieveURLFromData(aXferData.data, aXferData.flavour.contentType);
      setTimeout(openHomeDialog, 0, url);
    },

  onDragOver: function (aEvent, aFlavour, aDragSession)
    {
      var statusTextFld = document.getElementById("statusbar-display");
      statusTextFld.label = gNavigatorBundle.getString("droponhomebutton");
      aDragSession.dragAction = Components.interfaces.nsIDragService.DRAGDROP_ACTION_LINK;
    },

  onDragExit: function (aEvent, aDragSession)
    {
      var statusTextFld = document.getElementById("statusbar-display");
      statusTextFld.label = "";
    },

  getSupportedFlavours: function ()
    {
      var flavourSet = new FlavourSet();
      flavourSet.appendFlavour("application/x-moz-file", "nsIFile");
      flavourSet.appendFlavour("text/x-moz-url");
      flavourSet.appendFlavour("text/unicode");
      return flavourSet;
    }
}

function openHomeDialog(aURL)
{
  var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);
  var promptTitle = gNavigatorBundle.getString("droponhometitle");
  var promptMsg   = gNavigatorBundle.getString("droponhomemsg");
  var okButton    = gNavigatorBundle.getString("droponhomeokbutton");
  var pressedVal = promptService.confirmEx(window, promptTitle, promptMsg,
                          (promptService.BUTTON_TITLE_IS_STRING * promptService.BUTTON_POS_0) +
                          (promptService.BUTTON_TITLE_CANCEL * promptService.BUTTON_POS_1),
                          okButton, null, null, null, {value:0});

  if (pressedVal == 0) {
    nsPreferences.setUnicharPref("browser.startup.homepage", aURL);
    setTooltipText("home-button", aURL);
  }
}

var goButtonObserver = {
  onDragOver: function(aEvent, aFlavour, aDragSession)
    {
      aEvent.target.setAttribute("dragover", "true");
      return true;
    },
  onDragExit: function (aEvent, aDragSession)
    {
      aEvent.target.removeAttribute("dragover");
    },
  onDrop: function (aEvent, aXferData, aDragSession)
    {
      var xferData = aXferData.data.split("\n");
      var uri = xferData[0] ? xferData[0] : xferData[1];
      if (uri)
        loadURI(uri);
    },
  getSupportedFlavours: function ()
    {
      var flavourSet = new FlavourSet();
      flavourSet.appendFlavour("application/x-moz-file", "nsIFile");
      flavourSet.appendFlavour("text/x-moz-url");
      flavourSet.appendFlavour("text/unicode");
      return flavourSet;
    }
}

var searchButtonObserver = {
  onDragOver: function(aEvent, aFlavour, aDragSession)
    {
      aEvent.target.setAttribute("dragover", "true");
      return true;
    },
  onDragExit: function (aEvent, aDragSession)
    {
      aEvent.target.removeAttribute("dragover");
    },
  onDrop: function (aEvent, aXferData, aDragSession)
    {
      var xferData = aXferData.data.split("\n");
      var uri = xferData[1] ? xferData[1] : xferData[0];
      if (uri)
        OpenSearch('internet',false, uri);
    },
  getSupportedFlavours: function ()
    {
      var flavourSet = new FlavourSet();
      flavourSet.appendFlavour("application/x-moz-file", "nsIFile");
      flavourSet.appendFlavour("text/x-moz-url");
      flavourSet.appendFlavour("text/unicode");
      return flavourSet;
    }
}

var personalToolbarDNDObserver = {

  ////////////////////
  // Public methods //
  ////////////////////

  onDragStart: function (aEvent, aXferData, aDragAction)
    {

      // Prevent dragging from an invalid region
      if (!this.canDrop(aEvent))
        return;

      // Prevent dragging out of menupopups on non Win32 platforms. 
      // a) on Mac drag from menus is generally regarded as being satanic
      // b) on Linux, this causes an X-server crash, see bug 79003, 96504 and 139471
      // c) on Windows, there is no hang or crash associated with this, so we'll leave 
      // the functionality there. 
      if (navigator.platform != "Win32" && aEvent.target.localName != "toolbarbutton")
        return;

      // prevent dragging folders in the personal toolbar and menus.
      // PCH: under linux, since containers open on mouse down, we hit bug 96504. 
      // In addition, a drag start is fired when leaving an open toolbarbutton(type=menu) 
      // menupopup (see bug 143031)
      if (this.isContainer(aEvent.target) && aEvent.target.getAttribute("group") != "true") {
        if (this.isPlatformNotSupported) 
          return;
        if (!aEvent.shiftKey && !aEvent.altKey)
          return;
        // menus open on mouse down
        aEvent.target.firstChild.hidePopup();
      }

      // Prevent dragging non-bookmark menuitem or menus
      var uri = aEvent.target.id;
      if (!this.isBookmark(uri))
        return;

      //PCH: cleanup needed here, url is already calculated in isBookmark()
      var db = document.getElementById("NC:PersonalToolbarFolder").database;
      var url = RDFUtils.getTarget(db, uri, NC_RDF("URL"));
      if (url)
        url = url.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
      else
        url = "";
      var name = RDFUtils.getTarget(db, uri, NC_RDF("Name"));
      if (name)
        name = name.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
      else
        name = "";
      var urlString = url + "\n" + name;
      var htmlString = "<A HREF='" + url + "'>" + name + "</A>";
      aXferData.data = new TransferData();
      aXferData.data.addDataForFlavour("moz/rdfitem", uri);
      aXferData.data.addDataForFlavour("text/x-moz-url", urlString);
      aXferData.data.addDataForFlavour("text/html", htmlString);
      aXferData.data.addDataForFlavour("text/unicode", url);

      return;
    },

  onDragOver: function(aEvent, aFlavour, aDragSession) 
  {
    if (aDragSession.canDrop)
      this.onDragSetFeedBack(aEvent);
    if (this.isPlatformNotSupported)
      return;
    if (this.isTimerSupported)
      return;
    this.onDragOverCheckTimers();
    return;
  },

  onDragEnter: function (aEvent, aDragSession)
  {
    var target = aEvent.target;
    if (target.localName == "menupopup" || target.localName == "hbox")
      target = target.parentNode;
    if (aDragSession.canDrop) {
      this.onDragSetFeedBack(aEvent);
      this.onDragEnterSetTimer(target, aDragSession);
    }
    this.mCurrentDragOverTarget = target;
    return;
  },

  onDragExit: function (aEvent, aDragSession)
  {
    var target = aEvent.target;
    if (target.localName == "menupopup" || target.localName == "hbox")
      target = target.parentNode;
    this.onDragRemoveFeedBack(target);
    this.onDragExitSetTimer(target, aDragSession);
    this.mCurrentDragOverTarget = null;
    return;
  },

  onDrop: function (aEvent, aXferData, aDragSession)
  {

    this.onDragRemoveFeedBack(aEvent.target);
    var dropPosition = this.determineDropPosition(aEvent);

    // PCH: BAD!!! We should use the flavor
    // this code should be merged with the one in bookmarks.xml
    var xferData = aXferData.data.split("\n");
    if (xferData[0] == "")
      return;
    var elementRes = RDFUtils.getResource(xferData[0]);

    var childDB = document.getElementById("NC:PersonalToolbarFolder").database;
    var rdfContainer = Components.classes["@mozilla.org/rdf/container;1"].createInstance(Components.interfaces.nsIRDFContainer);

    // if dragged url is already bookmarked, remove it from current location first
    var parentContainer = this.findParentContainer(aDragSession.sourceNode);
    if (parentContainer) {
      rdfContainer.Init(childDB, parentContainer);
      rdfContainer.RemoveElement(elementRes, false);
    }

    // determine charset of link
    var linkCharset = aDragSession.sourceDocument ? aDragSession.sourceDocument.characterSet : null;
    // determine title of link
    var linkTitle;
    // look it up in bookmarks
    var bookmarksDS = RDFUtils.rdf.GetDataSource("rdf:bookmarks");
    var nameRes = RDFUtils.getResource(NC_RDF("Name"));
    var nameFromBookmarks = bookmarksDS.GetTarget(elementRes, nameRes, true);
    if (nameFromBookmarks)
      nameFromBookmarks = nameFromBookmarks.QueryInterface(Components.interfaces.nsIRDFLiteral);
    if (nameFromBookmarks)
      linkTitle = nameFromBookmarks.Value;
    else if (xferData.length >= 2)
      linkTitle = xferData[1]
    else {
      // look up this URL's title in global history
      var historyDS = RDFUtils.rdf.GetDataSource("rdf:history");
      var titlePropRes = RDFUtils.getResource(NC_RDF("Name"));
      var titleFromHistory = historyDS.GetTarget(elementRes, titlePropRes, true);
      if (titleFromHistory)
        titleFromHistory = titleFromHistory.QueryInterface(Components.interfaces.nsIRDFLiteral);
      if (titleFromHistory)
        linkTitle = titleFromHistory.Value;
    }

    var dropIndex;
    if (aEvent.target.id == "bookmarks-button") 
      // dropPosition is always DROP_ON
      parentContainer = RDFUtils.getResource("NC:BookmarksRoot");
    else if (dropPosition == this.DROP_ON) 
      parentContainer = RDFUtils.getResource(aEvent.target.id);
    else {
      parentContainer = this.findParentContainer(aEvent.target);
      rdfContainer.Init(childDB, parentContainer);
      var dropElementRes = RDFUtils.getResource(aEvent.target.id);
      dropIndex = rdfContainer.IndexOf(dropElementRes);
    }
    switch (dropPosition) {
      case this.DROP_BEFORE:
        if (dropIndex<1) dropIndex = 1;
        break;
      case this.DROP_ON:
        dropIndex = -1;
        break;
      case this.DROP_AFTER:
        if (dropIndex <= rdfContainer.GetCount()) ++dropIndex;         
        if (dropIndex<1) dropIndex = -1;
        break;
    }

    this.insertBookmarkAt(xferData[0], linkTitle, linkCharset, parentContainer, dropIndex);       
    return;
  },

  canDrop: function (aEvent, aDragSession)
  {
    var target = aEvent.target;
    return target.id && target.localName != "menupopup" && target.localName != "toolbar" &&
           target.localName != "menuseparator" && target.localName != "toolbarseparator" &
           target.id != "NC:SystemBookmarksStaticRoot" &&
           target.id.substring(0,5) != "find:";
  },

  getSupportedFlavours: function () 
  {
    var flavourSet = new FlavourSet();
    flavourSet.appendFlavour("moz/rdfitem");
    flavourSet.appendFlavour("text/unicode");
    flavourSet.appendFlavour("application/x-moz-file", "nsIFile");
    flavourSet.appendFlavour("text/x-moz-url");
    return flavourSet;
  }, 
  

  ////////////////////////////////////
  // Private methods and properties //
  ////////////////////////////////////

  DROP_BEFORE:-1,
  DROP_ON    : 0,
  DROP_AFTER : 1,
  springLoadedMenuDelay: 350, // milliseconds
  isPlatformNotSupported: navigator.platform.indexOf("Linux") != -1 ||
                          navigator.platform.indexOf("Mac")   != -1, // see bug 136524
  isTimerSupported: navigator.platform.indexOf("Win") == -1,

  mCurrentDragOverTarget: null,
  loadTimer  : null,
  closeTimer : null,
  loadTarget : null,
  closeTarget: null,


  onDragCloseMenu: function (aNode)
  {
    var children = aNode.childNodes;
    for (var i = 0; i < children.length; i++) {
      if (children[i].id == "NC:PersonalToolbarFolder") {
        this.onDragCloseMenu(children[i]);
      }
      else if (this.isContainer(children[i]) && children[i].getAttribute("open") == "true") {
        this.onDragCloseMenu(children[i].firstChild);
        if (children[i] != this.mCurrentDragOverTarget)
          children[i].firstChild.hidePopup();
      }
    } 
  },

  onDragCloseTarget: function ()
  {
    // if the mouse is not over a menu, then close everything.
    if (!this.mCurrentDragOverTarget) {
      this.onDragCloseMenu(document.getElementById("PersonalToolbar"));
      return
    }
    // The bookmark button is not a sibling of the folders in the PT
    if (this.mCurrentDragOverTarget.parentNode.id == "NC:PersonalToolbarFolder")
      this.onDragCloseMenu(document.getElementById("PersonalToolbar"));
    else
      this.onDragCloseMenu(this.mCurrentDragOverTarget.parentNode);
  },

  onDragLoadTarget: function (aTarget) 
  {
    if (!this.mCurrentDragOverTarget)
      return;
    // Load the current menu
    if (this.isContainer(aTarget) && aTarget.getAttribute("group") != "true")
      aTarget.firstChild.showPopup(aTarget, -1, -1, "menupopup");
  },

  onDragOverCheckTimers: function ()
  {
    var now = new Date().getTime();
    if (this.closeTimer && now-this.springLoadedMenuDelay>this.closeTimer) {
      this.onDragCloseTarget();
      this.closeTimer = null;
    }
    if (this.loadTimer && (now-this.springLoadedMenuDelay>this.loadTimer)) {
      this.onDragLoadTarget(this.loadTarget);
      this.loadTimer = null;
    }
  },

  onDragEnterSetTimer: function (aTarget, aDragSession)
  {
    if (this.isPlatformNotSupported)
      return;
    if (this.isTimerSupported) {
      var targetToBeLoaded = aTarget;
      clearTimeout(this.loadTimer);
      if (aTarget == aDragSession.sourceNode)
        return;
      //XXX Hack: see bug 139645
      var thisHack = this;
      this.loadTimer=setTimeout(function () {thisHack.onDragLoadTarget(targetToBeLoaded)}, this.springLoadedMenuDelay);
    } else {
      var now = new Date().getTime();
      this.loadTimer  = now;
      this.loadTarget = aTarget;
    }
  },

  onDragExitSetTimer: function (aTarget, aDragSession)
  {
    if (this.isPlatformNotSupported)
      return;
    var thisHack = this;
    if (this.isTimerSupported) {
      clearTimeout(this.closeTimer)
      this.closeTimer=setTimeout(function () {thisHack.onDragCloseTarget()}, this.springLoadedMenuDelay);
    } else {
      var now = new Date().getTime();
      this.closeTimer  = now;
      this.closeTarget = aTarget;
      this.loadTimer = null;

      // If user isn't rearranging within the menu, close it
      // To do so, we exploit a Mac bug: timeout set during
      // drag and drop on Windows and Mac are fired only after that the drop is released.
      // timeouts will pile up, we may have a better approach but for the moment, this one
      // correctly close the menus after a drop/cancel outside the personal toolbar.
      // The if statement in the function has been introduced to deal with rare but reproducible
      // missing Exit events.
      if (aDragSession.sourceNode.localName != "menuitem" && aDragSession.sourceNode.localName != "menu")
        setTimeout(function () { if (thisHack.mCurrentDragOverTarget) {thisHack.onDragRemoveFeedBack(thisHack.mCurrentDragOverTarget); thisHack.mCurrentDragOverTarget=null} thisHack.loadTimer=null; thisHack.onDragCloseTarget() }, 0);
    }
  },

  onDragSetFeedBack: function (aEvent)
  {
    var target = aEvent.target
    var dropPosition = this.determineDropPosition(aEvent)
    switch (target.localName) {
      case "toolbarseparator":
      case "toolbarbutton":
        if (this.isContainer(target)) {
          target.setAttribute("dragover-left"  , "true");
          target.setAttribute("dragover-right" , "true");
          target.setAttribute("dragover-top"   , "true");
          target.setAttribute("dragover-bottom", "true");
        }
        else {
          switch (dropPosition) {
            case this.DROP_BEFORE: 
              target.removeAttribute("dragover-right");
              target.setAttribute("dragover-left", "true");
              break;
            case this.DROP_AFTER:
              target.removeAttribute("dragover-left");
              target.setAttribute("dragover-right", "true");
              break;
          }
        }
        break;
      case "menuseparator": 
      case "menu":
      case "menuitem":
        switch (dropPosition) {
          case this.DROP_BEFORE: 
            target.removeAttribute("dragover-bottom");
            target.setAttribute("dragover-top", "true");
            break;
          case this.DROP_AFTER:
            target.removeAttribute("dragover-top");
            target.setAttribute("dragover-bottom", "true");
            break;
        }
        break;
      case "hbox"     : 
        target.lastChild.setAttribute("dragover-right", "true");
        break;
      case "menupopup": 
      case "toolbar"  : break;
      default: dump("No feedback for: "+target.localName+"\n");
    }
  },

  onDragRemoveFeedBack: function (aTarget)
  {
    if (aTarget.localName == "hbox") {
      aTarget.lastChild.removeAttribute("dragover-right");
    } else {
      aTarget.removeAttribute("dragover-left");
      aTarget.removeAttribute("dragover-right");
      aTarget.removeAttribute("dragover-top");
      aTarget.removeAttribute("dragover-bottom");
    }
  },

  onDropSetFeedBack: function (aTarget)
  {
    //XXX Not yet...
  },

  isContainer: function (aTarget)
  {
    return (aTarget.localName == "menu" || aTarget.localName == "toolbarbutton") &&
           (aTarget.getAttribute("container") == "true" || aTarget.getAttribute("group") == "true");
  },


  ///////////////////////////////////////////////////////
  // Methods that need to be moved into BookmarksUtils //
  ///////////////////////////////////////////////////////

  //XXXPCH this function returns wrong vertical positions
  //XXX skin authors could break us, we'll cross that bridge when they turn us 90degrees
  determineDropPosition: function (aEvent)
  {
    var overButtonBoxObject = aEvent.target.boxObject.QueryInterface(Components.interfaces.nsIBoxObject);
    // most things only drop on the left or right
    var regionCount = 2;

    // you can drop ONTO containers, so there is a "middle" region
    if (this.isContainer(aEvent.target))
      return this.DROP_ON;
      
    var measure;
    var coordValue;
    var clientCoordValue;
    if (aEvent.target.localName == "menuitem") {
      measure = overButtonBoxObject.height/regionCount;
      coordValue = overButtonBoxObject.y;
      clientCoordValue = aEvent.clientY;
    }
    else if (aEvent.target.localName == "toolbarbutton") {
      measure = overButtonBoxObject.width/regionCount;
      coordValue = overButtonBoxObject.x;
      clientCoordValue = aEvent.clientX;
    }
    else
      return this.DROP_ON;
    
    // in the first region?
    if (clientCoordValue < (coordValue + measure))
      return this.DROP_BEFORE;
    // in the last region?
    if (clientCoordValue >= (coordValue + (regionCount - 1)*measure))
      return this.DROP_AFTER;
    // must be in the middle somewhere
    return this.DROP_ON;
  },

  isBookmark: function (aURI)
  {
    if (!aURI)
      return false;
    var db = document.getElementById("NC:PersonalToolbarFolder").database;
    var typeValue = RDFUtils.getTarget(db, aURI, _RDF("type"));
    typeValue = RDFUtils.getValueFromResource(typeValue);
    return (typeValue == NC_RDF("BookmarkSeparator") ||
            typeValue == NC_RDF("Bookmark") ||
            typeValue == NC_RDF("Folder"))
  },

  // returns the parent resource of the dragged element. This is determined
  // by inspecting the source element of the drag and walking up the DOM tree
  // to find the appropriate containing node.
  findParentContainer: function (aElement)
  {
    if (!aElement) return null;
    switch (aElement.localName) {
      case "toolbarbutton":
        var box = aElement.parentNode;
        return RDFUtils.getResource(box.getAttribute("ref"));
      case "menu":
      case "menuitem":
        var parentNode = aElement.parentNode.parentNode;
     
        if (parentNode.getAttribute("type") != NC_RDF("Folder") &&
            parentNode.getAttributeNS("http://www.w3.org/1999/02/22-rdf-syntax-ns#", "type") != "http://home.netscape.com/NC-rdf#Folder")
          return RDFUtils.getResource("NC:BookmarksRoot");
        return RDFUtils.getResource(parentNode.id);
      case "treecell":
        var treeitem = aElement.parentNode.parentNode.parentNode.parentNode;
        var res = treeitem.getAttribute("ref");
        if (!res)
          res = treeitem.id;            
        return RDFUtils.getResource(res);
    }
    return null;
  },

  insertBookmarkAt: function (aURL, aTitle, aCharset, aFolderRes, aIndex)
  {
    const kBMSContractID = "@mozilla.org/browser/bookmarks-service;1";
    const kBMSIID = Components.interfaces.nsIBookmarksService;
    const kBMS = Components.classes[kBMSContractID].getService(kBMSIID);
    kBMS.createBookmarkWithDetails(aTitle, aURL, aCharset, aFolderRes, aIndex);
  }

}

const MAX_HISTORY_MENU_ITEMS = 15;
const MAX_HISTORY_ITEMS = 100;
var gRDF = null;
var gRDFC = null;
var gGlobalHistory = null;
var gURIFixup = null;
var gLocalStore = null;

function FillHistoryMenu(aParent, aMenu)
  {
    // Remove old entries if any
    deleteHistoryItems(aParent);

    var sessionHistory = getWebNavigation().sessionHistory;

    var count = sessionHistory.count;
    var index = sessionHistory.index;
    var end;
    var j;
    var entry;

    switch (aMenu)
      {
        case "back":
          end = (index > MAX_HISTORY_MENU_ITEMS) ? index - MAX_HISTORY_MENU_ITEMS : 0;
          if ((index - 1) < end) return false;
          for (j = index - 1; j >= end; j--)
            {
              entry = sessionHistory.getEntryAtIndex(j, false);
              if (entry)
                createMenuItem(aParent, j, entry.title);
            }
          break;
        case "forward":
          end  = ((count-index) > MAX_HISTORY_MENU_ITEMS) ? index + MAX_HISTORY_MENU_ITEMS : count;
          if ((index + 1) >= end) return false;
          for (j = index + 1; j < end; j++)
            {
              entry = sessionHistory.getEntryAtIndex(j, false);
              if (entry)
                createMenuItem(aParent, j, entry.title);
            }
          break;
        case "go":
          aParent.lastChild.hidden = (count == 0);
          end = count > MAX_HISTORY_MENU_ITEMS ? count - MAX_HISTORY_MENU_ITEMS : 0;
          for (j = count - 1; j >= end; j--)
            {
              entry = sessionHistory.getEntryAtIndex(j, false);
              if (entry)
                createRadioMenuItem(aParent, j, entry.title, j==index);
            }
          break;
      }
    return true;
  }

function executeUrlBarHistoryCommand( aTarget )
  {
    var index = aTarget.getAttribute("index");
    var label = aTarget.getAttribute("label");
    if (index != "nothing_available" && label)
      {
        var uri = getShortcutOrURI(label);
        if (gURLBar) {
          gURLBar.value = uri;
          addToUrlbarHistory();
          BrowserLoadURL();
        }
        else
          loadURI(uri);
      }
  }

function createUBHistoryMenu( aParent )
  {
    if (!gRDF)
      gRDF = Components.classes["@mozilla.org/rdf/rdf-service;1"]
                       .getService(Components.interfaces.nsIRDFService);

    if (!gLocalStore)
      gLocalStore = gRDF.GetDataSource("rdf:local-store");

    if (gLocalStore) {
      if (!gRDFC)
        gRDFC = Components.classes["@mozilla.org/rdf/container-utils;1"]
                          .getService(Components.interfaces.nsIRDFContainerUtils);

      var entries = gRDFC.MakeSeq(gLocalStore, gRDF.GetResource("nc:urlbar-history")).GetElements();
      var i= MAX_HISTORY_MENU_ITEMS;

      // Delete any old menu items only if there are legitimate
      // urls to display, otherwise we want to display the
      // '(Nothing Available)' item.
      deleteHistoryItems(aParent);
      if (!entries.hasMoreElements()) {
        //Create the "Nothing Available" Menu item and disable it.
        var na = gNavigatorBundle.getString("nothingAvailable");
        createMenuItem(aParent, "nothing_available", na);
        aParent.firstChild.setAttribute("disabled", "true");
      }

      while (entries.hasMoreElements() && (i-- > 0)) {
        var entry = entries.getNext();
        if (entry) {
          try {
            entry = entry.QueryInterface(Components.interfaces.nsIRDFLiteral);
          } catch(ex) {
            // XXXbar not an nsIRDFLiteral for some reason. see 90337.
            continue;
          }
          var url = entry.Value;
          createMenuItem(aParent, i, url);
        }
      }
    }
  }

function addToUrlbarHistory()
{
  var urlToAdd = gURLBar.value;
  if (!urlToAdd)
     return;
  if (urlToAdd.search(/[\x00-\x1F]/) != -1) // don't store bad URLs
     return;

  if (!gRDF)
     gRDF = Components.classes["@mozilla.org/rdf/rdf-service;1"]
                      .getService(Components.interfaces.nsIRDFService);

  if (!gGlobalHistory)
    gGlobalHistory = Components.classes["@mozilla.org/browser/global-history;1"]
                               .getService(Components.interfaces.nsIBrowserHistory);
  
  if (!gURIFixup)
    gURIFixup = Components.classes["@mozilla.org/docshell/urifixup;1"]
                          .getService(Components.interfaces.nsIURIFixup);
  if (!gLocalStore)
     gLocalStore = gRDF.GetDataSource("rdf:local-store");

  if (gLocalStore) {
     if (!gRDFC)
        gRDFC = Components.classes["@mozilla.org/rdf/container-utils;1"]
                          .getService(Components.interfaces.nsIRDFContainerUtils);

       var entries = gRDFC.MakeSeq(gLocalStore, gRDF.GetResource("nc:urlbar-history"));
       if (!entries)
          return;
       var elements = entries.GetElements();
       if (!elements)
          return;
       var index = 0;
       // create the nsIURI objects for comparing the 2 urls
       var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                     .getService(Components.interfaces.nsIIOService);
       
       var entryToAdd = gRDF.GetLiteral(urlToAdd);

       try {
         ioService.extractScheme(urlToAdd, {}, {});
       } catch(e) {
         urlToAdd = "http://" + urlToAdd;
       }
       
       try {
         var uriToAdd  = ioService.newURI(urlToAdd, null, null);
       }
       catch(e) {
         // it isn't a valid url
         // we'll leave uriToAdd as "undefined" and handle that later
       }

       while(elements.hasMoreElements()) {
          var entry = elements.getNext();
          if (!entry) continue;

          index ++;
          try {
            entry = entry.QueryInterface(Components.interfaces.nsIRDFLiteral);
          } catch(ex) {
            // XXXbar not an nsIRDFLiteral for some reason. see 90337.
            continue;
          }
          var rdfValue = entry.Value;

          try {
            ioService.extractScheme(rdfValue, {}, {});
          } catch(e) {
            rdfValue = "http://" + rdfValue;
          }

          if (uriToAdd) {
            try {
              var rdfUri = ioService.newURI(rdfValue, null, null);
                 
              if (rdfUri.equals(uriToAdd)) {
                // URI already present in the database
                // Remove it from its current position.
                // It is inserted to the top after the while loop.
                entries.RemoveElementAt(index, true);
                break;
              }
            }
               
            // the uri is still not recognized by the ioservice
            catch(ex) {
              // no problem, we'll handle this below
            }
          }

          // if we got this far, then something is funky with the URIs,
          // so we need to do a straight string compare of the raw strings
          if (urlToAdd == rdfValue) {
            entries.RemoveElementAt(index, true);
            break;
          }
       }   // while

       // Otherwise, we've got a new URL in town. Add it!

       try {
         var url = entryToAdd.Value;
         if (url.indexOf(" ") == -1) {
           var fixedUpURI = gURIFixup.createFixupURI(url, 0);
           gGlobalHistory.markPageAsTyped(fixedUpURI.spec);
         }
       }
       catch(ex) {
       }

       // Put the value as it was typed by the user in to RDF
       // Insert it to the beginning of the list.
       entries.InsertElementAt(entryToAdd, 1, true);

       // Remove any expired history items so that we don't let
       // this grow without bound.
       for (index = entries.GetCount(); index > MAX_HISTORY_ITEMS; --index) {
           entries.RemoveElementAt(index, true);
       }  // for
   }  // localstore
}

function createMenuItem( aParent, aIndex, aLabel)
  {
    var menuitem = document.createElement( "menuitem" );
    menuitem.setAttribute( "label", aLabel );
    menuitem.setAttribute( "index", aIndex );
    aParent.appendChild( menuitem );
  }

function createRadioMenuItem( aParent, aIndex, aLabel, aChecked)
  {
    var menuitem = document.createElement( "menuitem" );
    menuitem.setAttribute( "type", "radio" );
    menuitem.setAttribute( "label", aLabel );
    menuitem.setAttribute( "index", aIndex );
    if (aChecked==true)
      menuitem.setAttribute( "checked", "true" );
    aParent.appendChild( menuitem );
  }

function deleteHistoryItems(aParent)
  {
    var children = aParent.childNodes;
    for (var i = 0; i < children.length; i++ )
      {
        var index = children[i].getAttribute( "index" );
        if (index)
          aParent.removeChild( children[i] );
      }
  }

function updateGoMenu(event)
  {
    FillHistoryMenu(event.target, "go");
  }

const nsIDOMWindowInternal = Components.interfaces.nsIDOMWindowInternal;
const nsIWindowMediator = Components.interfaces.nsIWindowMediator;
const nsIWindowDataSource = Components.interfaces.nsIWindowDataSource;

function toNavigator()
{
  if (!CycleWindow("navigator:browser"))
    OpenBrowserWindow();
}

function toDownloadManager()
{
  var dlmgr = Components.classes['@mozilla.org/download-manager;1'].getService();
  dlmgr = dlmgr.QueryInterface(Components.interfaces.nsIDownloadManager);

  var windowMediator = Components.classes['@mozilla.org/appshell/window-mediator;1'].getService();
  windowMediator = windowMediator.QueryInterface(nsIWindowMediator);

  var dlmgrWindow = windowMediator.getMostRecentWindow("Download:Manager");
  if (dlmgrWindow) {
    dlmgrWindow.focus();
  }
  else {
    dlmgr.open(window, null);
  }
}
  
function toJavaScriptConsole()
{
    toOpenWindowByType("global:console", "chrome://global/content/console.xul");
}

function javaItemEnabling()
{
    var element = document.getElementById("java");
    if (navigator.javaEnabled())
      element.removeAttribute("disabled");
    else
      element.setAttribute("disabled", "true");
}
            
function toJavaConsole()
{
    var jvmMgr = Components.classes['@mozilla.org/oji/jvm-mgr;1']
                            .getService(Components.interfaces.nsIJVMManager)
    jvmMgr.showJavaConsole();
}

function toOpenWindowByType( inType, uri )
{
  var windowManager = Components.classes['@mozilla.org/appshell/window-mediator;1'].getService();

  var windowManagerInterface = windowManager.QueryInterface(nsIWindowMediator);

  var topWindow = windowManagerInterface.getMostRecentWindow( inType );
  
  if ( topWindow )
    topWindow.focus();
  else
    window.open(uri, "_blank", "chrome,extrachrome,menubar,resizable,scrollbars,status,toolbar");
}


function OpenBrowserWindow()
{
  var charsetArg = new String();
  var handler = Components.classes['@mozilla.org/commandlinehandler/general-startup;1?type=browser'];
  handler = handler.getService();
  handler = handler.QueryInterface(Components.interfaces.nsICmdLineHandler);
  var startpage = handler.defaultArgs;
  // XXXBLAKE For now...
  // var url = handler.chromeUrlForTask;
  var url = "chrome://browser/content";
  var wintype = document.firstChild.getAttribute('windowtype');

  // if and only if the current window is a browser window and it has a document with a character
  // set, then extract the current charset menu setting from the current document and use it to
  // initialize the new browser window...
  if (window && (wintype == "navigator:browser") && window._content && window._content.document)
  {
    var DocCharset = window._content.document.characterSet;
    charsetArg = "charset="+DocCharset;

    //we should "inherit" the charset menu setting in a new window
    window.openDialog(url, "_blank", "chrome,all,dialog=no", startpage, charsetArg);
  }
  else // forget about the charset information.
  {
    window.openDialog(url, "_blank", "chrome,all,dialog=no", startpage);
  }
}

function OpenTaskURL( inURL )
{
  window.open( inURL );
}

function goAboutDialog()
{
  window.openDialog("chrome://global/content/about.xul", "About", "modal,chrome,resizable=yes,height=450,width=550");
}



