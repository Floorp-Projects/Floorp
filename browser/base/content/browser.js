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
 *   Pierre Chanial <chanial@noos.fr>
 *   Jason Eager <jce2@po.cwru.edu>
 *   Joe Hewitt <hewitt@netscape.com>
 *   Alec Flett <alecf@netscape.com>
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
var gNavigatorBundle;
var gBrandBundle;
var gLastValidURLStr = "";
var gLastValidURL = null;
var gHaveUpdatedToolbarState = false;
var gClickSelectsAll = true;
var gIgnoreFocus = false;
var gIgnoreClick = false;
var gToolbarMode = "icons";
var gIconSize = "";
var gMustLoadSidebar = false;
var gURIFixup = null;

var gPrefService = null;

var appCore = null;

//cached elements
var gBrowser = null;

// Global variable that holds the nsContextMenu instance.
var gContextMenu = null;

// Global variable that caches the default search engine info
var gDefaultEngine = null;

const nsIWebNavigation = Components.interfaces.nsIWebNavigation;
var gPrintSettingsAreGlobal = true;
var gSavePrintSettings = true;
var gPrintSettings = null;
var gChromeState = null; // chrome state before we went into print preview
var gOldCloseHandler = null; // close handler before we went into print preview
var gInPrintPreviewMode = false;
var gWebProgress = null;
var gFormHistory = null;

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
  var saveFrameItem = document.getElementById("menu_saveFrame");
  if (!_content.frames.length || !isDocumentFrame(document.commandDispatcher.focusedWindow))
    saveFrameItem.setAttribute("hidden", "true");
  else
    saveFrameItem.removeAttribute("hidden");
}

//////////////////////////////// BOOKMARKS ////////////////////////////////////

function UpdateBookmarksLastVisitedDate(event)
{
  var url = getWebNavigation().currentURI.spec;
  if (url) {
    // if the URL is bookmarked, update its "Last Visited" date
    BMSVC.updateLastVisitedDate(url, _content.document.characterSet);
  }
}

function HandleBookmarkIcon(iconURL, addFlag)
{
  var url = getWebNavigation().currentURI.spec
  if (url) {
    // update URL with new icon reference
    if (addFlag)
      BMSVC.updateBookmarkIcon(url, iconURL);
    else
      BMSVC.removeBookmarkIcon(url, iconURL);
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
  if (!url) {
    var navigatorRegionBundle = document.getElementById("bundle_browser_region");
    url = navigatorRegionBundle.getString("homePageDefault");
  }

  return url;
}

function UpdateBackForwardButtons()
{
  var backBroadcaster = document.getElementById("Browser:Back");
  var forwardBroadcaster = document.getElementById("Browser:Forward");
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

function UpdatePageReport(event)
{
  if (!gReportButton)
    return;

  if (gBrowser.mCurrentBrowser.pageReport)
    gReportButton.setAttribute("blocked", "true");
  else
    gReportButton.removeAttribute("blocked");
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
  gNavigatorBundle = document.getElementById("bundle_browser");
  gBrandBundle = document.getElementById("bundle_brand");
  gBrowser = document.getElementById("content");
  gURLBar = document.getElementById("urlbar");
  gReportButton = document.getElementById("page-report-button");

  gBrowser.addEventListener("DOMUpdatePageReport", UpdatePageReport, false);

  var toolbox = document.getElementById("navigator-toolbox");
  toolbox.customizeDone = BrowserToolboxCustomizeDone;

  SetPageProxyState("invalid", null);

  var webNavigation;
  try {
    // Create the browser instance component.
    appCore = Components.classes["@mozilla.org/appshell/component/browser/instance;1"]
                        .createInstance(Components.interfaces.nsIBrowserInstance);
    if (!appCore)
      throw "couldn't create a browser instance";

    webNavigation = getWebNavigation();
    if (!webNavigation)
      throw "no XBL binding for browser";
  } catch (e) {
    alert("Error launching browser window:" + e);
    window.close(); // Give up.
    return;
  }

  // initialize observers and listeners
  window.XULBrowserWindow = new nsBrowserStatusHandler();
  window.browserContentListener =
    new nsBrowserContentListener(window, gBrowser);

  // Initialize browser instance..
  appCore.setWebShellWindow(window);

  gBrowser.addEventListener("load", loadEventHandlers, false);

  // set default character set if provided
  if ("arguments" in window && window.arguments.length > 1 && window.arguments[1]) {
    if (window.arguments[1].indexOf("charset=") != -1) {
      var arrayArgComponents = window.arguments[1].split("=");
      if (arrayArgComponents) {
        //we should "inherit" the charset menu setting in a new window
        getMarkupDocumentViewer().defaultCharacterSet = arrayArgComponents[1];
      }
    }
  }

  //initConsoleListener();

  // XXXjag work-around for bug 113076
  // there's another bug where we throw an exception when getting
  // sessionHistory if it is null, which I'm exploiting here to
  // detect the situation described in bug 113076.
  // The same problem caused bug 139522, also worked around below.
  try {
    gBrowser.sessionHistory;
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
    gBrowser.docShell.QueryInterface(Components.interfaces.nsIDocShellHistory).globalHistory = globalHistory;

    const selectedBrowser = gBrowser.selectedBrowser;
    if (selectedBrowser.securityUI)
      selectedBrowser.securityUI.init(selectedBrowser.contentWindow);
  }

  // hook up UI through progress listener
  gBrowser.addProgressListener(window.XULBrowserWindow, Components.interfaces.nsIWebProgress.NOTIFY_ALL);

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
      if (gURLBar)
        gURLBar.value = uriToLoad;
      if ("arguments" in window && window.arguments.length >= 3) {
        loadURI(uriToLoad, window.arguments[2]);
      } else {
        loadURI(uriToLoad);
      }
    }

    // hook up remote support
    if (XREMOTESERVICE_CONTRACTID in Components.classes) {
      var remoteService;
      remoteService = Components.classes[XREMOTESERVICE_CONTRACTID]
                                .getService(Components.interfaces.nsIXRemoteService);
      remoteService.addBrowserInstance(window);

      RegisterTabOpenObserver();
    }
  }
  
  if (window.opener) {
    var openerSidebarBox = window.opener.document.getElementById("sidebar-box");
    if (!openerSidebarBox.hidden) {
      gMustLoadSidebar = true;
      var sidebarBox = document.getElementById("sidebar-box");
      sidebarBox.hidden = false;
      var sidebarSplitter = document.getElementById("sidebar-splitter");
      sidebarSplitter.hidden = false;

      var sidebarTitle = document.getElementById("sidebar-title");
      sidebarTitle.setAttribute("value", window.opener.document.getElementById("sidebar-title").getAttribute("value"));
      sidebarBox.setAttribute("width", openerSidebarBox.boxObject.width);
      var sidebarCmd = openerSidebarBox.getAttribute("sidebarcommand");
      document.getElementById(sidebarCmd).setAttribute("checked", "true");
      sidebarBox.setAttribute("sidebarcommand", sidebarCmd);
    }
  }
  
  // called when we go into full screen, even if it is 
  // initiated by a web page script
  addEventListener("fullscreen", onFullScreen, false);

  // does clicking on the urlbar select its contents?
  if (navigator.platform.indexOf("Win") == -1)
    gClickSelectsAll = false;

  // Focus the content area unless we're loading a blank page
  var elt;
  if (uriToLoad == "about:blank" && !toolbar.hidden &&
      gURLBar && !gURLBar.parentNode.parentNode.collapsed)
  {
    elt = gURLBar;
  } else {
    elt = _content;
  }
  
  setTimeout(delayedStartup, 0, elt);
}

