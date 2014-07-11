# -*- indent-tabs-mode: nil; js-indent-level: 4 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Services = object with smart getters for common XPCOM services
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/PrivateBrowsingUtils.jsm");
Components.utils.import("resource:///modules/RecentWindow.jsm");

XPCOMUtils.defineLazyGetter(this, "BROWSER_NEW_TAB_URL", function () {
  const PREF = "browser.newtab.url";

  function getNewTabPageURL() {
    if (!Services.prefs.prefHasUserValue(PREF)) {
      if (PrivateBrowsingUtils.isWindowPrivate(window) &&
          !PrivateBrowsingUtils.permanentPrivateBrowsing)
        return "about:privatebrowsing";
    }
    let url = Services.prefs.getComplexValue(PREF, Ci.nsISupportsString).data;
    return url || "about:blank";
  }

  function update() {
    BROWSER_NEW_TAB_URL = getNewTabPageURL();
  }

  Services.prefs.addObserver(PREF, update, false);

  addEventListener("unload", function onUnload() {
    removeEventListener("unload", onUnload);
    Services.prefs.removeObserver(PREF, update);
  });

  return getNewTabPageURL();
});

var TAB_DROP_TYPE = "application/x-moz-tabbrowser-tab";

var gBidiUI = false;

/**
 * Determines whether the given url is considered a special URL for new tabs.
 */
function isBlankPageURL(aURL) {
  return aURL == "about:blank" || aURL == BROWSER_NEW_TAB_URL;
}

function getBrowserURL()
{
  return "chrome://browser/content/browser.xul";
}

function getTopWin(skipPopups) {
  // If this is called in a browser window, use that window regardless of
  // whether it's the frontmost window, since commands can be executed in
  // background windows (bug 626148).
  if (top.document.documentElement.getAttribute("windowtype") == "navigator:browser" &&
      (!skipPopups || top.toolbar.visible))
    return top;

  let isPrivate = PrivateBrowsingUtils.isWindowPrivate(window);
  return RecentWindow.getMostRecentBrowserWindow({private: isPrivate,
                                                  allowPopups: !skipPopups});
}

function openTopWin(url) {
  /* deprecated */
  openUILinkIn(url, "current");
}

function getBoolPref(prefname, def)
{
  try {
    return Services.prefs.getBoolPref(prefname);
  }
  catch(er) {
    return def;
  }
}

/* openUILink handles clicks on UI elements that cause URLs to load.
 *
 * As the third argument, you may pass an object with the same properties as
 * accepted by openUILinkIn, plus "ignoreButton" and "ignoreAlt".
 */
function openUILink(url, event, aIgnoreButton, aIgnoreAlt, aAllowThirdPartyFixup,
                    aPostData, aReferrerURI) {
  let params;

  if (aIgnoreButton && typeof aIgnoreButton == "object") {
    params = aIgnoreButton;

    // don't forward "ignoreButton" and "ignoreAlt" to openUILinkIn
    aIgnoreButton = params.ignoreButton;
    aIgnoreAlt = params.ignoreAlt;
    delete params.ignoreButton;
    delete params.ignoreAlt;
  } else {
    params = {
      allowThirdPartyFixup: aAllowThirdPartyFixup,
      postData: aPostData,
      referrerURI: aReferrerURI,
      initiatingDoc: event ? event.target.ownerDocument : null
    };
  }

  let where = whereToOpenLink(event, aIgnoreButton, aIgnoreAlt);
  openUILinkIn(url, where, params);
}


/* whereToOpenLink() looks at an event to decide where to open a link.
 *
 * The event may be a mouse event (click, double-click, middle-click) or keypress event (enter).
 *
 * On Windows, the modifiers are:
 * Ctrl        new tab, selected
 * Shift       new window
 * Ctrl+Shift  new tab, in background
 * Alt         save
 *
 * Middle-clicking is the same as Ctrl+clicking (it opens a new tab).
 *
 * Exceptions: 
 * - Alt is ignored for menu items selected using the keyboard so you don't accidentally save stuff.  
 *    (Currently, the Alt isn't sent here at all for menu items, but that will change in bug 126189.)
 * - Alt is hard to use in context menus, because pressing Alt closes the menu.
 * - Alt can't be used on the bookmarks toolbar because Alt is used for "treat this as something draggable".
 * - The button is ignored for the middle-click-paste-URL feature, since it's always a middle-click.
 */
