/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Services = object with smart getters for common XPCOM services
var { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AboutNewTab: "resource:///modules/AboutNewTab.jsm",
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  ContextualIdentityService:
    "resource://gre/modules/ContextualIdentityService.jsm",
  ExtensionSettingsStore: "resource://gre/modules/ExtensionSettingsStore.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  ShellService: "resource:///modules/ShellService.jsm",
});

XPCOMUtils.defineLazyGetter(this, "ReferrerInfo", () =>
  Components.Constructor(
    "@mozilla.org/referrer-info;1",
    "nsIReferrerInfo",
    "init"
  )
);

Object.defineProperty(this, "BROWSER_NEW_TAB_URL", {
  enumerable: true,
  get() {
    if (PrivateBrowsingUtils.isWindowPrivate(window)) {
      if (
        !PrivateBrowsingUtils.permanentPrivateBrowsing &&
        !AboutNewTab.newTabURLOverridden
      ) {
        return "about:privatebrowsing";
      }
      // If an extension controls the setting and does not have private
      // browsing permission, use the default setting.
      let extensionControlled = Services.prefs.getBoolPref(
        "browser.newtab.extensionControlled",
        false
      );
      let privateAllowed = Services.prefs.getBoolPref(
        "browser.newtab.privateAllowed",
        false
      );
      // There is a potential on upgrade that the prefs are not set yet, so we double check
      // for moz-extension.
      if (
        !privateAllowed &&
        (extensionControlled ||
          AboutNewTab.newTabURL.startsWith("moz-extension://"))
      ) {
        return "about:privatebrowsing";
      }
    }
    return AboutNewTab.newTabURL;
  },
});

var TAB_DROP_TYPE = "application/x-moz-tabbrowser-tab";

var gBidiUI = false;

/**
 * Determines whether the given url is considered a special URL for new tabs.
 */
function isBlankPageURL(aURL) {
  return (
    aURL == "about:blank" ||
    aURL == "about:home" ||
    aURL == "about:welcome" ||
    aURL == BROWSER_NEW_TAB_URL
  );
}

function getTopWin(skipPopups) {
  // If this is called in a browser window, use that window regardless of
  // whether it's the frontmost window, since commands can be executed in
  // background windows (bug 626148).
  if (
    top.document.documentElement.getAttribute("windowtype") ==
      "navigator:browser" &&
    (!skipPopups || top.toolbar.visible)
  ) {
    return top;
  }

  return BrowserWindowTracker.getTopWindow({
    private: PrivateBrowsingUtils.isWindowPrivate(window),
    allowPopups: !skipPopups,
  });
}

function doGetProtocolFlags(aURI) {
  let handler = Services.io.getProtocolHandler(aURI.scheme);
  // see DoGetProtocolFlags in nsIProtocolHandler.idl
  return handler instanceof Ci.nsIProtocolHandlerWithDynamicFlags
    ? handler
        .QueryInterface(Ci.nsIProtocolHandlerWithDynamicFlags)
        .getFlagsForURI(aURI)
    : handler.protocolFlags;
}

/**
 * openUILink handles clicks on UI elements that cause URLs to load.
 *
 * As the third argument, you may pass an object with the same properties as
 * accepted by openUILinkIn, plus "ignoreButton" and "ignoreAlt".
 *
 * @param url {string}
 * @param event {Event | Object} Event or JSON object representing an Event
 * @param {Boolean | Object} aIgnoreButton
 * @param {Boolean} aIgnoreButton
 * @param {Boolean} aIgnoreAlt
 * @param {Boolean} aAllowThirdPartyFixup
 * @param {Object} aPostData
 * @param {Object} aReferrerInfo
 */
function openUILink(
  url,
  event,
  aIgnoreButton,
  aIgnoreAlt,
  aAllowThirdPartyFixup,
  aPostData,
  aReferrerInfo
) {
  event = getRootEvent(event);
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
      referrerInfo: aReferrerInfo,
      initiatingDoc: event ? event.target.ownerDocument : null,
    };
  }

  if (!params.triggeringPrincipal) {
    throw new Error(
      "Required argument triggeringPrincipal missing within openUILink"
    );
  }

  let where = whereToOpenLink(event, aIgnoreButton, aIgnoreAlt);
  openUILinkIn(url, where, params);
}

