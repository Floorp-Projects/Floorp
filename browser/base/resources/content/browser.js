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
 *   Blake Ross <blakeross@telocity.com>
 *   Peter Annema <disttsc@bart.nl>
 *   Samir Gehani <sgehani@netscape.com>
 *
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
var gNavigatorRegionBundle;
var gLastValidURLStr = "";
var gHaveUpdatedToolbarState = false;
var gClickSelectsAll = -1;

var pref = null;

var appCore = null;

//cached elements
var gBrowser = null;

// focused frame URL
var gFocusedURL = null;
var gFocusedDocument = null;

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
    if (gBrowser.mTabContainer.childNodes.length == 1)
      gBrowser.setStripVisibilityTo(stripVisibility);
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

function ZoomManager() {
  // factorAnchor starts on factorOther
  this.factorOther = parseInt(gNavigatorBundle.getString("valueOther"));
  this.factorAnchor = this.factorOther;
}

ZoomManager.prototype = {
  instance : null,

  getInstance : function() {
    if (!ZoomManager.prototype.instance)
      ZoomManager.prototype.instance = new ZoomManager();

    return ZoomManager.prototype.instance;
  },

  MIN : 1,
  MAX : 2000,

  zoomFactorsString : "", // cache
  zoomFactors : null,

  factorOther : 300,
  factorAnchor : 300,
  steps : 0,

  get textZoom() {
    var currentZoom;
    try {
      currentZoom = Math.round(getMarkupDocumentViewer().textZoom * 100);
      if (this.indexOf(currentZoom) == -1) {
        if (currentZoom != this.factorOther) {
          this.factorOther = currentZoom;
          this.factorAnchor = this.factorOther;
        }
      }
    } catch (e) {
      currentZoom = 100;
    }
    return currentZoom;
  },

  set textZoom(aZoom) {
    if (aZoom < this.MIN || aZoom > this.MAX)
      throw Components.results.NS_ERROR_INVALID_ARG;

    getMarkupDocumentViewer().textZoom = aZoom / 100;
  },

  enlarge : function() {
    this.jump(1);
  },

  reduce : function() {
    this.jump(-1);
  },

  reset : function() {
    this.textZoom = 100;
  },

  getZoomFactors : function() {
    this.ensureZoomFactors();

    return this.zoomFactors;
  },

  indexOf : function(aZoom) {
    this.ensureZoomFactors();

    var index = -1;
    if (this.isZoomInRange(aZoom)) {
      index = this.zoomFactors.length - 1;
      while (index >= 0 && this.zoomFactors[index] != aZoom)
        --index;
    }

    return index;
  },

  /***** internal helper functions below here *****/

  ensureZoomFactors : function() {
    var zoomFactorsString = gNavigatorBundle.getString("values");
    if (this.zoomFactorsString != zoomFactorsString) {
      this.zoomFactorsString = zoomFactorsString;
      this.zoomFactors = zoomFactorsString.split(",");
      for (var i = 0; i<this.zoomFactors.length; ++i)
        this.zoomFactors[i] = parseInt(this.zoomFactors[i]);
    }
  },

  isLevelInRange : function(aLevel) {
    return (aLevel >= 0 && aLevel < this.zoomFactors.length);
  },

  isZoomInRange : function(aZoom) {
    return (aZoom >= this.zoomFactors[0] && aZoom <= this.zoomFactors[this.zoomFactors.length - 1]);
  },

  jump : function(aDirection) {
    if (aDirection != -1 && aDirection != 1)
      throw Components.results.NS_ERROR_INVALID_ARG;

    this.ensureZoomFactors();

    var currentZoom = this.textZoom;
    var insertIndex = -1;
    var stepFactor = parseFloat(gNavigatorBundle.getString("stepFactor"));

    // temporarily add factorOther to list
    if (this.isZoomInRange(this.factorOther)) {
      insertIndex = 0;
      while (this.zoomFactors[insertIndex] < this.factorOther)
        ++insertIndex;

      if (this.zoomFactors[insertIndex] != this.factorOther)
        this.zoomFactors.splice(insertIndex, 0, this.factorOther);
    }

    var factor;
    var done = false;

    if (this.isZoomInRange(currentZoom)) {
      var index = this.indexOf(currentZoom);
      if (aDirection == -1 && index == 0 ||
          aDirection ==  1 && index == this.zoomFactors.length - 1) {
        this.steps = 0;
        this.factorAnchor = this.zoomFactors[index];
      } else {
        factor = this.zoomFactors[index + aDirection];
        done = true;
      }
    }

    if (!done) {
      this.steps += aDirection;
      factor = this.factorAnchor * Math.pow(stepFactor, this.steps);
      if (factor < this.MIN || factor > this.MAX) {
        this.steps -= aDirection;
        factor = this.factorAnchor * Math.pow(stepFactor, this.steps);
      }
      factor = Math.round(factor);
      if (this.isZoomInRange(factor))
        factor = this.snap(factor);
      else
        this.factorOther = factor;
    }

    if (insertIndex != -1)
      this.zoomFactors.splice(insertIndex, 1);

    this.textZoom = factor;
  },

  snap : function(aZoom) {
    if (this.isZoomInRange(aZoom)) {
      var level = 0;
      while (this.zoomFactors[level + 1] < aZoom)
        ++level;

      // if aZoom closer to [level + 1] than [level], snap to [level + 1]
      if ((this.zoomFactors[level + 1] - aZoom) < (aZoom - this.zoomFactors[level]))
        ++level;

      aZoom = this.zoomFactors[level];
    }

    return aZoom;
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

function Startup()
{
  // init globals
  gNavigatorBundle = document.getElementById("bundle_navigator");
  gNavigatorRegionBundle = document.getElementById("bundle_navigator_region");

  gBrowser = document.getElementById("content");
  gURLBar = document.getElementById("urlbar");

  registerZoomManager();
  utilityOnLoad();
  
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
  //  so we'll be notified when onloads complete.
  var contentArea = document.getElementById("content");
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

  // XXXjag work-around for bug 113076
  // there's another bug where we throw an exception when getting
  // sessionHistory if it is null, which I'm exploiting here to
  // detect the situation described in bug 113076.
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
  }

  // hook up UI through progress listener
  getBrowser().addProgressListener(window.XULBrowserWindow);

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
      if ("arguments" in window && window.arguments.length >= 4) {
        loadURI(uriToLoad, window.arguments[3]);
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

    // Focus the content area if the caller instructed us to.
    if ("arguments" in window && window.arguments.length >= 3 && window.arguments[2] == true ||
        !window.locationbar.visible)
      setTimeout(WindowFocusTimerCallback, 0, _content);
    else
      setTimeout(WindowFocusTimerCallback, 0, gURLBar);

    // Perform default browser checking (after window opens).
    setTimeout( checkForDefaultBrowser, 0 );

    // hook up remote support
    if (XREMOTESERVICE_CONTRACTID in Components.classes) {
      var remoteService;
      remoteService = Components.classes[XREMOTESERVICE_CONTRACTID]
                                .getService(Components.interfaces.nsIXRemoteService);
      remoteService.addBrowserInstance(window);
    }
  }
  
  // called when we go into full screen, even if it is 
  // initiated by a web page script
  addEventListener("fullscreen", onFullScreen, false);

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
    var personalToolbar = document.getElementById("innermostBox");
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
  
  utilityOnUnload();

  // unregister us as a pref listener
  removePrefListener(gButtonPrefListener);
  removePrefListener(gTabStripPrefListener);

  window.browserContentListener.close();
  // Close the app core.
  if (appCore)
    appCore.close();
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
  return OpenBookmarkGroupFromResource(resource, datasource, rdf);
}

function OpenBookmarkGroupFromResource(resource, datasource, rdf) {
  var urlResource = rdf.GetResource("http://home.netscape.com/NC-rdf#URL");
  var rdfContainer = Components.classes["@mozilla.org/rdf/container;1"].getService(Components.interfaces.nsIRDFContainer);
  rdfContainer.Init(datasource, resource);
  var containerChildren = rdfContainer.GetElements();
  var tabPanels = gBrowser.mPanelContainer.childNodes;
  var tabCount = tabPanels.length;
  var index = 0;
  while (containerChildren.hasMoreElements()) {
    var resource = containerChildren.getNext().QueryInterface(Components.interfaces.nsIRDFResource);
    var target = datasource.GetTarget(resource, urlResource, true);
    if (target) {
      var uri = target.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
      if (index < tabCount)
        tabPanels[index].loadURI(uri, null, nsIWebNavigation.LOAD_FLAGS_NONE);
      else
        gBrowser.addTab(uri);
      index++;
    }
  }  
  
  if (index == 0)
    return; // If the bookmark group was completely invalid, just bail.
     
  // Select the first tab in the group.
  var tabs = gBrowser.mTabContainer.childNodes;
  gBrowser.selectedTab = tabs[0];
  
  // Close any remaining open tabs that are left over.
  for (var i = tabCount-1; i >= index; i--)
    gBrowser.removeTab(tabs[i]);
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
  kNC_Name = rdf.GetResource("http://home.netscape.com/NC-rdf#Name");
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


function OpenSearch(tabName, forceDialogFlag, searchStr, newWindowFlag)
{
  //This function needs to be split up someday.

  var defaultSearchURL = null;
  var fallbackDefaultSearchURL = gNavigatorRegionBundle.getString("fallbackDefaultSearchURL");
  ensureSearchPref()
  //Check to see if search string contains "://" or "ftp." or white space.
  //If it does treat as url and match for pattern
  
  var urlmatch= /(:\/\/|^ftp\.)[^ \S]+$/ 
  var forceAsURL = urlmatch.test(searchStr);

  try {
    defaultSearchURL = pref.getComplexValue("browser.search.defaulturl",
                                            Components.interfaces.nsIPrefLocalizedString).data;
  } catch (ex) {
  }

  // Fallback to a default url (one that we can get sidebar search results for)
  if (!defaultSearchURL)
    defaultSearchURL = fallbackDefaultSearchURL;

  if (!searchStr) {
    loadURI(gNavigatorRegionBundle.getString("otherSearchURL"));
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
        var windowManager = Components.classes["@mozilla.org/rdf/datasource;1?name=window-mediator"]
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
}

function BrowserOpenTab()
{
  gBrowser.selectedTab = gBrowser.addTab('about:blank');
  setTimeout("gURLBar.focus();", 0); 
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

function BrowserEditBookmarks()
{
  openDialog("chrome://communicator/content/bookmarks/bookmarks.xul", "Bookmarks",
             "chrome,all,dialog=no");
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
    BrowserViewSourceOfURL(url.replace(/^view-source:/, ""), null);
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

function BrowserViewSourceOfURL(url, charset)
{
  // try to open a view-source window while inheriting the charset (if any)
  openDialog("chrome://browser/content/viewSource.xul",
             "_blank",
             "scrollbars,resizable,chrome,dialog=no",
             url, charset);
}

// doc=null for regular page info, doc=owner document for frame info.
function BrowserPageInfo(doc)
{
  window.openDialog("chrome://browser/content/pageInfo.xul",
                    "_blank",
                    "chrome,dialog=no",
                    doc);
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

function URLBarFocusHandler(aEvent)
{
  if (gURLBar) {
    if (gClickSelectsAll == -1)
      gClickSelectsAll = pref.getBoolPref("browser.urlbar.clickSelectsAll");
    if (gClickSelectsAll)
      gURLBar.setSelectionRange(0, gURLBar.textLength);
  }
}

function URLBarBlurHandler(aEvent)
{
  // XXX why the hell do we have to do this?
  if (gClickSelectsAll)
    gURLBar.setSelectionRange(0, 0);
}

function URLBarKeyupHandler(aEvent)
{
  if (aEvent.keyCode == aEvent.DOM_VK_TAB ) {
    ShowAndSelectContentsOfURLBar();
  }
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
  }
}

function ShowAndSelectContentsOfURLBar()
{
  var navBar = document.getElementById("LocationToolbar");
  
  // If it's hidden, show it.
  if (navBar.getAttribute("hidden") == "true")
    goToggleToolbar('LocationToolbar','cmd_viewlocationbar');

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

function updateToolbarStates(toolbarMenuElt)
{
  if (gHaveUpdatedToolbarState)
    return;

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

function displayPageInfo()
{
  window.openDialog("chrome://navigator/content/pageInfo.xul", "_blank",
                    "dialog=no", null, "securityTab");
}

/** Document Zoom Management Code
 *
 * To use this, you'll need to have a <menu id="menu_textZoom"/>
 * and a getMarkupDocumentViewer() function which returns a
 * nsIMarkupDocumentViewer.
 *
 **/

function registerZoomManager()
{
  var textZoomMenu = document.getElementById("menu_textZoom");
  var zoom = ZoomManager.prototype.getInstance();

  var parentMenu = textZoomMenu.parentNode;
  parentMenu.addEventListener("popupshowing", updateViewMenu, false);

  var insertBefore = document.getElementById("menu_textZoomInsertBefore");
  var popup = insertBefore.parentNode;
  var accessKeys = gNavigatorBundle.getString("accessKeys").split(",");
  var zoomFactors = zoom.getZoomFactors();
  for (var i = 0; i < zoomFactors.length; ++i) {
    var menuItem = document.createElement("menuitem");
    menuItem.setAttribute("type", "radio");
    menuItem.setAttribute("name", "textZoom");

    var label;
    if (zoomFactors[i] == 100)
      label = gNavigatorBundle.getString("labelOriginal");
    else
      label = gNavigatorBundle.getString("label");

    menuItem.setAttribute("label", label.replace(/%zoom%/, zoomFactors[i]));
    menuItem.setAttribute("accesskey", accessKeys[i]);
    menuItem.setAttribute("oncommand", "ZoomManager.prototype.getInstance().textZoom = this.value;");
    menuItem.setAttribute("value", zoomFactors[i]);
    popup.insertBefore(menuItem, insertBefore);
  }
}

function updateViewMenu()
{
  var zoom = ZoomManager.prototype.getInstance();

  var textZoomMenu = document.getElementById("menu_textZoom");
  var menuLabel = gNavigatorBundle.getString("menuLabel").replace(/%zoom%/, zoom.textZoom);
  textZoomMenu.setAttribute("label", menuLabel);
}

function updateTextZoomMenu()
{
  var zoom = ZoomManager.prototype.getInstance();

  var currentZoom = zoom.textZoom;

  var textZoomOther = document.getElementById("menu_textZoomOther");
  var label = gNavigatorBundle.getString("labelOther");
  textZoomOther.setAttribute("label", label.replace(/%zoom%/, zoom.factorOther));
  textZoomOther.setAttribute("value", zoom.factorOther);

  var popup = document.getElementById("menu_textZoomPopup");
  var item = popup.firstChild;
  while (item) {
    if (item.getAttribute("name") == "textZoom") {
      if (item.getAttribute("value") == currentZoom)
        item.setAttribute("checked","true");
      else
        item.removeAttribute("checked");
    }
    item = item.nextSibling;
  }
}

function setTextZoomOther()
{
  var zoom = ZoomManager.prototype.getInstance();

  // open dialog and ask for new value
  var o = {value: zoom.factorOther, zoomMin: zoom.MIN, zoomMax: zoom.MAX};
  window.openDialog("chrome://communicator/content/askViewZoom.xul", "AskViewZoom", "chrome,modal,titlebar", o);
  if (o.zoomOK)
    zoom.textZoom = o.value;
}

function toHistory()
{
  openDialog("chrome://communicator/content/history/history.xul", "History",
             "chrome,all,dialog=no" );
}

function toDownloadManager()
{
  var dlmgr = Components.classes['@mozilla.org/download-manager;1'].getService();
  dlmgr = dlmgr.QueryInterface(Components.interfaces.nsIDownloadManager);
  dlmgr.open(window);
}
  
function toJavaScriptConsole()
{
  openDialog("chrome://global/content/console.xul", "JSConsole",
             "chrome,all,dialog=no" );
}

function javaItemEnabling()
{
    var element = document.getElementById("java");
    if (navigator.javaEnabled())
      element.removeAttribute("hidden");
    else
      element.setAttribute("hidden", "true");
}
            
function toJavaConsole()
{
    var jvmMgr = Components.classes['@mozilla.org/oji/jvm-mgr;1']
                            .getService(Components.interfaces.nsIJVMManager)
    jvmMgr.showJavaConsole();
}

function toOpenWindowByType( inType, uri )
{
  var windowManager = Components.classes['@mozilla.org/rdf/datasource;1?name=window-mediator'].getService();

  var windowManagerInterface = windowManager.QueryInterface( Components.interfaces.nsIWindowMediator);

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
  const url = "chrome://browser/content/browser.xul";
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

const nsIWebNavigation = Components.interfaces.nsIWebNavigation;
var gPrintSettings = null;
var gChromeState = null; // chrome state before we went into print preview
var gOldCloseHandler = null; // close handler before we went into print preview

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

  if (!gChromeState)
    gChromeState = new Object;
  var chrome = new Array;
  var i = 0;
  chrome[i++] = document.getElementById("main-menubar");
  chrome[i++] = document.getElementById("nav-bar");
  chrome[i++] = document.getElementById("LocationToolbar");
  chrome[i++] = document.getElementById("PersonalToolbar");
  chrome[i++] = document.getElementById("status-bar");

  // now that we've figured out which elements we're interested, toggle 'em
  for (i = 0; i < chrome.length; ++i)
  {
    if (aHide)
      chrome[i].hidden = true;
    else
      chrome[i].hidden = false;
  }
  
  // now deal with the tab browser ``strip'' 
  var theTabbrowser = document.getElementById("content"); 
  if (aHide) // normal mode -> print preview
  {
    gChromeState.hadTabStrip = theTabbrowser.getStripVisibility();
    theTabbrowser.setStripVisibilityTo(false);
  }
  else // print preview -> normal mode
  {
    // tabs were showing before entering print preview
    if (gChromeState.hadTabStrip) 
    {
      theTabbrowser.setStripVisibilityTo(true);
      gChromeState.hadTabStrip = false; // reset
    }
  }
}

function showPrintPreviewToolbar()
{
  toggleAffectedChrome(true);
  const kXULNS = 
    "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

  var printPreviewTB = document.createElementNS(kXULNS, "toolbar");
  printPreviewTB.setAttribute("printpreview", true);
  printPreviewTB.setAttribute("id", "print-preview-toolbar");
  var navTB = document.getElementById("nav-bar");
  navTB.parentNode.appendChild(printPreviewTB);
}

function BrowserExitPrintPreview()
{
  // exit print preview galley mode in content area
  var ifreq = _content.QueryInterface(
    Components.interfaces.nsIInterfaceRequestor);
  var webBrowserPrint = ifreq.getInterface(
    Components.interfaces.nsIWebBrowserPrint);     
  webBrowserPrint.exitPrintPreview(); 
  _content.focus();

  // remove the print preview toolbar
  var navTB = document.getElementById("nav-bar");
  var printPreviewTB = document.getElementById("print-preview-toolbar");
  navTB.parentNode.removeChild(printPreviewTB);

  // restore chrome to original state
  toggleAffectedChrome(false);

  // restore old onclose handler if we found one before previewing
  var mainWin = document.getElementById("main-window");
  mainWin.setAttribute("onclose", gOldCloseHandler);
}

function GetPrintSettings(webBrowserPrint)
{
  var prevPS = gPrintSettings;

  try {
    if (gPrintSettings == null) {
      var useGlobalPrintSettings = true;
      var pref = Components.classes["@mozilla.org/preferences-service;1"]
                           .getService(Components.interfaces.nsIPrefBranch);
      if (pref) {
        useGlobalPrintSettings = pref.getBoolPref("print.use_global_printsettings", false);
      }

      if (useGlobalPrintSettings) {
        gPrintSettings = webBrowserPrint.globalPrintSettings;
      } else {
        gPrintSettings = webBrowserPrint.newPrintSettings;
      }
    }
  } catch (e) {
    dump("GetPrintSettings "+e);
  }

  return gPrintSettings;
}

function BrowserPrintPreview()
{
  var mainWin = document.getElementById("main-window");

  // save previous close handler to restoreon exiting print preview mode
  if (mainWin.hasAttribute("onclose"))
    gOldCloseHandler = mainWin.getAttribute("onclose");
  else
    gOldCloseHandler = null;
  mainWin.setAttribute("onclose", "BrowserExitPrintPreview(); return false;");
 
  try {
    var ifreq = _content.QueryInterface(
      Components.interfaces.nsIInterfaceRequestor);
    var webBrowserPrint = ifreq.getInterface(
      Components.interfaces.nsIWebBrowserPrint);     
    if (webBrowserPrint) {
      gPrintSettings = GetPrintSettings(webBrowserPrint);
      webBrowserPrint.printPreview(gPrintSettings);
    }
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

  try {
    var ifreq = _content.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
    var webBrowserPrint = ifreq.getInterface(Components.interfaces.nsIWebBrowserPrint);     
    if (webBrowserPrint) {
      gPrintSettings = GetPrintSettings(webBrowserPrint);
    }

    goPageSetup(gPrintSettings);  // from utilityOverlay.js

    if (webBrowserPrint) {
      if (webBrowserPrint.doingPrintPreview) {
        webBrowserPrint.printPreview(gPrintSettings);
      }
    }

  } catch (e) {
    dump("BrowserPrintSetup "+e);
  }
}

function BrowserPrint()
{
  try {
    var ifreq = _content.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
    var webBrowserPrint = ifreq.getInterface(Components.interfaces.nsIWebBrowserPrint);     
    if (webBrowserPrint) {
      gPrintSettings = GetPrintSettings(webBrowserPrint);
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

function openPanel(aURI, aTitle)
{
  SidebarShowHide();
  var sidebarPane = document.getElementById("browserSidebar");
  sidebarPane.setAttribute("src", aURI);
  var sidebarHeader = document.getElementById("sidebarhdrSidebar");
  sidebarHeader.setAttribute("label", aTitle);
}

function SidebarShowHide() {
  var sidebar_box = document.getElementById('boxSidebar');
  var sidebar_splitter = document.getElementById('splitterSidebar');

  if (sidebar_is_hidden()) {
    // for older profiles: 
    sidebar_box.setAttribute('hidden', 'false'); 
    if (sidebar_splitter.getAttribute('state') == 'collapsed')
      sidebar_splitter.removeAttribute('state');
    sidebar_splitter.removeAttribute('hidden');
  } else {
      sidebar_box.setAttribute('hidden', 'true');
      sidebar_splitter.setAttribute('hidden', 'true');
  }
  persist_width();
  window._content.focus();
}

// sidebar_is_hidden() - Helper function for SidebarShowHide().
function sidebar_is_hidden() {
  var sidebar_box = document.getElementById('boxSidebar');
  return sidebar_box.getAttribute('hidden') == 'true';
}

function sidebar_is_collapsed() {
  var sidebar_splitter = document.getElementById('splitterSidebar');
  return (sidebar_splitter &&
          sidebar_splitter.getAttribute('state') == 'collapsed');
}

function SidebarExpandCollapse() {
  var sidebar_splitter = document.getElementById('splitterSidebar');
  var sidebar_box = document.getElementById('boxSidebar');
  if (sidebar_splitter.getAttribute('state') == 'collapsed') {
    sidebar_splitter.removeAttribute('state');
    sidebar_box.setAttribute('hidden', 'false');
  } else {
    sidebar_splitter.setAttribute('state', 'collapsed');
    sidebar_box.setAttribute('hidden', 'true');
  }
}
function persist_width() {
  // XXX Mini hack. Persist isn't working too well. Force the persist,
  // but wait until the width change has commited.
  setTimeout("document.persist('boxSidebar', 'width');",100);
}

function SidebarFinishClick() {

  // XXX Semi-hack for bug #16516.
  // If we had the proper drag event listener, we would not need this
  // timeout. The timeout makes sure the width is written to disk after
  // the sidebar-box gets the newly dragged width.
  setTimeout("persist_width()",100);
}

// SidebarCleanUpExpandCollapse() - Respond to grippy click.
function SidebarCleanUpExpandCollapse() {
  setTimeout("document.persist('boxSidebar', 'hidden');",100);
}