function whereToOpenLink( e, ignoreButton, ignoreAlt )
{
  // This method must treat a null event like a left click without modifier keys (i.e.
  // e = { shiftKey:false, ctrlKey:false, metaKey:false, altKey:false, button:0 })
  // for compatibility purposes.
  if (!e)
    return "current";

  var shift = e.shiftKey;
  var ctrl =  e.ctrlKey;
  var meta =  e.metaKey;
  var alt  =  e.altKey && !ignoreAlt;

  // ignoreButton allows "middle-click paste" to use function without always opening in a new window.
  var middle = !ignoreButton && e.button == 1;
  var middleUsesTabs = getBoolPref("browser.tabs.opentabfor.middleclick", true);

  // Don't do anything special with right-mouse clicks.  They're probably clicks on context menu items.

#ifdef XP_MACOSX
  if (meta || (middle && middleUsesTabs))
#else
  if (ctrl || (middle && middleUsesTabs))
#endif
    return shift ? "tabshifted" : "tab";

  if (alt && getBoolPref("browser.altClickSave", false))
    return "save";

  if (shift || (middle && !middleUsesTabs))
    return "window";

  return "current";
}

/* openUILinkIn opens a URL in a place specified by the parameter |where|.
 *
 * |where| can be:
 *  "current"     current tab            (if there aren't any browser windows, then in a new window instead)
 *  "tab"         new tab                (if there aren't any browser windows, then in a new window instead)
 *  "tabshifted"  same as "tab" but in background if default is to select new tabs, and vice versa
 *  "window"      new window
 *  "save"        save to disk (with no filename hint!)
 *
 * aAllowThirdPartyFixup controls whether third party services such as Google's
 * I Feel Lucky are allowed to interpret this URL. This parameter may be
 * undefined, which is treated as false.
 *
 * Instead of aAllowThirdPartyFixup, you may also pass an object with any of
 * these properties:
 *   allowThirdPartyFixup (boolean)
 *   postData             (nsIInputStream)
 *   referrerURI          (nsIURI)
 *   relatedToCurrent     (boolean)
 *   skipTabAnimation     (boolean)
 */
function openUILinkIn(url, where, aAllowThirdPartyFixup, aPostData, aReferrerURI) {
  var params;

  if (arguments.length == 3 && typeof arguments[2] == "object") {
    params = aAllowThirdPartyFixup;
  } else {
    params = {
      allowThirdPartyFixup: aAllowThirdPartyFixup,
      postData: aPostData,
      referrerURI: aReferrerURI
    };
  }

  params.fromChrome = true;

  openLinkIn(url, where, params);
}