// Utility function to check command events for potential middle-click events
// from checkForMiddleClick and unwrap them.
function getRootEvent(aEvent) {
  // Part of the fix for Bug 1523813.
  // Middle-click events arrive here wrapped in different numbers (1-2) of
  // command events, depending on the button originally clicked.
  if (!aEvent) {
    return aEvent;
  }
  let tempEvent = aEvent;
  while (tempEvent.sourceEvent) {
    if (tempEvent.sourceEvent.button == 1) {
      aEvent = tempEvent.sourceEvent;
      break;
    }
    tempEvent = tempEvent.sourceEvent;
  }
  return aEvent;
}

/**
 * whereToOpenLink() looks at an event to decide where to open a link.
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
 *
 * @param e {Event|Object} Event or JSON Object
 * @param ignoreButton {Boolean}
 * @param ignoreAlt {Boolean}
 * @returns {"current" | "tabshifted" | "tab" | "save" | "window"}
 */
function whereToOpenLink(e, ignoreButton, ignoreAlt) {
  // This method must treat a null event like a left click without modifier keys (i.e.
  // e = { shiftKey:false, ctrlKey:false, metaKey:false, altKey:false, button:0 })
  // for compatibility purposes.
  if (!e) {
    return "current";
  }

  e = getRootEvent(e);

  var shift = e.shiftKey;
  var ctrl = e.ctrlKey;
  var meta = e.metaKey;
  var alt = e.altKey && !ignoreAlt;

  // ignoreButton allows "middle-click paste" to use function without always opening in a new window.
  let middle = !ignoreButton && e.button == 1;
  let middleUsesTabs = Services.prefs.getBoolPref(
    "browser.tabs.opentabfor.middleclick",
    true
  );
  let middleUsesNewWindow = Services.prefs.getBoolPref(
    "middlemouse.openNewWindow",
    false
  );

  // Don't do anything special with right-mouse clicks.  They're probably clicks on context menu items.

  var metaKey = AppConstants.platform == "macosx" ? meta : ctrl;
  if (metaKey || (middle && middleUsesTabs)) {
    return shift ? "tabshifted" : "tab";
  }

  if (alt && Services.prefs.getBoolPref("browser.altClickSave", false)) {
    return "save";
  }

  if (shift || (middle && !middleUsesTabs && middleUsesNewWindow)) {
    return "window";
  }

  return "current";
}

/* openTrustedLinkIn will attempt to open the given URI using the SystemPrincipal
 * as the trigeringPrincipal, unless a more specific Principal is provided.
 *
 * See openUILinkIn for a discussion of parameters
 */
function openTrustedLinkIn(url, where, aParams) {
  var params = aParams;

  if (!params) {
    params = {};
  }

  if (!params.triggeringPrincipal) {
    params.triggeringPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
  }

  openUILinkIn(url, where, params);
}

/* openWebLinkIn will attempt to open the given URI using the NullPrincipal
 * as the triggeringPrincipal, unless a more specific Principal is provided.
 *
 * See openUILinkIn for a discussion of parameters
 */