function delayedStartup(aElt)
{
  if (gMustLoadSidebar) {
    var sidebar = document.getElementById("sidebar");
    sidebar.setAttribute("src", window.opener.document.getElementById("sidebar").getAttribute("src"));
  }
  
  // Perform default browser checking (after window opens).
  checkForDefaultBrowser();

  // now load bookmarks after a delay
  BMSVC.ReadBookmarks();
  var bt = document.getElementById("bookmarks-toolbar");
  if ("toolbar" in bt)
    bt.toolbar.builder.rebuild();       

  WindowFocusTimerCallback(aElt);

  // Get the preferences service
  var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefService);
  pref = prefService.getBranch(null);

  updateHomeTooltip();
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
    gBrowser.removeProgressListener(window.XULBrowserWindow);
  } catch (ex) {
  }

  window.XULBrowserWindow.destroy();
  window.XULBrowserWindow = null;

  BrowserFlushBookmarksAndHistory();

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

function constructGoMenuItem(goMenu, beforeItem, url, title)
{
  const kXULNS = 
    "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

  var menuitem = document.createElementNS(kXULNS, "menuitem");
  menuitem.setAttribute("url", url);
  menuitem.setAttribute("label", title);
  goMenu.insertBefore(menuitem, beforeItem);
  return menuitem;
}

function onGoMenuHidden()
{
  setTimeout("destroyGoMenuItems(document.getElementById('goPopup'));", 0);
}

function destroyGoMenuItems(goMenu) {
  var startSeparator = document.getElementById("startHistorySeparator");
  var endSeparator = document.getElementById("endHistorySeparator");
  endSeparator.hidden = true;

  // Destroy the items.
  var destroy = false;
  for (var i = 0; i < goMenu.childNodes.length; i++) {
    var item = goMenu.childNodes[i];
    if (item == endSeparator)
      break;

    if (destroy) {
      i--;
      goMenu.removeChild(item);
    }

    if (item == startSeparator)
      destroy = true;
  }
}