function openLinkIn(url, where, params) {
  if (!where || !url)
    return;

  var aFromChrome           = params.fromChrome;
  var aAllowThirdPartyFixup = params.allowThirdPartyFixup;
  var aPostData             = params.postData;
  var aCharset              = params.charset;
  var aReferrerURI          = params.referrerURI;
  var aRelatedToCurrent     = params.relatedToCurrent;
  var aDisableMCB           = params.disableMCB;
  var aInBackground         = params.inBackground;
  var aDisallowInheritPrincipal = params.disallowInheritPrincipal;
  var aInitiatingDoc        = params.initiatingDoc;
  var aIsPrivate            = params.private;
  var aSkipTabAnimation     = params.skipTabAnimation;

  if (where == "save") {
    if (!aInitiatingDoc) {
      Components.utils.reportError("openUILink/openLinkIn was called with " +
        "where == 'save' but without initiatingDoc.  See bug 814264.");
      return;
    }
    saveURL(url, null, null, true, null, aReferrerURI, aInitiatingDoc);
    return;
  }
  const Cc = Components.classes;
  const Ci = Components.interfaces;

  var w = getTopWin();
  if ((where == "tab" || where == "tabshifted") &&
      w && !w.toolbar.visible) {
    w = getTopWin(true);
    aRelatedToCurrent = false;
  }

  if (!w || where == "window") {
    var sa = Cc["@mozilla.org/supports-array;1"].
             createInstance(Ci.nsISupportsArray);

    var wuri = Cc["@mozilla.org/supports-string;1"].
               createInstance(Ci.nsISupportsString);
    wuri.data = url;

    let charset = null;
    if (aCharset) {
      charset = Cc["@mozilla.org/supports-string;1"]
                  .createInstance(Ci.nsISupportsString);
      charset.data = "charset=" + aCharset;
    }

    var allowThirdPartyFixupSupports = Cc["@mozilla.org/supports-PRBool;1"].
                                       createInstance(Ci.nsISupportsPRBool);
    allowThirdPartyFixupSupports.data = aAllowThirdPartyFixup;

    sa.AppendElement(wuri);
    sa.AppendElement(charset);
    sa.AppendElement(aReferrerURI);
    sa.AppendElement(aPostData);
    sa.AppendElement(allowThirdPartyFixupSupports);

    let features = "chrome,dialog=no,all";
    if (aIsPrivate) {
      features += ",private";
    }

    Services.ww.openWindow(w || window, getBrowserURL(), null, features, sa);
    return;
  }

  let loadInBackground = where == "current" ? false : aInBackground;
  if (loadInBackground == null) {
    loadInBackground = aFromChrome ?
                         false :
                         getBoolPref("browser.tabs.loadInBackground");
  }

  let uriObj;
  if (where == "current") {
    try {
      uriObj = Services.io.newURI(url, null, null);
    } catch (e) {}
  }

  if (where == "current" && w.gBrowser.selectedTab.pinned) {
    try {
      // nsIURI.host can throw for non-nsStandardURL nsIURIs.
      if (!uriObj || (!uriObj.schemeIs("javascript") &&
                      w.gBrowser.currentURI.host != uriObj.host)) {
        where = "tab";
        loadInBackground = false;
      }
    } catch (err) {
      where = "tab";
      loadInBackground = false;
    }
  }

  // Raise the target window before loading the URI, since loading it may
  // result in a new frontmost window (e.g. "javascript:window.open('');").
  w.focus();

  switch (where) {
  case "current":
    let flags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;

    if (aAllowThirdPartyFixup) {
      flags |= Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;
      flags |= Ci.nsIWebNavigation.LOAD_FLAGS_FIXUP_SCHEME_TYPOS;
    }

    // LOAD_FLAGS_DISALLOW_INHERIT_OWNER isn't supported for javascript URIs,
    // i.e. it causes them not to load at all. Callers should strip
    // "javascript:" from pasted strings to protect users from malicious URIs
    // (see stripUnsafeProtocolOnPaste).
    if (aDisallowInheritPrincipal && !(uriObj && uriObj.schemeIs("javascript")))
      flags |= Ci.nsIWebNavigation.LOAD_FLAGS_DISALLOW_INHERIT_OWNER;

    w.gBrowser.loadURIWithFlags(url, flags, aReferrerURI, null, aPostData);
    break;
  case "tabshifted":
    loadInBackground = !loadInBackground;
    // fall through
  case "tab":
    let browser = w.gBrowser;
    browser.loadOneTab(url, {
                       referrerURI: aReferrerURI,
                       charset: aCharset,
                       postData: aPostData,
                       inBackground: loadInBackground,
                       allowThirdPartyFixup: aAllowThirdPartyFixup,
                       relatedToCurrent: aRelatedToCurrent,
                       skipAnimation: aSkipTabAnimation,
                       disableMCB: aDisableMCB});
    break;
  }

  w.gBrowser.selectedBrowser.focus();

  if (!loadInBackground && w.isBlankPageURL(url))
    w.focusAndSelectUrlBar();
}

