/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Services = object with smart getters for common XPCOM services
Components.utils.import("resource://gre/modules/AppConstants.jsm");
Components.utils.import("resource://gre/modules/ContextualIdentityService.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/PrivateBrowsingUtils.jsm");
Components.utils.import("resource:///modules/RecentWindow.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ShellService",
                                  "resource:///modules/ShellService.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "aboutNewTabService",
                                   "@mozilla.org/browser/aboutnewtab-service;1",
                                   "nsIAboutNewTabService");

this.__defineGetter__("BROWSER_NEW_TAB_URL", () => {
  if (PrivateBrowsingUtils.isWindowPrivate(window) &&
      !PrivateBrowsingUtils.permanentPrivateBrowsing &&
      !aboutNewTabService.overridden) {
    return "about:privatebrowsing";
  }
  return aboutNewTabService.newTabURL;
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
  catch (er) {
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
      referrerPolicy: Components.interfaces.nsIHttpChannel.REFERRER_POLICY_DEFAULT,
      initiatingDoc: event ? event.target.ownerDocument : null,
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

  var metaKey = AppConstants.platform == "macosx" ? meta : ctrl;
  if (metaKey || (middle && middleUsesTabs))
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
 *   allowPinnedTabHostChange (boolean)
 *   allowPopups          (boolean)
 *   userContextId        (unsigned int)
 */
function openUILinkIn(url, where, aAllowThirdPartyFixup, aPostData, aReferrerURI) {
  var params;

  if (arguments.length == 3 && typeof arguments[2] == "object") {
    params = aAllowThirdPartyFixup;
  } else {
    params = {
      allowThirdPartyFixup: aAllowThirdPartyFixup,
      postData: aPostData,
      referrerURI: aReferrerURI,
      referrerPolicy: Components.interfaces.nsIHttpChannel.REFERRER_POLICY_DEFAULT,
    };
  }

  params.fromChrome = true;

  openLinkIn(url, where, params);
}

function openLinkIn(url, where, params) {
  if (!where || !url)
    return;
  const Cc = Components.classes;
  const Ci = Components.interfaces;

  var aFromChrome           = params.fromChrome;
  var aAllowThirdPartyFixup = params.allowThirdPartyFixup;
  var aPostData             = params.postData;
  var aCharset              = params.charset;
  var aReferrerURI          = params.referrerURI;
  var aReferrerPolicy       = ('referrerPolicy' in params ?
      params.referrerPolicy : Ci.nsIHttpChannel.REFERRER_POLICY_DEFAULT);
  var aRelatedToCurrent     = params.relatedToCurrent;
  var aAllowMixedContent    = params.allowMixedContent;
  var aInBackground         = params.inBackground;
  var aDisallowInheritPrincipal = params.disallowInheritPrincipal;
  var aInitiatingDoc        = params.initiatingDoc;
  var aIsPrivate            = params.private;
  var aSkipTabAnimation     = params.skipTabAnimation;
  var aAllowPinnedTabHostChange = !!params.allowPinnedTabHostChange;
  var aNoReferrer           = params.noReferrer;
  var aAllowPopups          = !!params.allowPopups;
  var aUserContextId        = params.userContextId;
  var aIndicateErrorPageLoad = params.indicateErrorPageLoad;
  var aPrincipal            = params.originPrincipal;
  var aForceAboutBlankViewerInCurrent =
      params.forceAboutBlankViewerInCurrent;

  // Establish a window in which we're running this code.
  var w = getTopWin();

  if ((where == "tab" || where == "tabshifted") &&
      w && !w.toolbar.visible) {
    w = getTopWin(true);
    aRelatedToCurrent = false;
  }

  // Can only do this after we're sure of what |w| will be the rest of this function.
  // Note that if |w| is null we might have no current browser (we'll open a new window).
  var aCurrentBrowser = params.currentBrowser || (w && w.gBrowser.selectedBrowser);

  if (where == "save") {
    // TODO(1073187): propagate referrerPolicy.

    // ContentClick.jsm passes isContentWindowPrivate for saveURL instead of passing a CPOW initiatingDoc
    if ("isContentWindowPrivate" in params) {
      saveURL(url, null, null, true, true, aNoReferrer ? null : aReferrerURI, null, params.isContentWindowPrivate);
    }
    else {
      if (!aInitiatingDoc) {
        Components.utils.reportError("openUILink/openLinkIn was called with " +
          "where == 'save' but without initiatingDoc.  See bug 814264.");
        return;
      }
      saveURL(url, null, null, true, true, aNoReferrer ? null : aReferrerURI, aInitiatingDoc);
    }
    return;
  }

  if (!w || where == "window") {
    // This propagates to window.arguments.
    var sa = Cc["@mozilla.org/array;1"].
             createInstance(Ci.nsIMutableArray);

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

    var referrerURISupports = null;
    if (aReferrerURI && !aNoReferrer) {
      referrerURISupports = Cc["@mozilla.org/supports-string;1"].
                            createInstance(Ci.nsISupportsString);
      referrerURISupports.data = aReferrerURI.spec;
    }

    var referrerPolicySupports = Cc["@mozilla.org/supports-PRUint32;1"].
                                 createInstance(Ci.nsISupportsPRUint32);
    referrerPolicySupports.data = aReferrerPolicy;

    var userContextIdSupports = Cc["@mozilla.org/supports-PRUint32;1"].
                                 createInstance(Ci.nsISupportsPRUint32);
    userContextIdSupports.data = aUserContextId;

    sa.appendElement(wuri, /* weak =*/ false);
    sa.appendElement(charset, /* weak =*/ false);
    sa.appendElement(referrerURISupports, /* weak =*/ false);
    sa.appendElement(aPostData, /* weak =*/ false);
    sa.appendElement(allowThirdPartyFixupSupports, /* weak =*/ false);
    sa.appendElement(referrerPolicySupports, /* weak =*/ false);
    sa.appendElement(userContextIdSupports, /* weak =*/ false);
    sa.appendElement(aPrincipal, /* weak =*/ false);

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

  // NB: we avoid using |w| here because in the 'popup window' case,
  // if we pass a currentBrowser param |w.gBrowser| might not be the
  // tabbrowser that contains |aCurrentBrowser|. We really only care
  // about the tab linked to |aCurrentBrowser|.
  let tab = aCurrentBrowser.ownerGlobal.gBrowser.getTabForBrowser(aCurrentBrowser);
  if (where == "current" && tab.pinned &&
      !aAllowPinnedTabHostChange) {
    try {
      // nsIURI.host can throw for non-nsStandardURL nsIURIs.
      if (!uriObj || (!uriObj.schemeIs("javascript") &&
                      aCurrentBrowser.currentURI.host != uriObj.host)) {
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

  let browserUsedForLoad = null;
  switch (where) {
  case "current":
    let flags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;

    if (aAllowThirdPartyFixup) {
      flags |= Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;
      flags |= Ci.nsIWebNavigation.LOAD_FLAGS_FIXUP_SCHEME_TYPOS;
    }

    // LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL isn't supported for javascript URIs,
    // i.e. it causes them not to load at all. Callers should strip
    // "javascript:" from pasted strings to protect users from malicious URIs
    // (see stripUnsafeProtocolOnPaste).
    if (aDisallowInheritPrincipal && !(uriObj && uriObj.schemeIs("javascript"))) {
      flags |= Ci.nsIWebNavigation.LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL;
    }

    if (aAllowPopups) {
      flags |= Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_POPUPS;
    }
    if (aIndicateErrorPageLoad) {
      flags |= Ci.nsIWebNavigation.LOAD_FLAGS_ERROR_LOAD_CHANGES_RV;
    }

    if (aForceAboutBlankViewerInCurrent) {
      w.gBrowser.selectedBrowser.createAboutBlankContentViewer(aPrincipal);
    }

    aCurrentBrowser.loadURIWithFlags(url, {
      flags: flags,
      referrerURI: aNoReferrer ? null : aReferrerURI,
      referrerPolicy: aReferrerPolicy,
      postData: aPostData,
      userContextId: aUserContextId
    });
    browserUsedForLoad = aCurrentBrowser;
    break;
  case "tabshifted":
    loadInBackground = !loadInBackground;
    // fall through
  case "tab":
    let tabUsedForLoad = w.gBrowser.loadOneTab(url, {
      referrerURI: aReferrerURI,
      referrerPolicy: aReferrerPolicy,
      charset: aCharset,
      postData: aPostData,
      inBackground: loadInBackground,
      allowThirdPartyFixup: aAllowThirdPartyFixup,
      relatedToCurrent: aRelatedToCurrent,
      skipAnimation: aSkipTabAnimation,
      allowMixedContent: aAllowMixedContent,
      noReferrer: aNoReferrer,
      userContextId: aUserContextId,
      originPrincipal: aPrincipal,
    });
    browserUsedForLoad = tabUsedForLoad.linkedBrowser;
    break;
  }

  // Focus the content, but only if the browser used for the load is selected.
  if (browserUsedForLoad &&
      browserUsedForLoad == browserUsedForLoad.ownerGlobal.gBrowser.selectedBrowser) {
    browserUsedForLoad.focus();
  }

  if (!loadInBackground && w.isBlankPageURL(url)) {
    w.focusAndSelectUrlBar();
  }
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

// Populate a menu with user-context menu items. This method should be called
// by onpopupshowing passing the event as first argument.
function createUserContextMenu(event, isContextMenu = false, excludeUserContextId = 0) {
  while (event.target.hasChildNodes()) {
    event.target.removeChild(event.target.firstChild);
  }

  let bundle = document.getElementById("bundle_browser");
  let docfrag = document.createDocumentFragment();

  // If we are excluding a userContextId, we want to add a 'no-container' item.
  if (excludeUserContextId) {
    let menuitem = document.createElement("menuitem");
    menuitem.setAttribute("data-usercontextid", "0");
    menuitem.setAttribute("label", bundle.getString("userContextNone.label"));
    menuitem.setAttribute("accesskey", bundle.getString("userContextNone.accesskey"));

    // We don't set an oncommand/command attribute because if we have
    // to exclude a userContextId we are generating the contextMenu and
    // isContextMenu will be true.

    docfrag.appendChild(menuitem);

    let menuseparator = document.createElement("menuseparator");
    docfrag.appendChild(menuseparator);
  }

  ContextualIdentityService.getIdentities().forEach(identity => {
    if (identity.userContextId == excludeUserContextId) {
      return;
    }

    let menuitem = document.createElement("menuitem");
    menuitem.setAttribute("data-usercontextid", identity.userContextId);
    menuitem.setAttribute("label", ContextualIdentityService.getUserContextLabel(identity.userContextId));

    if (identity.accessKey) {
      menuitem.setAttribute("accesskey", bundle.getString(identity.accessKey));
    }

    menuitem.classList.add("menuitem-iconic");
    menuitem.setAttribute("data-identity-color", identity.color);

    if (!isContextMenu) {
      menuitem.setAttribute("command", "Browser:NewUserContextTab");
    }

    menuitem.setAttribute("data-identity-icon", identity.icon);

    docfrag.appendChild(menuitem);
  });

  if (!isContextMenu) {
    docfrag.appendChild(document.createElement("menuseparator"));

    let menuitem = document.createElement("menuitem");
    menuitem.setAttribute("label",
                          bundle.getString("userContext.aboutPage.label"));
    menuitem.setAttribute("accesskey",
                          bundle.getString("userContext.aboutPage.accesskey"));
    menuitem.setAttribute("command", "Browser:OpenAboutContainers");
    docfrag.appendChild(menuitem);
  }

  event.target.appendChild(docfrag);
  return true;
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

/** This function takes in a key element and compares it to the keys pressed during an event.
 *
 * @param aEvent
 *        The KeyboardEvent event you want to compare against your key.
 *
 * @param aKey
 *        The <key> element checked to see if it was called in aEvent.
 *        For example, aKey can be a variable set to document.getElementById("key_close")
 *        to check if the close command key was pressed in aEvent.
*/
function eventMatchesKey(aEvent, aKey)
{
  let keyPressed = aKey.getAttribute("key").toLowerCase();
  let keyModifiers = aKey.getAttribute("modifiers");
  let modifiers = ["Alt", "Control", "Meta", "Shift"];

  if (aEvent.key != keyPressed) {
    return false;
  }
  let eventModifiers = modifiers.filter(modifier => aEvent.getModifierState(modifier));
  // Check if aEvent has a modifier and aKey doesn't
  if (eventModifiers.length > 0 && keyModifiers.length == 0) {
     return false;
  }
  // Check whether aKey's modifiers match aEvent's modifiers
  if (keyModifiers) {
    keyModifiers = keyModifiers.split(/[\s,]+/);
    // Capitalize first letter of aKey's modifers to compare to aEvent's modifier
    keyModifiers.forEach(function(modifier, index) {
      if (modifier == "accel") {
        keyModifiers[index] = AppConstants.platform == "macosx" ?  "Meta" : "Control";
      } else {
        keyModifiers[index] = modifier[0].toUpperCase() + modifier.slice(1);
      }
    });
    return modifiers.every(modifier => keyModifiers.includes(modifier) == aEvent.getModifierState(modifier));
  }
  return true;
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

// This function exists for legacy reasons.
function getShellService()
{
  return ShellService;
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
    var systemLocale = localeService.getSystemLocale().getCategory("NSILOCALE_CTYPE").substr(0, 3);

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

  var features = "chrome,";
  if (AppConstants.platform == "win") {
    features += "centerscreen,dependent";
  } else if (AppConstants.platform == "macosx") {
    features += "resizable=no,minimizable=no";
  } else {
    features += "centerscreen,dependent,dialog=no";
  }

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

  let win = Services.wm.getMostRecentWindow("navigator:browser");
  let friendlyCategoryName = internalPrefCategoryNameToFriendlyName(paneID);
  let params;
  if (extraArgs && extraArgs["urlParams"]) {
    params = new URLSearchParams();
    let urlParams = extraArgs["urlParams"];
    for (let name in urlParams) {
      if (urlParams[name] !== undefined) {
        params.set(name, urlParams[name]);
      }
    }
  }
  let preferencesURL = "about:preferences" + (params ? "?" + params : "") +
                       (friendlyCategoryName ? "#" + friendlyCategoryName : "");
  let newLoad = true;
  let browser = null;
  if (!win) {
    const Cc = Components.classes;
    const Ci = Components.interfaces;
    let windowArguments = Cc["@mozilla.org/array;1"]
                            .createInstance(Ci.nsIMutableArray);
    let supportsStringPrefURL = Cc["@mozilla.org/supports-string;1"]
                                  .createInstance(Ci.nsISupportsString);
    supportsStringPrefURL.data = preferencesURL;
    windowArguments.appendElement(supportsStringPrefURL, /* weak =*/ false);

    win = Services.ww.openWindow(null, Services.prefs.getCharPref("browser.chromeURL"),
                                 "_blank", "chrome,dialog=no,all", windowArguments);
  } else {
    let shouldReplaceFragment = friendlyCategoryName ? "whenComparingAndReplace" : "whenComparing";
    newLoad = !win.switchToTabHavingURI(preferencesURL, true, { ignoreFragment: shouldReplaceFragment, replaceQueryString: true });
    browser = win.gBrowser.selectedBrowser;
  }

  if (newLoad) {
    Services.obs.addObserver(function advancedPaneLoadedObs(prefWin, topic, data) {
      if (!browser) {
        browser = win.gBrowser.selectedBrowser;
      }
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

/**
 * Opens the troubleshooting information (about:support) page for this version
 * of the application.
 */
function openHealthReport()
{
  openUILinkIn("about:healthreport", "tab");
}

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
  if (typeof gSafeBrowsing != "undefined") {
    gSafeBrowsing.setReportPhishingMenu();
  }
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
 * @param [optional] aReferrerPolicy
 *        Referrer policy - Ci.nsIHttpChannel.REFERRER_POLICY_*.
 */
function openNewTabWith(aURL, aDocument, aPostData, aEvent,
                        aAllowThirdPartyFixup, aReferrer, aReferrerPolicy) {

  // As in openNewWindowWith(), we want to pass the charset of the
  // current document over to a new tab.
  let originCharset = null;
  if (document.documentElement.getAttribute("windowtype") == "navigator:browser")
    originCharset = gBrowser.selectedBrowser.characterSet;

  openLinkIn(aURL, aEvent && aEvent.shiftKey ? "tabshifted" : "tab",
             { charset: originCharset,
               postData: aPostData,
               allowThirdPartyFixup: aAllowThirdPartyFixup,
               referrerURI: aReferrer,
               referrerPolicy: aReferrerPolicy,
             });
}

/**
 * @param aDocument
 *        Note this parameter is ignored. See openNewTabWith()
 */
function openNewWindowWith(aURL, aDocument, aPostData, aAllowThirdPartyFixup,
                           aReferrer, aReferrerPolicy) {
  // Extract the current charset menu setting from the current document and
  // use it to initialize the new browser window...
  let originCharset = null;
  if (document.documentElement.getAttribute("windowtype") == "navigator:browser")
    originCharset = gBrowser.selectedBrowser.characterSet;

  openLinkIn(aURL, "window",
             { charset: originCharset,
               postData: aPostData,
               allowThirdPartyFixup: aAllowThirdPartyFixup,
               referrerURI: aReferrer,
               referrerPolicy: aReferrerPolicy,
             });
}

function getHelpLinkURL(aHelpTopic) {
  var url = Components.classes["@mozilla.org/toolkit/URLFormatterService;1"]
                      .getService(Components.interfaces.nsIURLFormatter)
                      .formatURLPref("app.support.baseURL");
  return url + aHelpTopic;
}

// aCalledFromModal is optional
function openHelpLink(aHelpTopic, aCalledFromModal, aWhere) {
  var url = getHelpLinkURL(aHelpTopic);
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

  // remove single trailing slash for http/https/ftp URLs
  let url = aURL.replace(/^((?:http|https|ftp):\/\/[^/]+)\/$/, "$1");

  // remove http://
  if (!url.startsWith("http://")) {
    return url;
  }
  let urlWithoutProtocol = url.substring(7);

  let flags = Services.uriFixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP |
              Services.uriFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS;
  let fixedUpURL, expectedURLSpec;
  try {
    fixedUpURL = Services.uriFixup.createFixupURI(urlWithoutProtocol, flags);
    expectedURLSpec = makeURI(aURL).spec;
  } catch (ex) {
    return url;
  }
  if (fixedUpURL.spec == expectedURLSpec) {
    return urlWithoutProtocol;
  }
  return url;
}