function updateGoMenu(goMenu)
{
  // In case the timer didn't fire.
  destroyGoMenuItems(goMenu);

  var history = document.getElementById("hiddenHistoryTree");
  
  if (history.hidden) {
    history.hidden = false;
    var globalHistory = Components.classes["@mozilla.org/browser/global-history;1"]
                                  .getService(Components.interfaces.nsIGlobalHistory);
    var dataSource = globalHistory.QueryInterface(Components.interfaces.nsIRDFDataSource);
    history.database.AddDataSource(dataSource);
  }

  if (!history.ref)
    history.ref = "NC:HistoryRoot";
  
  var count = history.treeBoxObject.view.rowCount;
  if (count > 10)
    count = 10;

  if (count == 0)
    return;

  const NC_NS     = "http://home.netscape.com/NC-rdf#";

  if (!gRDF)
     gRDF = Components.classes["@mozilla.org/rdf/rdf-service;1"]
                      .getService(Components.interfaces.nsIRDFService);

  var builder = history.builder.QueryInterface(Components.interfaces.nsIXULTreeBuilder);
  
  var beforeItem = document.getElementById("endHistorySeparator");
  beforeItem.hidden = false;

  var nameResource = gRDF.GetResource(NC_NS + "Name");

  for (var i = count-1; i >= 0; i--) {
    var res = builder.getResourceAtIndex(i);
    var url = res.Value;
    var titleRes = history.database.GetTarget(res, nameResource, true);
    var titleLiteral = titleRes.QueryInterface(Components.interfaces.nsIRDFLiteral);
    beforeItem = constructGoMenuItem(goMenu, beforeItem, url, titleLiteral.Value);
  }
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

function openLocation()
{
  if (gURLBar && !gURLBar.parentNode.parentNode.collapsed) {
    gURLBar.focus();
    gURLBar.select();
  }
  else {
    openDialog("chrome://browser/content/openLocation.xul", "_blank", "chrome,modal,titlebar", window);
  }
}

function BrowserOpenTab()
{
  if (!gInPrintPreviewMode) {
    gBrowser.selectedTab = gBrowser.addTab('about:blank');
    if (gURLBar)
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

      open("chrome://browser/content/bookmarks/bookmarksManager.xul", "_blank",
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
    var shortcutURL = BMSVC.resolveKeyword(url);
    if (!shortcutURL) {
      // rjc: add support for string substitution with shortcuts (4/4/2000)
      //      (see bug # 29871 for details)
      var aOffset = url.indexOf(" ");
      if (aOffset > 0) {
        var cmd = url.substr(0, aOffset);
        var text = url.substr(aOffset+1);
        shortcutURL = BMSVC.resolveKeyword(cmd);
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

function BrowserViewSourceOfURL(url, charset, pageCookie)
{
  // try to open a view-source window while inheriting the charset (if any)
  openDialog("chrome://browser/content/viewSource.xul",
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


function URLBarFocusHandler(aEvent, aElt)
{
  if (gIgnoreFocus)
    gIgnoreFocus = false;
  else if (gClickSelectsAll)
    aElt.select();
}

function URLBarMouseDownHandler(aEvent, aElt)
{
  if (aElt.hasAttribute("focused")) {
    gIgnoreClick = true;
  } else {
    gIgnoreFocus = true;
    gIgnoreClick = false;
    aElt.setSelectionRange(0, 0);
  }
}

function URLBarClickHandler(aEvent, aElt)
{
  if (!gIgnoreClick && gClickSelectsAll && aElt.selectionStart == aElt.selectionEnd)
    aElt.select();
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

// If "ESC" is pressed in the url bar, we replace the urlbar's value with the url of the page
// and highlight it, unless it is about:blank, where we reset it to "".
function handleURLBarRevert()
{
  var url = getWebNavigation().currentURI.spec;
  var throbberElement = document.getElementById("navigator-throbber");

  var isScrolling = gURLBar.popupOpen;
  
  // don't revert to last valid url unless page is NOT loading
  // and user is NOT key-scrolling through autocomplete list
  if ((!throbberElement || !throbberElement.hasAttribute("busy")) && !isScrolling) {
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
  return !isScrolling; 
}

function handleURLBarCommand(aTriggeringEvent)
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
  if (gURLBar && gURLBar.value != gLastValidURLStr)
    SetPageProxyState("invalid", null);
}

function SetPageProxyState(aState, aURI)
{
  if (!gURLBar)
    return;

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

function SearchBarPopupShowing(aEvent)
{
  var searchBar = document.getElementById("search-bar");
  var searchMode = searchBar.searchMode;

  var popup = document.getElementById("SearchBarPopup");
  var node = popup.firstChild;
  while (node) {
    node.setAttribute("checked", node.id == searchMode);
    node = node.nextSibling;
  }

  var findItem = document.getElementById("miSearchModeFind");
  findItem.setAttribute("checked", !searchMode);
}

function SearchBarPopupCommand(aEvent)
{
  var searchBar = document.getElementById("search-bar");

  if (aEvent.target.id == "miSearchModeFind") {
    searchBar.removeAttribute("searchmode");
    searchBar.setAttribute("autocompletesearchparam", "__PhoenixFindInPage");
    pref.setCharPref("browser.search.defaultengine", "");

    // Clear out the search engine icon
    searchBar.firstChild.removeAttribute("src");
  } else {
    searchBar.setAttribute("searchmode", aEvent.target.id);
    searchBar.setAttribute("autocompletesearchparam", "q");
    pref.setCharPref("browser.search.defaultengine", aEvent.target.id);
  }
  
  searchBar.detachController();
  focusSearchBar();
}

function handleSearchBarCommand(aEvent)
{
  var searchBar = document.getElementById("search-bar");

  // Save the current value in the form history
  if (!gFormHistory)
    gFormHistory = Components.classes["@mozilla.org/satchel/form-history;1"]
                             .getService(Components.interfaces.nsIFormHistory);
  gFormHistory.addEntry(searchBar.getAttribute("autocompletesearchparam"), searchBar.value);

  if (searchBar.hasAttribute("searchmode")) {
    gURLBar.value = searchBar.searchValue;
    BrowserLoadURL(aEvent);
  } else {
    quickFindInPage(searchBar.value);
  }
}

function quickFindInPage(aValue)
{
  var focusedWindow = document.commandDispatcher.focusedWindow;
  if (!focusedWindow || focusedWindow == window)
    focusedWindow = window._content;
      
  var findInst = getBrowser().webBrowserFind;
  var findInFrames = findInst.QueryInterface(Components.interfaces.nsIWebBrowserFindInFrames);
  findInFrames.rootSearchFrame = _content;
  findInFrames.currentSearchFrame = focusedWindow;

  var findService = Components.classes["@mozilla.org/find/find_service;1"]
                          .getService(Components.interfaces.nsIFindService);
  findInst.searchString  = aValue;
  findInst.matchCase     = findService.matchCase;
  findInst.wrapFind      = true;
  findInst.entireWord    = findService.entireWord;
  findInst.findBackwards = false;

  findInst.findNext();
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
  var navToolbox = document.getElementById("navigator-toolbox");
  navToolbox.hidden = aHide;
  var theTabbrowser = document.getElementById("content"); 

  if (aHide)
  {
    // going into print preview mode
    //deal with tab browser
    gChromeState.hadTabStrip = theTabbrowser.getStripVisibility();
    theTabbrowser.setStripVisibilityTo(false);
  }
  else
  {
    // restoring normal mode (i.e., leaving print preview mode)
    //restore tab browser
    theTabbrowser.setStripVisibilityTo(gChromeState.hadTabStrip);
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
 *       browser.js with functionality that can be encapsulated into
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

function OpenSearch(tabName, forceDialogFlag, searchStr, newWindowFlag)
{
  //This function needs to be split up someday.

  var defaultSearchURL = null;
  var navigatorRegionBundle = document.getElementById("bundle_browser_region");
  var fallbackDefaultSearchURL = navigatorRegionBundle.getString("fallbackDefaultSearchURL");
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
}

var personalToolbarDNDObserver = {

  ////////////////////
  // Public methods //
  ////////////////////

  onDragStart: function (aEvent, aXferData, aDragAction)
    {
      var target = aEvent.originalTarget;

      // Prevent dragging from an invalid region
      if (!this.canDrop(aEvent))
        return;

      // Prevent dragging out of menupopups on non Win32 platforms. 
      // a) on Mac drag from menus is generally regarded as being satanic
      // b) on Linux, this causes an X-server crash, (bug 151336)
      // c) on Windows, there is no hang or crash associated with this, so we'll leave 
      // the functionality there. 
      if (navigator.platform != "Win32" && target.localName != "toolbarbutton")
        return;

      // a drag start is fired when leaving an open toolbarbutton(type=menu) 
      // (see bug 143031)
      if (this.isContainer(target) && 
          target.getAttribute("group") != "true") {
        if (this.isPlatformNotSupported) 
          return;
        if (!aEvent.shiftKey && !aEvent.altKey)
          return;
        // menus open on mouse down
        target.firstChild.hidePopup();
      }
      var bt = document.getElementById("bookmarks-toolbar");
      var selection  = bt.getBTSelection(target);
      aXferData.data = BookmarksUtils.getXferDataFromSelection(selection);
    },

  onDragOver: function(aEvent, aFlavour, aDragSession) 
  {
    var bt = document.getElementById("bookmarks-toolbar");
    var orientation = bt.getBTOrientation(aEvent)
    if (aDragSession.canDrop)
      this.onDragSetFeedBack(aEvent.originalTarget, orientation);
    if (orientation != this.mCurrentDropPosition) {
      // emulating onDragExit and onDragEnter events since the drop region
      // has changed on the target.
      this.onDragExit(aEvent, aDragSession);
      this.onDragEnter(aEvent, aDragSession);
    }
    if (this.isPlatformNotSupported)
      return;
    if (this.isTimerSupported)
      return;
    this.onDragOverCheckTimers();
  },

  onDragEnter: function (aEvent, aDragSession)
  {
    var target = aEvent.originalTarget;
    var bt = document.getElementById("bookmarks-toolbar");
    var orientation = bt.getBTOrientation(aEvent);
    if (target.localName == "menupopup" || target.localName == "hbox")
      target = target.parentNode;
    if (aDragSession.canDrop) {
      this.onDragSetFeedBack(target, orientation);
      this.onDragEnterSetTimer(target, aDragSession);
    }
    this.mCurrentDragOverTarget = target;
    this.mCurrentDropPosition   = orientation;
  },

  onDragExit: function (aEvent, aDragSession)
  {
    var target = aEvent.originalTarget;
    if (target.localName == "menupopup" || target.localName == "hbox")
      target = target.parentNode;
    this.onDragRemoveFeedBack(target);
    this.onDragExitSetTimer(target, aDragSession);
    this.mCurrentDragOverTarget = null;
    this.mCurrentDropPosition = null;
  },

  onDrop: function (aEvent, aXferData, aDragSession)
  {
    this.onDragRemoveFeedBack(aEvent.originalTarget);
    var bt          = document.getElementById("bookmarks-toolbar");
    var selection   = BookmarksUtils.getSelectionFromXferData(aDragSession);

    // if the personal toolbar does not exist, recreate it
    if (aEvent.originalTarget == "bookmarks-toolbar") {
      BookmarksUtils.recreatePersonalToolbarFolder(transactionSet);
      target = { parent: "NC:PersonalToolbarFolder", index: 1 };
    } else {
      var orientation = bt.getBTOrientation(aEvent);
      var target      = bt.getBTSelection(aEvent.originalTarget);
      target          = BookmarksUtils.getTargetFromSelection(target, orientation);
    }

    const kDSIID      = Components.interfaces.nsIDragService;
    const kCopyAction = kDSIID.DRAGDROP_ACTION_COPY + kDSIID.DRAGDROP_ACTION_LINK;
    if (aDragSession.dragAction & kCopyAction)
      BookmarksUtils.insertSelection("drag", selection, target, true);
    else
      BookmarksUtils.moveSelection("drag", selection, target);
  },

  canDrop: function (aEvent, aDragSession)
  {
    var target = aEvent.originalTarget;
    return target.id && target.localName != "menupopup" && target.localName != "toolbar" &&
           target.localName != "menuseparator" && target.localName != "toolbarseparator" &
           target.id != "NC:SystemBookmarksStaticRoot" &&
           target.id.substring(0,5) != "find:";
  },

  canHandleMultipleItems: true,

  getSupportedFlavours: function () 
  {
    var flavourSet = new FlavourSet();
    flavourSet.appendFlavour("moz/rdfitem");
    flavourSet.appendFlavour("text/x-moz-url");
    flavourSet.appendFlavour("application/x-moz-file", "nsIFile");
    flavourSet.appendFlavour("text/unicode");
    return flavourSet;
  }, 
  

  ////////////////////////////////////
  // Private methods and properties //
  ////////////////////////////////////

  springLoadedMenuDelay: 350, // milliseconds
  isPlatformNotSupported: navigator.platform.indexOf("Mac") != -1, // see bug 136524
  isTimerSupported: navigator.platform.indexOf("Win") == -1,

  mCurrentDragOverTarget: null,
  mCurrentDropPosition: null,
  loadTimer  : null,
  closeTimer : null,
  loadTarget : null,
  closeTarget: null,

  _observers : null,
  get mObservers ()
  {
    if (!this._observers) {
      this._observers = [
        document.getElementById("bookmarks-ptf"),
        document.getElementById("bookmarks-menu").parentNode
      ]
    }
    return this._observers;
  },

  getObserverForNode: function (aNode)
  {
    if (!aNode)
      return null;
    var node = aNode;
    var observer;
    do {
      for (var i=0; i < this.mObservers.length; i++) {
        observer = this.mObservers[i];
        if (observer == node)
          return observer;
      }
      node = node.parentNode;
    } while (node != document)
    return null;
  },

  onDragCloseMenu: function (aNode)
  {
    var children = aNode.childNodes;
    for (var i = 0; i < children.length; i++) {
      if (this.isContainer(children[i]) && 
          children[i].getAttribute("open") == "true") {
        this.onDragCloseMenu(children[i].lastChild);
        if (children[i] != this.mCurrentDragOverTarget || this.mCurrentDropPosition != BookmarksUtils.DROP_ON)
          children[i].lastChild.hidePopup();
      }
    } 
  },

  onDragCloseTarget: function ()
  {
    var currentObserver = this.getObserverForNode(this.mCurrentDragOverTarget);
    // close all the menus not hovered by the mouse
    for (var i=0; i < this.mObservers.length; i++) {
      if (currentObserver != this.mObservers[i])
        this.onDragCloseMenu(this.mObservers[i]);
      else
        this.onDragCloseMenu(this.mCurrentDragOverTarget.parentNode);
    }
  },

  onDragLoadTarget: function (aTarget) 
  {
    if (!this.mCurrentDragOverTarget)
      return;
    // Load the current menu
    if (this.mCurrentDropPosition == BookmarksUtils.DROP_ON && 
        this.isContainer(aTarget)             && 
        aTarget.getAttribute("group") != "true")
      aTarget.lastChild.showPopup(aTarget);
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

  onDragSetFeedBack: function (aTarget, aOrientation)
  {
   switch (aTarget.localName) {
      case "toolbarseparator":
      case "toolbarbutton":
        switch (aOrientation) {
          case BookmarksUtils.DROP_BEFORE: 
            aTarget.setAttribute("dragover-left", "true");
            break;
          case BookmarksUtils.DROP_AFTER:
            aTarget.setAttribute("dragover-right", "true");
            break;
          case BookmarksUtils.DROP_ON:
            aTarget.setAttribute("dragover-top"   , "true");
            aTarget.setAttribute("dragover-bottom", "true");
            aTarget.setAttribute("dragover-left"  , "true");
            aTarget.setAttribute("dragover-right" , "true");
            break;
        }
        break;
      case "menuseparator": 
      case "menu":
      case "menuitem":
        switch (aOrientation) {
          case BookmarksUtils.DROP_BEFORE: 
            aTarget.setAttribute("dragover-top", "true");
            break;
          case BookmarksUtils.DROP_AFTER:
            aTarget.setAttribute("dragover-bottom", "true");
            break;
          case BookmarksUtils.DROP_ON:
            break;
        }
        break;
      case "hbox"     : 
        aTarget.lastChild.setAttribute("dragover-right", "true");
        break;
      case "menupopup": 
      case "toolbar"  : break;
      case "bookmarks-toolbar": break; // no personal toolbar folder
      default: dump("No feedback for: "+aTarget.localName+"\n");
    }
  },

  onDragRemoveFeedBack: function (aTarget)
  { 
    if (aTarget.localName == "bookmarks-toolbar") {
      var ptf = document.getAnonymousElementByAttribute(aTarget, "anonid", "bookmarks-ptf");
      if (ptf.hasChildNodes())
        ptf.lastChild.removeAttribute("dragover-right");
    } else if (aTarget.localName == "hbox") {
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
    return  aTarget.localName == "menu" || (aTarget.localName == "toolbarbutton") &&
           (aTarget.getAttribute("container") == "true" || aTarget.getAttribute("group") == "true");
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

const nsIDOMWindowInternal = Components.interfaces.nsIDOMWindowInternal;
const nsIWindowMediator = Components.interfaces.nsIWindowMediator;
const nsIWindowDataSource = Components.interfaces.nsIWindowDataSource;

function toNavigator()
{
  if (!CycleWindow("navigator:browser"))
    OpenBrowserWindow();
}

function toJavaScriptConsole()
{
    toOpenWindowByType("global:console", "chrome://browser/content/console/console.xul");
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
  var url = handler.chromeUrlForTask;
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

function openAboutDialog()
{
  window.openDialog("chrome://browser/content/aboutDialog.xul", "About", "modal,centerscreen,chrome,resizable=no");
}

function BrowserCustomizeToolbar()
{
  // Disable tlhe menubar and toolbar context menu items
  var menubar = document.getElementById("main-menubar");
  for (var i = 0; i < menubar.childNodes.length; ++i)
    menubar.childNodes[i].setAttribute("disabled", true);
    
  var cmd = document.getElementById("cmd_CustomizeToolbars");
  cmd.setAttribute("disabled", "true");
  
  window.openDialog("chrome://global/content/customizeToolbar.xul", "CustomizeToolbar",
                    "chrome,all,dependent", document.getElementById("navigator-toolbox"));
}

function BrowserToolboxCustomizeDone(aToolboxChanged)
{
  // Update global UI elements that may have been added or removed
  if (aToolboxChanged) {
    gURLBar = document.getElementById("urlbar");
    gProxyButton = document.getElementById("page-proxy-button");
    gProxyFavIcon = document.getElementById("page-proxy-favicon");
    gProxyDeck = document.getElementById("page-proxy-deck");
    updateHomeTooltip();
    window.XULBrowserWindow.init();
  }

  // Update the urlbar
  var url = getWebNavigation().currentURI.spec;
  if (gURLBar) {
    gURLBar.value = url;
    var uri = Components.classes["@mozilla.org/network/standard-url;1"]
                        .createInstance(Components.interfaces.nsIURI);
    uri.spec = url;
    SetPageProxyState("valid", uri);
  }

  // Re-enable parts of the UI we disabled during the dialog
  var menubar = document.getElementById("main-menubar");
  for (var i = 0; i < menubar.childNodes.length; ++i)
    menubar.childNodes[i].setAttribute("disabled", false);
  var cmd = document.getElementById("cmd_CustomizeToolbars");
  cmd.removeAttribute("disabled");

  // XXX Shouldn't have to do this, but I do
  window.focus();
}

var FullScreen = 
{
  toggle: function()
  {
    // show/hide all menubars, toolbars, and statusbars (except the full screen toolbar)
    this.showXULChrome("menubar", window.fullScreen);
    this.showXULChrome("toolbar", window.fullScreen);
    this.showXULChrome("statusbar", window.fullScreen);
  },
  
  showXULChrome: function(aTag, aShow)
  {
    var XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    var els = document.getElementsByTagNameNS(XULNS, aTag);
    
    var i;
    for (i = 0; i < els.length; ++i) {
      // XXX don't interfere with previously collapsed toolbars
      if (els[i].getAttribute("fullscreentoolbar") == "true") {
        if (!aShow) {
          gToolbarMode = els[i].getAttribute("mode");
          gIconSize = els[i].getAttribute("iconsize");
          els[i].setAttribute("mode", "icons");
          els[i].setAttribute("iconsize", "small");
        }
        else {
          els[i].setAttribute("mode", gToolbarMode);
          els[i].setAttribute("iconsize", gIconSize);
        }
      } else {
        // use moz-collapsed so it doesn't persist hidden/collapsed,
        // so that new windows don't have missing toolbars
        if (aShow)
          els[i].removeAttribute("moz-collapsed");
        else
          els[i].setAttribute("moz-collapsed", "true");
      }
    }
    
    var controls = document.getElementsByAttribute("fullscreencontrol", "true");
    for (i = 0; i < controls.length; ++i)
      controls[i].hidden = aShow;
  },
};

const NS_ERROR_MODULE_NETWORK = 2152398848;
const NS_NET_STATUS_READ_FROM = NS_ERROR_MODULE_NETWORK + 8;
const NS_NET_STATUS_WROTE_TO  = NS_ERROR_MODULE_NETWORK + 9;

function nsBrowserStatusHandler()
{
  this.init();
}

nsBrowserStatusHandler.prototype =
{
  userTyped :
  {
    _value : false,
    browser : null,

    get value() {
      if (this.browser != getBrowser().mCurrentBrowser)
        this._value = false;
      
      return this._value;
    },

    set value(aValue) {
      if (this._value != aValue) {
        this._value = aValue;
        this.browser = aValue ? getBrowser().mCurrentBrowser : null;
      }

      return aValue;
    }
  },

  // Stored Status, Link and Loading values
  status : "",
  defaultStatus : "",
  jsStatus : "",
  jsDefaultStatus : "",
  overLink : "",
  startTime : 0,

  statusTimeoutInEffect : false,

  hideAboutBlank : true,

  QueryInterface : function(aIID)
  {
    if (aIID.equals(Components.interfaces.nsIWebProgressListener) ||
        aIID.equals(Components.interfaces.nsISupportsWeakReference) ||
        aIID.equals(Components.interfaces.nsIXULBrowserWindow) ||
        aIID.equals(Components.interfaces.nsISupports))
      return this;
    throw Components.results.NS_NOINTERFACE;
  },

  init : function()
  {
    this.urlBar          = document.getElementById("urlbar");
    this.throbberElement = document.getElementById("navigator-throbber");
    this.statusMeter     = document.getElementById("statusbar-icon");
    this.stopCommand     = document.getElementById("Browser:Stop");
    this.statusTextField = document.getElementById("statusbar-display");
    this.isImage         = document.getElementById("isImage");
    this.securityButton  = document.getElementById("security-button");

    // Initialize the security button's state and tooltip text
    const nsIWebProgressListener = Components.interfaces.nsIWebProgressListener;
    this.onSecurityChange(null, null, nsIWebProgressListener.STATE_IS_INSECURE);
  },

  destroy : function()
  {
    // XXXjag to avoid leaks :-/, see bug 60729
    this.urlBar          = null;
    this.throbberElement = null;
    this.statusMeter     = null;
    this.stopCommand     = null;
    this.statusTextField = null;
    this.isImage         = null;
    this.securityButton  = null;
    this.userTyped       = null;
  },

  setJSStatus : function(status)
  {
    this.jsStatus = status;
    this.updateStatusField();
  },

  setJSDefaultStatus : function(status)
  {
    this.jsDefaultStatus = status;
    this.updateStatusField();
  },

  setDefaultStatus : function(status)
  {
    this.defaultStatus = status;
    this.updateStatusField();
  },

  setOverLink : function(link, b)
  {
    this.overLink = link;
    this.updateStatusField();
    if (link)
      this.statusTextField.setAttribute('crop', 'center');
    else
      this.statusTextField.setAttribute('crop', 'end');
  },

  updateStatusField : function()
  {
    var text = this.overLink || this.status || this.jsStatus || this.jsDefaultStatus || this.defaultStatus;

    // check the current value so we don't trigger an attribute change
    // and cause needless (slow!) UI updates
    if (this.statusTextField.label != text)
      this.statusTextField.label = text;
  },

  mimeTypeIsTextBased : function(contentType)
  {
    return /^text\/|\+xml$/.test(contentType) ||
           contentType == "application/x-javascript" ||
           contentType == "application/xml" ||
           contentType == "mozilla.application/cached-xul";
  },

  onLinkIconAvailable : function(aHref) {
    if (gProxyFavIcon) {
      
      // XXXBlake pref.getBoolPref("browser.chrome.site_icons"))
      gProxyFavIcon.setAttribute("src", aHref);

      // update any bookmarks with new icon reference
      BMSVC.updateBookmarkIcon(this.urlBar.value, aHref);
    }
  },

  onProgressChange : function (aWebProgress, aRequest,
                               aCurSelfProgress, aMaxSelfProgress,
                               aCurTotalProgress, aMaxTotalProgress)
  {
    if (aMaxTotalProgress > 0) {
      // This is highly optimized.  Don't touch this code unless
      // you are intimately familiar with the cost of setting
      // attrs on XUL elements. -- hyatt
      var percentage = (aCurTotalProgress * 100) / aMaxTotalProgress;
      this.statusMeter.value = percentage;
    } 
  },

  onStateChange : function(aWebProgress, aRequest, aStateFlags, aStatus)
  {  
    const nsIWebProgressListener = Components.interfaces.nsIWebProgressListener;
    const nsIChannel = Components.interfaces.nsIChannel;
    var ctype;
    if (aStateFlags & nsIWebProgressListener.STATE_START) {
        // This (thanks to the filter) is a network start or the first
        // stray request (the first request outside of the document load),
        // initialize the throbber and his friends.
        
        // Call start document load listeners (only if this is a network load)
        if (aStateFlags & nsIWebProgressListener.STATE_IS_NETWORK &&
            aRequest && aWebProgress.DOMWindow == content)
          this.startDocumentLoad(aRequest);
  
        if (this.throbberElement) {
          // Turn the throbber on.
          this.throbberElement.setAttribute("busy", "true");
        }

        // XXX: This needs to be based on window activity...
        this.stopCommand.removeAttribute("disabled");
    }
    else if (aStateFlags & nsIWebProgressListener.STATE_STOP) {
      if (aStateFlags & nsIWebProgressListener.STATE_IS_NETWORK) {
        if (aRequest) {
          if (aWebProgress.DOMWindow == content)
            this.endDocumentLoad(aRequest, aStatus);
        }
      }

      // This (thanks to the filter) is a network stop or the last
      // request stop outside of loading the document, stop throbbers
      // and progress bars and such
      if (aRequest) {
        var msg = "";
        // Get the channel if the request is a channel
        var channel;
        try {
          channel = aRequest.QueryInterface(nsIChannel);
        }
        catch(e) { };
          if (channel) {
            var location = channel.URI.spec;
            if (location != "about:blank") {
              const kErrorBindingAborted = 2152398850;
              const kErrorNetTimeout = 2152398862;
              switch (aStatus) {
                case kErrorBindingAborted:
                  msg = gNavigatorBundle.getString("nv_stopped");
                  break;
                case kErrorNetTimeout:
                  msg = gNavigatorBundle.getString("nv_timeout");
                  break;
              }
            }
          }
          // If msg is false then we did not have an error (channel may have
          // been null, in the case of a stray image load).
          if (!msg) {
            msg = gNavigatorBundle.getString("nv_done");
          }
          this.status = "";
          this.setDefaultStatus(msg);
          if (channel) {
            try {
              ctype = channel.contentType;
              if (this.mimeTypeIsTextBased(ctype))
                this.isImage.removeAttribute('disabled');
              else
                this.isImage.setAttribute('disabled', 'true');
            }
            catch (e) {}
          }
        }

        // Turn the progress meter and throbber off.
        this.statusMeter.value = 0;  // be sure to clear the progress bar
        if (this.throbberElement)
          this.throbberElement.removeAttribute("busy");

        // XXX: These need to be based on window activity...
        // XXXjag: <command id="cmd_stop"/> ?
        this.stopCommand.setAttribute("disabled", "true");
    }
  },

  onLocationChange : function(aWebProgress, aRequest, aLocation)
  {
    this.setOverLink("", null);

    var location = aLocation.spec;

    if (this.hideAboutBlank) {
      this.hideAboutBlank = false;
      if (location == "about:blank")
        location = "";
    }

    // Disable menu entries for images, enable otherwise
    if (content.document && this.mimeTypeIsTextBased(content.document.contentType))
      this.isImage.removeAttribute('disabled');
    else
      this.isImage.setAttribute('disabled', 'true');

    // We should probably not do this if the value has changed since the user
    // searched
    // Update urlbar only if a new page was loaded on the primary content area
    // Do not update urlbar if there was a subframe navigation

    if (aWebProgress.DOMWindow == content) {
      //XXXBlake don't we have to reinit this.urlBar, etc.
      //         when the toolbar changes?
      if (this.urlBar && !this.userTyped.value) {
        // If the url has "wyciwyg://" as the protocol, strip it off.
        // Nobody wants to see it on the urlbar for dynamically generated
        // pages. 
        if (/^\s*wyciwyg:\/\/\d+\//.test(location))
          location = RegExp.rightContext;
        this.urlBar.value = location;
        // the above causes userTyped.value to become true, reset it
        this.userTyped.value = false;
      }

      SetPageProxyState("valid", aLocation);
    }
    UpdateBackForwardButtons();
  },

  onStatusChange : function(aWebProgress, aRequest, aStatus, aMessage)
  {
    this.status = aMessage;
    this.updateStatusField();
  },

  onSecurityChange : function(aWebProgress, aRequest, aState)
  {
    const wpl = Components.interfaces.nsIWebProgressListener;

    switch (aState) {
      case wpl.STATE_IS_SECURE | wpl.STATE_SECURE_HIGH:
        this.securityButton.setAttribute("level", "high");
        break;
      case wpl.STATE_IS_SECURE | wpl.STATE_SECURE_LOW:
        this.securityButton.setAttribute("level", "low");
        break;
      case wpl.STATE_IS_BROKEN:
        this.securityButton.setAttribute("level", "broken");
        break;
      case wpl.STATE_IS_INSECURE:
      default:
        this.securityButton.removeAttribute("level");
        break;
    }

    var securityUI = getBrowser().securityUI;
    if (securityUI)
      this.securityButton.setAttribute("tooltiptext", securityUI.tooltipText);
    else
      this.securityButton.removeAttribute("tooltiptext");
  },

  startDocumentLoad : function(aRequest)
  {
    // Reset so we can see if the user typed after the document load
    // starting and the location changing.
    this.userTyped.value = false;

    const nsIChannel = Components.interfaces.nsIChannel;
    var urlStr = aRequest.QueryInterface(nsIChannel).URI.spec;
    var observerService = Components.classes["@mozilla.org/observer-service;1"]
                                    .getService(Components.interfaces.nsIObserverService);
    try {
      observerService.notifyObservers(_content, "StartDocumentLoad", urlStr);
    } catch (e) {
    }
  },

  endDocumentLoad : function(aRequest, aStatus)
  {
    const nsIChannel = Components.interfaces.nsIChannel;
    var urlStr = aRequest.QueryInterface(nsIChannel).originalURI.spec;

    if (Components.isSuccessCode(aStatus))
      dump("Document "+urlStr+" loaded successfully\n"); // per QA request
    else {
      // per QA request
      var e = new Components.Exception("", aStatus);
      var name = e.name;
      dump("Error loading URL "+urlStr+" : "+
           Number(aStatus).toString(16));
      if (name)
           dump(" ("+name+")");
      dump('\n'); 
    }

    var observerService = Components.classes["@mozilla.org/observer-service;1"]
                                    .getService(Components.interfaces.nsIObserverService);

    var notification = Components.isSuccessCode(aStatus) ? "EndDocumentLoad" : "FailDocumentLoad";
    try {
      observerService.notifyObservers(_content, notification, urlStr);
    } catch (e) {
    }
  }
}

function onViewToolbarsPopupShowing(aEvent)
{
  var popup = aEvent.target;
  
  // Empty the menu
  for (var i = popup.childNodes.length-1; i >= 0; --i) {
    var deadItem = popup.childNodes[i];
    if (deadItem.hasAttribute("toolbarindex"))
      popup.removeChild(deadItem);
  }
  
  var firstMenuItem = popup.firstChild;
  
  var toolbox = document.getElementById("navigator-toolbox");
  for (var i = 0; i < toolbox.childNodes.length; ++i) {
    var toolbar = toolbox.childNodes[i];
    var toolbarName = toolbar.getAttribute("toolbarname");
    if (toolbarName) {
      var menuItem = document.createElement("menuitem");
      menuItem.setAttribute("toolbarindex", i);
      menuItem.setAttribute("type", "checkbox");
      menuItem.setAttribute("label", toolbarName);
      menuItem.setAttribute("accesskey", toolbar.getAttribute("accesskey"));
      menuItem.setAttribute("checked", toolbar.getAttribute("collapsed") != "true");
      popup.insertBefore(menuItem, firstMenuItem);        
      
      menuItem.addEventListener("command", onViewToolbarCommand, false);
    }
    toolbar = toolbar.nextSibling;
  }
}

function onViewToolbarCommand(aEvent)
{
  var toolbox = document.getElementById("navigator-toolbox");
  var index = aEvent.originalTarget.getAttribute("toolbarindex");
  var toolbar = toolbox.childNodes[index];
  
  toolbar.collapsed = aEvent.originalTarget.getAttribute("checked") != "true";
  document.persist(toolbar.id, "collapsed");
}

function displayPageInfo()
{
    window.openDialog("chrome://navigator/content/pageInfo.xul", "_blank",
                      "dialog=no", null, "securityTab");
}

function displayPageReport()
{
    window.openDialog("chrome://browser/content/pageReport.xul", "_blank",
                      "dialog=no");
}

const mediatorContractId = "@mozilla.org/appshell/window-mediator;1";
const nsIWebBrowserChrome = Components.interfaces.nsIWebBrowserChrome;

function nsBrowserContentListener(toplevelWindow, contentWindow)
{
    // this one is not as easy as you would hope.
    // need to convert toplevelWindow to an XPConnected object, instead
    // of a DOM-based object, to be able to QI() it to nsIXULWindow
    
    this.init(toplevelWindow, contentWindow);
}

/* implements nsIURIContentListener */

nsBrowserContentListener.prototype =
{
    init: function(toplevelWindow, contentWindow)
    {
        this.toplevelWindow = toplevelWindow;
        this.contentWindow = contentWindow;

        // hook up the whole parent chain thing
        var windowDocShell = this.convertWindowToDocShell(toplevelWindow);
        if (windowDocShell)
            windowDocshell.parentURIContentListener = this;
    
        var registerWindow = false;
        try {          
          var treeItem = contentWindow.docShell.QueryInterface(Components.interfaces.nsIDocShellTreeItem);
          var treeOwner = treeItem.treeOwner;
          var interfaceRequestor = treeOwner.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
          var webBrowserChrome = interfaceRequestor.getInterface(nsIWebBrowserChrome);
          if (webBrowserChrome)
          {
            var chromeFlags = webBrowserChrome.chromeFlags;
            var res = chromeFlags & nsIWebBrowserChrome.CHROME_ALL;
            var res2 = chromeFlags & nsIWebBrowserChrome.CHROME_DEFAULT;
            if ( res == nsIWebBrowserChrome.CHROME_ALL || res2 == nsIWebBrowserChrome.CHROME_DEFAULT)
            {             
              registerWindow = true;
            }
         }
       } catch (ex) {} 

        // register ourselves
       if (registerWindow)
       {
        var uriLoader = Components.classes["@mozilla.org/uriloader;1"].getService(Components.interfaces.nsIURILoader);
        uriLoader.registerContentListener(this);
       }
    },
    close: function()
    {
        this.contentWindow = null;
        var uriLoader = Components.classes["@mozilla.org/uriloader;1"].getService(Components.interfaces.nsIURILoader);

        uriLoader.unRegisterContentListener(this);
    },
    QueryInterface: function(iid)
    {
        if (iid.equals(Components.interfaces.nsIURIContentListener))
            return this;
        if (iid.equals(Components.interfaces.nsISupportsWeakReference))
            return this;
        throw Components.results.NS_NOINTERFACE;

    },
    onStartURIOpen: function(uri)
    {
        // ignore and don't abort
        return false;
    },

    doContent: function(contentType, isContentPreferred, request, contentHandler)
    {
        // forward the doContent to our content area webshell
        var docShell = this.contentWindow.docShell;
        var contentListener;
        try {
            contentListener =
                docShell.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                .getInterface(Components.interfaces.nsIURIContentListener);
        } catch (ex) {
            dump(ex);
        }
        
        if (!contentListener) return false;
        
        return contentListener.doContent(contentType, isContentPreferred, request, contentHandler);
        
    },

    isPreferred: function(contentType, desiredContentType)
    {
        // seems like we should be getting this from helper apps or something
        switch(contentType) {
            case "text/html":
            case "text/xul":
            case "text/rdf":
            case "text/xml":
            case "text/css":
            case "image/gif":
            case "image/jpeg":
            case "image/png":
            case "text/plain":
            case "application/http-index-format":
                return true;
        }
        return false;
    },
    canHandleContent: function(contentType, isContentPreferred, desiredContentType)
    {
        var docShell = this.contentWindow.docShell;
        var contentListener;
        try {
            contentListener =
                docShell.QueryInterface(Components.interfaces.nsIInterfaceRequestor).getInterface(Components.interfaces.nsIURIContentListener);
        } catch (ex) {
            dump(ex);
        }
        if (!contentListener) return false;
        
        return contentListener.canHandleContent(contentType, isContentPreferred, desiredContentType);
    },
    convertWindowToDocShell: function(win) {
        // don't know how to do this
        return null;
    },
    loadCookie: null,
    parentContentListener: null
}

function toggleSidebar(aCommandID) {
  var sidebarBox = document.getElementById("sidebar-box");
  if (!aCommandID)
    aCommandID = sidebarBox.getAttribute("sidebarcommand");

  var elt = document.getElementById(aCommandID);
  var sidebar = document.getElementById("sidebar");
  var sidebarTitle = document.getElementById("sidebar-title");
  var sidebarSplitter = document.getElementById("sidebar-splitter");

  if (elt.getAttribute("checked") == "true") {
    elt.removeAttribute("checked");
    sidebarBox.hidden = true;
    sidebarSplitter.hidden = true;
    return;
  }
  
  var elts = document.getElementsByAttribute("group", "sidebar");
  for (var i = 0; i < elts.length; ++i)
    elts[i].removeAttribute("checked");

  elt.setAttribute("checked", "true");;

  if (sidebarBox.hidden) {
    sidebarBox.hidden = false;
    sidebarSplitter.hidden = false;
  }
  
  var url = elt.getAttribute("sidebarurl");
  var title = elt.getAttribute("sidebartitle");
  if (!title)
    title = elt.getAttribute("label");
  sidebar.setAttribute("src", url);
  sidebarBox.setAttribute("sidebarcommand", elt.id);
  sidebarTitle.setAttribute("value", title);
}

function goPreferences(containerID, paneURL, itemID)
{
  var pref = Components.classes["@mozilla.org/preferences-service;1"]
                       .getService(Components.interfaces.nsIPrefBranch);
  //check for an existing pref window and focus it; it's not application modal
  const kWindowMediatorContractID = "@mozilla.org/appshell/window-mediator;1";
  const kWindowMediatorIID = Components.interfaces.nsIWindowMediator;
  const kWindowMediator = Components.classes[kWindowMediatorContractID].getService(kWindowMediatorIID);
  var lastPrefWindow = kWindowMediator.getMostRecentWindow("mozilla:preferences");
  if (lastPrefWindow)
    lastPrefWindow.focus();
  else {
    var features = "chrome,titlebar,resizable";
    openDialog("chrome://browser/content/pref/pref.xul","PrefWindow", 
               features, paneURL, containerID, itemID);
  }
}

function updateHomeTooltip()
{
  var homeButton = document.getElementById("home-button");
  if (homeButton) {
    var homePage = getHomePage();
    homeButton.setAttribute("tooltiptext", homePage);
  }
}

function focusSearchBar()
{
  var searchBar = document.getElementById("search-bar");
  if (searchBar) {
    searchBar.select();
    searchBar.focus();
  }
}