// Used as an onclick handler for UI elements with link-like behavior.
// e.g. onclick="checkForMiddleClick(this, event);"
function checkForMiddleClick(node, event) {
  // We should be using the disabled property here instead of the attribute,
  // but some elements that this function is used with don't support it (e.g.
  // menuitem).
  if (node.getAttribute("disabled") == "true")
    return; // Do nothing

  if (event.button == 1) {
    /* Execute the node's oncommand or command.
     *
     * XXX: we should use node.oncommand(event) once bug 246720 is fixed.
     */
    var target = node.hasAttribute("oncommand") ? node :
                 node.ownerDocument.getElementById(node.getAttribute("command"));
    var fn = new Function("event", target.getAttribute("oncommand"));
    fn.call(target, event);

    // If the middle-click was on part of a menu, close the menu.
    // (Menus close automatically with left-click but not with middle-click.)
    closeMenus(event.target);
  }
}

// Closes all popups that are ancestors of the node.
function closeMenus(node)
{
  if ("tagName" in node) {
    if (node.namespaceURI == "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
    && (node.tagName == "menupopup" || node.tagName == "popup"))
      node.hidePopup();

    closeMenus(node.parentNode);
  }
}

// Gather all descendent text under given document node.
function gatherTextUnder ( root ) 
{
  var text = "";
  var node = root.firstChild;
  var depth = 1;
  while ( node && depth > 0 ) {
    // See if this node is text.
    if ( node.nodeType == Node.TEXT_NODE ) {
      // Add this text to our collection.
      text += " " + node.data;
    } else if ( node instanceof HTMLImageElement) {
      // If it has an "alt" attribute, add that.
      var altText = node.getAttribute( "alt" );
      if ( altText && altText != "" ) {
        text += " " + altText;
      }
    }
    // Find next node to test.
    // First, see if this node has children.
    if ( node.hasChildNodes() ) {
      // Go to first child.
      node = node.firstChild;
      depth++;
    } else {
      // No children, try next sibling (or parent next sibling).
      while ( depth > 0 && !node.nextSibling ) {
        node = node.parentNode;
        depth--;
      }
      if ( node.nextSibling ) {
        node = node.nextSibling;
      }
    }
  }
  // Strip leading and tailing whitespace.
  text = text.trim();
  // Compress remaining whitespace.
  text = text.replace( /\s+/g, " " );
  return text;
}

function getShellService()
{
  var shell = null;
  try {
    shell = Components.classes["@mozilla.org/browser/shell-service;1"]
      .getService(Components.interfaces.nsIShellService);
  } catch (e) {
  }
  return shell;
}

function isBidiEnabled() {
  // first check the pref.
  if (getBoolPref("bidi.browser.ui", false))
    return true;

  // then check intl.uidirection.<locale>
  var chromeReg = Components.classes["@mozilla.org/chrome/chrome-registry;1"].
                  getService(Components.interfaces.nsIXULChromeRegistry);
  if (chromeReg.isLocaleRTL("global"))
    return true;

  // now see if the system locale is an RTL one.
  var rv = false;

  try {
    var localeService = Components.classes["@mozilla.org/intl/nslocaleservice;1"]
                                  .getService(Components.interfaces.nsILocaleService);
    var systemLocale = localeService.getSystemLocale().getCategory("NSILOCALE_CTYPE").substr(0,3);

    switch (systemLocale) {
      case "ar-":
      case "he-":
      case "fa-":
      case "ug-":
      case "ur-":
      case "syr":
        rv = true;
        Services.prefs.setBoolPref("bidi.browser.ui", true);
    }
  } catch (e) {}

  return rv;
}