function openWebLinkIn(url, where, params) {
  if (!params) {
    params = {};
  }

  if (!params.triggeringPrincipal) {
    params.triggeringPrincipal = Services.scriptSecurityManager.createNullPrincipal(
      {}
    );
  }
  if (params.triggeringPrincipal.isSystemPrincipal) {
    throw new Error(
      "System principal should never be passed into openWebLinkIn()"
    );
  }

  openUILinkIn(url, where, params);
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
 * DEPRECATION WARNING:
 * USE        -> openTrustedLinkIn(url, where, aParams) if the source is always
 *                     a user event on a user- or product-specified URL (as
 *                     opposed to URLs provided by a webpage)
 * USE        -> openWebLinkIn(url, where, aParams) if the URI should be loaded
 *                     with a specific triggeringPrincipal, for instance, if
 *                     the url was supplied by web content.
 * DEPRECATED -> openUILinkIn(url, where, AllowThirdPartyFixup, aPostData, ...)
 *
 *
 * allowThirdPartyFixup controls whether third party services such as Google's
 * I Feel Lucky are allowed to interpret this URL. This parameter may be
 * undefined, which is treated as false.
 *
 * Instead of aAllowThirdPartyFixup, you may also pass an object with any of
 * these properties:
 *   allowThirdPartyFixup (boolean)
 *   postData             (nsIInputStream)
 *   referrerInfo         (nsIReferrerInfo)
 *   relatedToCurrent     (boolean)
 *   skipTabAnimation     (boolean)
 *   allowPinnedTabHostChange (boolean)
 *   allowPopups          (boolean)
 *   userContextId        (unsigned int)
 *   targetBrowser        (XUL browser)
 */
function openUILinkIn(
  url,
  where,
  aAllowThirdPartyFixup,
  aPostData,
  aReferrerInfo
) {
  var params;

  if (arguments.length == 3 && typeof arguments[2] == "object") {
    params = aAllowThirdPartyFixup;
  }
  if (!params || !params.triggeringPrincipal) {
    throw new Error(
      "Required argument triggeringPrincipal missing within openUILinkIn"
    );
  }

  params.fromChrome = true;

  openLinkIn(url, where, params);
}

/* eslint-disable complexity */
function openLinkIn(url, where, params) {
  if (!where || !url) {
    return;
  }

  var aFromChrome = params.fromChrome;
  var aAllowThirdPartyFixup = params.allowThirdPartyFixup;
  var aPostData = params.postData;
  var aCharset = params.charset;
  var aReferrerInfo = params.referrerInfo
    ? params.referrerInfo
    : new ReferrerInfo(Ci.nsIReferrerInfo.EMPTY, true, null);
  var aRelatedToCurrent = params.relatedToCurrent;
  var aAllowInheritPrincipal = !!params.allowInheritPrincipal;
  var aAllowMixedContent = params.allowMixedContent;
  var aForceAllowDataURI = params.forceAllowDataURI;
  var aInBackground = params.inBackground;
  var aInitiatingDoc = params.initiatingDoc;
  var aIsPrivate = params.private;
  var aSkipTabAnimation = params.skipTabAnimation;
  var aAllowPinnedTabHostChange = !!params.allowPinnedTabHostChange;
  var aAllowPopups = !!params.allowPopups;
  var aUserContextId = params.userContextId;
  var aIndicateErrorPageLoad = params.indicateErrorPageLoad;
  var aPrincipal = params.originPrincipal;
  var aStoragePrincipal = params.originStoragePrincipal;
  var aTriggeringPrincipal = params.triggeringPrincipal;
  var aCsp = params.csp;
  var aForceAboutBlankViewerInCurrent = params.forceAboutBlankViewerInCurrent;
  var aResolveOnNewTabCreated = params.resolveOnNewTabCreated;

  if (!aTriggeringPrincipal) {
    throw new Error("Must load with a triggering Principal");
  }

  if (where == "save") {
    // TODO(1073187): propagate referrerPolicy.
    // ContentClick.jsm passes isContentWindowPrivate for saveURL instead of passing a CPOW initiatingDoc
    if ("isContentWindowPrivate" in params) {
      saveURL(
        url,
        null,
        null,
        true,
        true,
        aReferrerInfo,
        null,
        params.isContentWindowPrivate,
        aPrincipal
      );
    } else {
      if (!aInitiatingDoc) {
        Cu.reportError(
          "openUILink/openLinkIn was called with " +
            "where == 'save' but without initiatingDoc.  See bug 814264."
        );
        return;
      }
      saveURL(url, null, null, true, true, aReferrerInfo, aInitiatingDoc);
    }
    return;
  }

  // Establish which window we'll load the link in.
  let w;
  if (where == "current" && params.targetBrowser) {
    w = params.targetBrowser.ownerGlobal;
  } else {
    w = getTopWin();
  }
  // We don't want to open tabs in popups, so try to find a non-popup window in
  // that case.
  if ((where == "tab" || where == "tabshifted") && w && !w.toolbar.visible) {
    w = getTopWin(true);
    aRelatedToCurrent = false;
  }

  // Teach the principal about the right OA to use, e.g. in case when
  // opening a link in a new private window, or in a new container tab.
  // Please note we do not have to do that for SystemPrincipals and we
  // can not do it for NullPrincipals since NullPrincipals are only
  // identical if they actually are the same object (See Bug: 1346759)
  function useOAForPrincipal(principal) {
    if (principal && principal.isContentPrincipal) {
      let attrs = {
        userContextId: aUserContextId,
        privateBrowsingId:
          aIsPrivate || (w && PrivateBrowsingUtils.isWindowPrivate(w)),
        firstPartyDomain: principal.originAttributes.firstPartyDomain,
      };
      return Services.scriptSecurityManager.principalWithOA(principal, attrs);
    }
    return principal;
  }
  aPrincipal = useOAForPrincipal(aPrincipal);
  aStoragePrincipal = useOAForPrincipal(aStoragePrincipal);
  aTriggeringPrincipal = useOAForPrincipal(aTriggeringPrincipal);

  if (!w || where == "window") {
    let features = "chrome,dialog=no,all";
    if (aIsPrivate) {
      features += ",private";
      // To prevent regular browsing data from leaking to private browsing sites,
      // strip the referrer when opening a new private window. (See Bug: 1409226)
      aReferrerInfo = new ReferrerInfo(
        aReferrerInfo.referrerPolicy,
        false,
        aReferrerInfo.originalReferrer
      );
    }

    // This propagates to window.arguments.
    var sa = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);

    var wuri = Cc["@mozilla.org/supports-string;1"].createInstance(
      Ci.nsISupportsString
    );
    wuri.data = url;

    let charset = null;
    if (aCharset) {
      charset = Cc["@mozilla.org/supports-string;1"].createInstance(
        Ci.nsISupportsString
      );
      charset.data = "charset=" + aCharset;
    }

    var allowThirdPartyFixupSupports = Cc[
      "@mozilla.org/supports-PRBool;1"
    ].createInstance(Ci.nsISupportsPRBool);
    allowThirdPartyFixupSupports.data = aAllowThirdPartyFixup;

    var userContextIdSupports = Cc[
      "@mozilla.org/supports-PRUint32;1"
    ].createInstance(Ci.nsISupportsPRUint32);
    userContextIdSupports.data = aUserContextId;

    sa.appendElement(wuri);
    sa.appendElement(charset);
    sa.appendElement(aReferrerInfo);
    sa.appendElement(aPostData);
    sa.appendElement(allowThirdPartyFixupSupports);
    sa.appendElement(userContextIdSupports);
    sa.appendElement(aPrincipal);
    sa.appendElement(aStoragePrincipal);
    sa.appendElement(aTriggeringPrincipal);
    sa.appendElement(null); // allowInheritPrincipal
    sa.appendElement(aCsp);

    const sourceWindow = w || window;
    let win;
    if (params.frameOuterWindowID != undefined && sourceWindow) {
      // Only notify it as a WebExtensions' webNavigation.onCreatedNavigationTarget
      // event if it contains the expected frameOuterWindowID params.
      // (e.g. we should not notify it as a onCreatedNavigationTarget if the user is
      // opening a new window using the keyboard shortcut).
      const sourceTabBrowser = sourceWindow.gBrowser.selectedBrowser;
      let delayedStartupObserver = aSubject => {
        if (aSubject == win) {
          Services.obs.removeObserver(
            delayedStartupObserver,
            "browser-delayed-startup-finished"
          );
          Services.obs.notifyObservers(
            {
              wrappedJSObject: {
                url,
                createdTabBrowser: win.gBrowser.selectedBrowser,
                sourceTabBrowser,
                sourceFrameOuterWindowID: params.frameOuterWindowID,
              },
            },
            "webNavigation-createdNavigationTarget"
          );
        }
      };
      Services.obs.addObserver(
        delayedStartupObserver,
        "browser-delayed-startup-finished"
      );
    }
    win = Services.ww.openWindow(
      sourceWindow,
      AppConstants.BROWSER_CHROME_URL,
      null,
      features,
      sa
    );
    return;
  }

  // We're now committed to loading the link in an existing browser window.

  // Raise the target window before loading the URI, since loading it may
  // result in a new frontmost window (e.g. "javascript:window.open('');").
  w.focus();

  let targetBrowser;
  let loadInBackground;
  let uriObj;

  if (where == "current") {
    targetBrowser = params.targetBrowser || w.gBrowser.selectedBrowser;
    loadInBackground = false;

    try {
      uriObj = Services.io.newURI(url);
    } catch (e) {}

    if (
      w.gBrowser.getTabForBrowser(targetBrowser).pinned &&
      !aAllowPinnedTabHostChange
    ) {
      try {
        // nsIURI.host can throw for non-nsStandardURL nsIURIs.
        if (
          !uriObj ||
          (!uriObj.schemeIs("javascript") &&
            targetBrowser.currentURI.host != uriObj.host)
        ) {
          where = "tab";
          loadInBackground = false;
        }
      } catch (err) {
        where = "tab";
        loadInBackground = false;
      }
    }
  } else {
    // 'where' is "tab" or "tabshifted", so we'll load the link in a new tab.
    loadInBackground = aInBackground;
    if (loadInBackground == null) {
      loadInBackground = aFromChrome
        ? false
        : Services.prefs.getBoolPref("browser.tabs.loadInBackground");
    }
  }

  let focusUrlBar = false;

  switch (where) {
    case "current":
      let flags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;

      if (aAllowThirdPartyFixup) {
        flags |= Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;
        flags |= Ci.nsIWebNavigation.LOAD_FLAGS_FIXUP_SCHEME_TYPOS;
      }
      // LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL isn't supported for javascript URIs,
      // i.e. it causes them not to load at all. Callers should strip
      // "javascript:" from pasted strings to prevent blank tabs
      if (!aAllowInheritPrincipal) {
        flags |= Ci.nsIWebNavigation.LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL;
      }

      if (aAllowPopups) {
        flags |= Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_POPUPS;
      }
      if (aIndicateErrorPageLoad) {
        flags |= Ci.nsIWebNavigation.LOAD_FLAGS_ERROR_LOAD_CHANGES_RV;
      }
      if (aForceAllowDataURI) {
        flags |= Ci.nsIWebNavigation.LOAD_FLAGS_FORCE_ALLOW_DATA_URI;
      }

      let { URI_INHERITS_SECURITY_CONTEXT } = Ci.nsIProtocolHandler;
      if (
        aForceAboutBlankViewerInCurrent &&
        (!uriObj || doGetProtocolFlags(uriObj) & URI_INHERITS_SECURITY_CONTEXT)
      ) {
        // Unless we know for sure we're not inheriting principals,
        // force the about:blank viewer to have the right principal:
        targetBrowser.createAboutBlankContentViewer(
          aPrincipal,
          aStoragePrincipal
        );
      }

      targetBrowser.loadURI(url, {
        triggeringPrincipal: aTriggeringPrincipal,
        csp: aCsp,
        flags,
        referrerInfo: aReferrerInfo,
        postData: aPostData,
        userContextId: aUserContextId,
      });

      // Don't focus the content area if focus is in the address bar and we're
      // loading the New Tab page.
      focusUrlBar =
        w.document.activeElement == w.gURLBar.inputField &&
        w.isBlankPageURL(url);
      break;
    case "tabshifted":
      loadInBackground = !loadInBackground;
    // fall through
    case "tab":
      focusUrlBar =
        !loadInBackground &&
        w.isBlankPageURL(url) &&
        !AboutNewTab.willNotifyUser;

      let tabUsedForLoad = w.gBrowser.loadOneTab(url, {
        referrerInfo: aReferrerInfo,
        charset: aCharset,
        postData: aPostData,
        inBackground: loadInBackground,
        allowThirdPartyFixup: aAllowThirdPartyFixup,
        relatedToCurrent: aRelatedToCurrent,
        skipAnimation: aSkipTabAnimation,
        allowMixedContent: aAllowMixedContent,
        userContextId: aUserContextId,
        originPrincipal: aPrincipal,
        originStoragePrincipal: aStoragePrincipal,
        triggeringPrincipal: aTriggeringPrincipal,
        allowInheritPrincipal: aAllowInheritPrincipal,
        csp: aCsp,
        focusUrlBar,
      });
      targetBrowser = tabUsedForLoad.linkedBrowser;

      if (aResolveOnNewTabCreated) {
        aResolveOnNewTabCreated(targetBrowser);
      }

      if (params.frameOuterWindowID != undefined && w) {
        // Only notify it as a WebExtensions' webNavigation.onCreatedNavigationTarget
        // event if it contains the expected frameOuterWindowID params.
        // (e.g. we should not notify it as a onCreatedNavigationTarget if the user is
        // opening a new tab using the keyboard shortcut).
        Services.obs.notifyObservers(
          {
            wrappedJSObject: {
              url,
              createdTabBrowser: targetBrowser,
              sourceTabBrowser: w.gBrowser.selectedBrowser,
              sourceFrameOuterWindowID: params.frameOuterWindowID,
            },
          },
          "webNavigation-createdNavigationTarget"
        );
      }
      break;
  }

  if (!focusUrlBar && targetBrowser == w.gBrowser.selectedBrowser) {
    // Focus the content, but only if the browser used for the load is selected.
    targetBrowser.focus();
  }
}