function openAboutDialog() {
  var enumerator = Services.wm.getEnumerator("Browser:About");
  while (enumerator.hasMoreElements()) {
    // Only open one about window (Bug 599573)
    let win = enumerator.getNext();
    if (win.closed) {
      continue;
    }
    win.focus();
    return;
  }

#ifdef XP_WIN
  var features = "chrome,centerscreen,dependent";
#elifdef XP_MACOSX
  var features = "chrome,resizable=no,minimizable=no";
#else
  var features = "chrome,centerscreen,dependent,dialog=no";
#endif
  window.openDialog("chrome://browser/content/aboutDialog.xul", "", features);
}

function openPreferences(paneID, extraArgs)
{
  function switchToAdvancedSubPane(doc) {
    if (extraArgs && extraArgs["advancedTab"]) {
      let advancedPaneTabs = doc.getElementById("advancedPrefs");
      advancedPaneTabs.selectedTab = doc.getElementById(extraArgs["advancedTab"]);
    }
  }

  // This function is duplicated from preferences.js.
  function internalPrefCategoryNameToFriendlyName(aName) {
    return (aName || "").replace(/^pane./, function(toReplace) { return toReplace[4].toLowerCase(); });
  }

  if (getBoolPref("browser.preferences.inContent")) {
    let win = Services.wm.getMostRecentWindow("navigator:browser");
    if (!win) {
      return;
    }

    let friendlyCategoryName = internalPrefCategoryNameToFriendlyName(paneID);
    let preferencesURL = "about:preferences" +
                         (friendlyCategoryName ? "#" + friendlyCategoryName : "");
    let newLoad = !win.switchToTabHavingURI(preferencesURL, true, {ignoreFragment: true});
    let browser = win.gBrowser.selectedBrowser;

    if (newLoad) {
      Services.obs.addObserver(function advancedPaneLoadedObs(prefWin, topic, data) {
        if (prefWin != browser.contentWindow) {
          return;
        }
        Services.obs.removeObserver(advancedPaneLoadedObs, "advanced-pane-loaded");
        switchToAdvancedSubPane(browser.contentDocument);
      }, "advanced-pane-loaded", false);
    } else {
      if (paneID) {
        browser.contentWindow.gotoPref(paneID);
      }
      switchToAdvancedSubPane(browser.contentDocument);
    }
  } else {
    var instantApply = getBoolPref("browser.preferences.instantApply", false);
    var features = "chrome,titlebar,toolbar,centerscreen" + (instantApply ? ",dialog=no" : ",modal");

    var win = Services.wm.getMostRecentWindow("Browser:Preferences");
    if (win) {
      win.focus();
      if (paneID) {
        var pane = win.document.getElementById(paneID);
        win.document.documentElement.showPane(pane);
      }

      switchToAdvancedSubPane(win.document);
    } else {
      openDialog("chrome://browser/content/preferences/preferences.xul",
                 "Preferences", features, paneID, extraArgs);
    }
  }
}

function openAdvancedPreferences(tabID)
{
  openPreferences("paneAdvanced", { "advancedTab" : tabID });
}

/**
 * Opens the troubleshooting information (about:support) page for this version
 * of the application.
 */
function openTroubleshootingPage()
{
  openUILinkIn("about:support", "tab");
}

#ifdef MOZ_SERVICES_HEALTHREPORT
/**
 * Opens the troubleshooting information (about:support) page for this version
 * of the application.
 */
function openHealthReport()
{
  openUILinkIn("about:healthreport", "tab");
}
#endif

/**
 * Opens the feedback page for this version of the application.
 */
function openFeedbackPage()
{
  var url = Components.classes["@mozilla.org/toolkit/URLFormatterService;1"]
                      .getService(Components.interfaces.nsIURLFormatter)
                      .formatURLPref("app.feedback.baseURL");
  openUILinkIn(url, "tab");
}

function openTourPage()
{
  let scope = {}
  Components.utils.import("resource:///modules/UITour.jsm", scope);
  openUILinkIn(scope.UITour.url, "tab");
}

function buildHelpMenu()
{
  // Enable/disable the "Report Web Forgery" menu item.
  if (typeof gSafeBrowsing != "undefined")
    gSafeBrowsing.setReportPhishingMenu();
}