// Used as an onclick handler for UI elements with link-like behavior.
// e.g. onclick="checkForMiddleClick(this, event);"
function checkForMiddleClick(node, event) {
  // We should be using the disabled property here instead of the attribute,
  // but some elements that this function is used with don't support it (e.g.
  // menuitem).
  if (node.getAttribute("disabled") == "true") {
    return;
  } // Do nothing

  if (event.button == 1) {
    /* Execute the node's oncommand or command.
     */

    let cmdEvent = document.createEvent("xulcommandevent");
    cmdEvent.initCommandEvent(
      "command",
      true,
      true,
      window,
      0,
      event.ctrlKey,
      event.altKey,
      event.shiftKey,
      event.metaKey,
      event,
      event.mozInputSource
    );
    node.dispatchEvent(cmdEvent);

    // If the middle-click was on part of a menu, close the menu.
    // (Menus close automatically with left-click but not with middle-click.)
    closeMenus(event.target);
  }
}

// Populate a menu with user-context menu items. This method should be called
// by onpopupshowing passing the event as first argument.
function createUserContextMenu(
  event,
  {
    isContextMenu = false,
    excludeUserContextId = 0,
    showDefaultTab = false,
    useAccessKeys = true,
  } = {}
) {
  while (event.target.hasChildNodes()) {
    event.target.firstChild.remove();
  }

  let bundle = Services.strings.createBundle(
    "chrome://browser/locale/browser.properties"
  );
  let docfrag = document.createDocumentFragment();

  // If we are excluding a userContextId, we want to add a 'no-container' item.
  if (excludeUserContextId || showDefaultTab) {
    let menuitem = document.createXULElement("menuitem");
    menuitem.setAttribute("data-usercontextid", "0");
    menuitem.setAttribute(
      "label",
      bundle.GetStringFromName("userContextNone.label")
    );
    menuitem.setAttribute(
      "accesskey",
      bundle.GetStringFromName("userContextNone.accesskey")
    );

    // We don't set an oncommand/command attribute because if we have
    // to exclude a userContextId we are generating the contextMenu and
    // isContextMenu will be true.

    docfrag.appendChild(menuitem);

    let menuseparator = document.createXULElement("menuseparator");
    docfrag.appendChild(menuseparator);
  }

  ContextualIdentityService.getPublicIdentities().forEach(identity => {
    if (identity.userContextId == excludeUserContextId) {
      return;
    }

    let menuitem = document.createXULElement("menuitem");
    menuitem.setAttribute("data-usercontextid", identity.userContextId);
    menuitem.setAttribute(
      "label",
      ContextualIdentityService.getUserContextLabel(identity.userContextId)
    );

    if (identity.accessKey && useAccessKeys) {
      menuitem.setAttribute(
        "accesskey",
        bundle.GetStringFromName(identity.accessKey)
      );
    }

    menuitem.classList.add("menuitem-iconic");
    menuitem.classList.add("identity-color-" + identity.color);

    if (!isContextMenu) {
      menuitem.setAttribute("command", "Browser:NewUserContextTab");
    }

    menuitem.classList.add("identity-icon-" + identity.icon);

    docfrag.appendChild(menuitem);
  });

  if (!isContextMenu) {
    docfrag.appendChild(document.createXULElement("menuseparator"));

    let menuitem = document.createXULElement("menuitem");
    menuitem.setAttribute(
      "label",
      bundle.GetStringFromName("userContext.aboutPage.label")
    );
    if (useAccessKeys) {
      menuitem.setAttribute(
        "accesskey",
        bundle.GetStringFromName("userContext.aboutPage.accesskey")
      );
    }
    menuitem.setAttribute("command", "Browser:OpenAboutContainers");
    docfrag.appendChild(menuitem);
  }

  event.target.appendChild(docfrag);
  return true;
}

// Closes all popups that are ancestors of the node.
function closeMenus(node) {
  if ("tagName" in node) {
    if (
      node.namespaceURI ==
        "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul" &&
      (node.tagName == "menupopup" || node.tagName == "popup")
    ) {
      node.hidePopup();
    }

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
function eventMatchesKey(aEvent, aKey) {
  let keyPressed = aKey.getAttribute("key").toLowerCase();
  let keyModifiers = aKey.getAttribute("modifiers");
  let modifiers = ["Alt", "Control", "Meta", "Shift"];

  if (aEvent.key != keyPressed) {
    return false;
  }
  let eventModifiers = modifiers.filter(modifier =>
    aEvent.getModifierState(modifier)
  );
  // Check if aEvent has a modifier and aKey doesn't
  if (eventModifiers.length && !keyModifiers.length) {
    return false;
  }
  // Check whether aKey's modifiers match aEvent's modifiers
  if (keyModifiers) {
    keyModifiers = keyModifiers.split(/[\s,]+/);
    // Capitalize first letter of aKey's modifers to compare to aEvent's modifier
    keyModifiers.forEach(function(modifier, index) {
      if (modifier == "accel") {
        keyModifiers[index] =
          AppConstants.platform == "macosx" ? "Meta" : "Control";
      } else {
        keyModifiers[index] = modifier[0].toUpperCase() + modifier.slice(1);
      }
    });
    return modifiers.every(
      modifier =>
        keyModifiers.includes(modifier) == aEvent.getModifierState(modifier)
    );
  }
  return true;
}

// Gather all descendent text under given document node.
function gatherTextUnder(root) {
  var text = "";
  var node = root.firstChild;
  var depth = 1;
  while (node && depth > 0) {
    // See if this node is text.
    if (node.nodeType == Node.TEXT_NODE) {
      // Add this text to our collection.
      text += " " + node.data;
    } else if (node instanceof HTMLImageElement) {
      // If it has an "alt" attribute, add that.
      var altText = node.getAttribute("alt");
      if (altText && altText != "") {
        text += " " + altText;
      }
    }
    // Find next node to test.
    // First, see if this node has children.
    if (node.hasChildNodes()) {
      // Go to first child.
      node = node.firstChild;
      depth++;
    } else {
      // No children, try next sibling (or parent next sibling).
      while (depth > 0 && !node.nextSibling) {
        node = node.parentNode;
        depth--;
      }
      if (node.nextSibling) {
        node = node.nextSibling;
      }
    }
  }
  // Strip leading and tailing whitespace.
  text = text.trim();
  // Compress remaining whitespace.
  text = text.replace(/\s+/g, " ");
  return text;
}

// This function exists for legacy reasons.
function getShellService() {
  return ShellService;
}

function isBidiEnabled() {
  // first check the pref.
  if (Services.prefs.getBoolPref("bidi.browser.ui", false)) {
    return true;
  }

  // now see if the app locale is an RTL one.
  const isRTL = Services.locale.isAppLocaleRTL;

  if (isRTL) {
    Services.prefs.setBoolPref("bidi.browser.ui", true);
  }
  return isRTL;
}

function openAboutDialog() {
  for (let win of Services.wm.getEnumerator("Browser:About")) {
    // Only open one about window (Bug 599573)
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

  window.openDialog("chrome://browser/content/aboutDialog.xhtml", "", features);
}

function openPreferences(paneID, extraArgs) {
  // This function is duplicated from preferences.js.
  function internalPrefCategoryNameToFriendlyName(aName) {
    return (aName || "").replace(/^pane./, function(toReplace) {
      return toReplace[4].toLowerCase();
    });
  }

  let win = Services.wm.getMostRecentWindow("navigator:browser");
  let friendlyCategoryName = internalPrefCategoryNameToFriendlyName(paneID);
  let params;
  if (extraArgs && extraArgs.urlParams) {
    params = new URLSearchParams();
    let urlParams = extraArgs.urlParams;
    for (let name in urlParams) {
      if (urlParams[name] !== undefined) {
        params.set(name, urlParams[name]);
      }
    }
  }
  let preferencesURL =
    "about:preferences" +
    (params ? "?" + params : "") +
    (friendlyCategoryName ? "#" + friendlyCategoryName : "");
  let newLoad = true;
  let browser = null;
  if (!win) {
    let windowArguments = Cc["@mozilla.org/array;1"].createInstance(
      Ci.nsIMutableArray
    );
    let supportsStringPrefURL = Cc[
      "@mozilla.org/supports-string;1"
    ].createInstance(Ci.nsISupportsString);
    supportsStringPrefURL.data = preferencesURL;
    windowArguments.appendElement(supportsStringPrefURL);

    win = Services.ww.openWindow(
      null,
      AppConstants.BROWSER_CHROME_URL,
      "_blank",
      "chrome,dialog=no,all",
      windowArguments
    );
  } else {
    let shouldReplaceFragment = friendlyCategoryName
      ? "whenComparingAndReplace"
      : "whenComparing";
    newLoad = !win.switchToTabHavingURI(preferencesURL, true, {
      ignoreFragment: shouldReplaceFragment,
      replaceQueryString: true,
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    browser = win.gBrowser.selectedBrowser;
  }

  if (newLoad) {
    Services.obs.addObserver(function panesLoadedObs(prefWin, topic, data) {
      if (!browser) {
        browser = win.gBrowser.selectedBrowser;
      }
      if (prefWin != browser.contentWindow) {
        return;
      }
      Services.obs.removeObserver(panesLoadedObs, "sync-pane-loaded");
    }, "sync-pane-loaded");
  } else if (paneID) {
    browser.contentWindow.gotoPref(paneID);
  }
}

/**
 * Opens the troubleshooting information (about:support) page for this version
 * of the application.
 */
function openTroubleshootingPage() {
  openTrustedLinkIn("about:support", "tab");
}

/**
 * Opens the feedback page for this version of the application.
 */
function openFeedbackPage() {
  var url = Services.urlFormatter.formatURLPref("app.feedback.baseURL");
  openTrustedLinkIn(url, "tab");
}

function openTourPage() {
  let scope = {};
  ChromeUtils.import("resource:///modules/UITour.jsm", scope);
  openTrustedLinkIn(scope.UITour.url, "tab");
}

function buildHelpMenu() {
  document.getElementById(
    "feedbackPage"
  ).disabled = !Services.policies.isAllowed("feedbackCommands");

  document.getElementById(
    "helpSafeMode"
  ).disabled = !Services.policies.isAllowed("safeMode");

  let supportMenu = Services.policies.getSupportMenu();
  if (supportMenu) {
    let menuitem = document.getElementById("helpPolicySupport");
    menuitem.hidden = false;
    menuitem.setAttribute("label", supportMenu.Title);
    if ("AccessKey" in supportMenu) {
      menuitem.setAttribute("accesskey", supportMenu.AccessKey);
    }
    document.getElementById("helpPolicySeparator").hidden = false;
  }

  // Enable/disable the "Report Web Forgery" menu item.
  if (typeof gSafeBrowsing != "undefined") {
    gSafeBrowsing.setReportPhishingMenu();
  }

  updateImportCommandEnabledState();
}

function isElementVisible(aElement) {
  if (!aElement) {
    return false;
  }

  // If aElement or a direct or indirect parent is hidden or collapsed,
  // height, width or both will be 0.
  var rect = aElement.getBoundingClientRect();
  return rect.height > 0 && rect.width > 0;
}

function makeURLAbsolute(aBase, aUrl) {
  // Note:  makeURI() will throw if aUri is not a valid URI
  return makeURI(aUrl, null, makeURI(aBase)).spec;
}

function getHelpLinkURL(aHelpTopic) {
  var url = Services.urlFormatter.formatURLPref("app.support.baseURL");
  return url + aHelpTopic;
}

// aCalledFromModal is optional
function openHelpLink(aHelpTopic, aCalledFromModal, aWhere) {
  var url = getHelpLinkURL(aHelpTopic);
  var where = aWhere;
  if (!aWhere) {
    where = aCalledFromModal ? "window" : "tab";
  }

  openTrustedLinkIn(url, where);
}

function openPrefsHelp(aEvent) {
  let helpTopic = aEvent.target.getAttribute("helpTopic");
  openHelpLink(helpTopic);
}

/**
 * Updates the enabled state of the "Import From Another Browser" command
 * depending on the DisableProfileImport policy.
 */
function updateImportCommandEnabledState() {
  if (!Services.policies.isAllowed("profileImport")) {
    document
      .getElementById("cmd_file_importFromAnotherBrowser")
      .setAttribute("disabled", "true");
    document
      .getElementById("cmd_help_importFromAnotherBrowser")
      .setAttribute("disabled", "true");
  }
}