function isElementVisible(aElement)
{
  if (!aElement)
    return false;

  // If aElement or a direct or indirect parent is hidden or collapsed,
  // height, width or both will be 0.
  var bo = aElement.boxObject;
  return (bo.height > 0 && bo.width > 0);
}

function makeURLAbsolute(aBase, aUrl)
{
  // Note:  makeURI() will throw if aUri is not a valid URI
  return makeURI(aUrl, null, makeURI(aBase)).spec;
}

/**
 * openNewTabWith: opens a new tab with the given URL.
 *
 * @param aURL
 *        The URL to open (as a string).
 * @param aDocument
 *        Note this parameter is now ignored. There is no security check & no
 *        referrer header derived from aDocument (null case).
 * @param aPostData
 *        Form POST data, or null.
 * @param aEvent
 *        The triggering event (for the purpose of determining whether to open
 *        in the background), or null.
 * @param aAllowThirdPartyFixup
 *        If true, then we allow the URL text to be sent to third party services
 *        (e.g., Google's I Feel Lucky) for interpretation. This parameter may
 *        be undefined in which case it is treated as false.
 * @param [optional] aReferrer
 *        This will be used as the referrer. There will be no security check.
 */ 
function openNewTabWith(aURL, aDocument, aPostData, aEvent,
                        aAllowThirdPartyFixup, aReferrer) {

  // As in openNewWindowWith(), we want to pass the charset of the
  // current document over to a new tab.
  let originCharset = null;
  if (document.documentElement.getAttribute("windowtype") == "navigator:browser")
    originCharset = gBrowser.selectedBrowser.characterSet;

  openLinkIn(aURL, aEvent && aEvent.shiftKey ? "tabshifted" : "tab",
             { charset: originCharset,
               postData: aPostData,
               allowThirdPartyFixup: aAllowThirdPartyFixup,
               referrerURI: aReferrer });
}

/**
 * @param aDocument
 *        Note this parameter is ignored. See openNewTabWith()
 */
function openNewWindowWith(aURL, aDocument, aPostData, aAllowThirdPartyFixup, aReferrer) {
  // Extract the current charset menu setting from the current document and
  // use it to initialize the new browser window...
  let originCharset = null;
  if (document.documentElement.getAttribute("windowtype") == "navigator:browser")
    originCharset = gBrowser.selectedBrowser.characterSet;

  openLinkIn(aURL, "window",
             { charset: originCharset,
               postData: aPostData,
               allowThirdPartyFixup: aAllowThirdPartyFixup,
               referrerURI: aReferrer });
}

// aCalledFromModal is optional
function openHelpLink(aHelpTopic, aCalledFromModal, aWhere) {
  var url = Components.classes["@mozilla.org/toolkit/URLFormatterService;1"]
                      .getService(Components.interfaces.nsIURLFormatter)
                      .formatURLPref("app.support.baseURL");
  url += aHelpTopic;

  var where = aWhere;
  if (!aWhere)
    where = aCalledFromModal ? "window" : "tab";

  openUILinkIn(url, where);
}

function openPrefsHelp() {
  // non-instant apply prefwindows are usually modal, so we can't open in the topmost window, 
  // since its probably behind the window.
  var instantApply = getBoolPref("browser.preferences.instantApply");

  var helpTopic = document.getElementsByTagName("prefwindow")[0].currentPane.helpTopic;
  openHelpLink(helpTopic, !instantApply);
}

function trimURL(aURL) {
  // This function must not modify the given URL such that calling
  // nsIURIFixup::createFixupURI with the result will produce a different URI.
  return aURL /* remove single trailing slash for http/https/ftp URLs */
             .replace(/^((?:http|https|ftp):\/\/[^/]+)\/$/, "$1")
              /* remove http:// unless the host starts with "ftp\d*\." or contains "@" */
             .replace(/^http:\/\/((?!ftp\d*\.)[^\/@]+(?:\/|$))/, "$1");
}
