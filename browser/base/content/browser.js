# -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

let Ci = Components.interfaces;
let Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const nsIWebNavigation = Ci.nsIWebNavigation;

var gCharsetMenu = null;
var gLastBrowserCharset = null;
var gPrevCharset = null;
var gProxyFavIcon = null;
var gLastValidURLStr = "";
var gInPrintPreviewMode = false;
var gDownloadMgr = null;
var gContextMenu = null; // nsContextMenu instance
var gStartupRan = false;

#ifndef XP_MACOSX
var gEditUIVisible = true;
#endif

[
  ["gBrowser",            "content"],
  ["gNavToolbox",         "navigator-toolbox"],
  ["gURLBar",             "urlbar"],
  ["gNavigatorBundle",    "bundle_browser"]
].forEach(function (elementGlobal) {
  var [name, id] = elementGlobal;
  window.__defineGetter__(name, function () {
    var element = document.getElementById(id);
    if (!element)
      return null;
    delete window[name];
    return window[name] = element;
  });
  window.__defineSetter__(name, function (val) {
    delete window[name];
    return window[name] = val;
  });
});

// Smart getter for the findbar.  If you don't wish to force the creation of
// the findbar, check gFindBarInitialized first.
var gFindBarInitialized = false;
XPCOMUtils.defineLazyGetter(window, "gFindBar", function() {
  let XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
  let findbar = document.createElementNS(XULNS, "findbar");
  findbar.id = "FindToolbar";

  let browserBottomBox = document.getElementById("browser-bottombox");
  browserBottomBox.insertBefore(findbar, browserBottomBox.firstChild);

  // Force a style flush to ensure that our binding is attached.
  findbar.clientTop;
  findbar.browser = gBrowser;
  window.gFindBarInitialized = true;
  return findbar;
});

XPCOMUtils.defineLazyGetter(this, "gPrefService", function() {
  return Services.prefs;
});

__defineGetter__("AddonManager", function() {
  let tmp = {};
  Cu.import("resource://gre/modules/AddonManager.jsm", tmp);
  return this.AddonManager = tmp.AddonManager;
});
__defineSetter__("AddonManager", function (val) {
  delete this.AddonManager;
  return this.AddonManager = val;
});

__defineGetter__("PluralForm", function() {
  Cu.import("resource://gre/modules/PluralForm.jsm");
  return this.PluralForm;
});
__defineSetter__("PluralForm", function (val) {
  delete this.PluralForm;
  return this.PluralForm = val;
});

XPCOMUtils.defineLazyModuleGetter(this, "TelemetryStopwatch",
  "resource://gre/modules/TelemetryStopwatch.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AboutHomeUtils",
  "resource:///modules/AboutHomeUtils.jsm");

#ifdef MOZ_SERVICES_SYNC
XPCOMUtils.defineLazyModuleGetter(this, "Weave",
  "resource://services-sync/main.js");
#endif

XPCOMUtils.defineLazyGetter(this, "PopupNotifications", function () {
  let tmp = {};
  Cu.import("resource://gre/modules/PopupNotifications.jsm", tmp);
  try {
    return new tmp.PopupNotifications(gBrowser,
                                      document.getElementById("notification-popup"),
                                      document.getElementById("notification-popup-box"));
  } catch (ex) {
    Cu.reportError(ex);
    return null;
  }
});

XPCOMUtils.defineLazyGetter(this, "DeveloperToolbar", function() {
  let tmp = {};
  Cu.import("resource:///modules/devtools/DeveloperToolbar.jsm", tmp);
  return new tmp.DeveloperToolbar(window, document.getElementById("developer-toolbar"));
});

XPCOMUtils.defineLazyGetter(this, "DebuggerUI", function() {
  let tmp = {};
  Cu.import("resource:///modules/devtools/DebuggerUI.jsm", tmp);
  return new tmp.DebuggerUI(window);
});

XPCOMUtils.defineLazyModuleGetter(this, "Social",
  "resource:///modules/Social.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PageThumbs",
  "resource:///modules/PageThumbs.jsm");

#ifdef MOZ_SAFE_BROWSING
XPCOMUtils.defineLazyModuleGetter(this, "SafeBrowsing",
  "resource://gre/modules/SafeBrowsing.jsm");
#endif

XPCOMUtils.defineLazyModuleGetter(this, "gBrowserNewTabPreloader",
  "resource:///modules/BrowserNewTabPreloader.jsm", "BrowserNewTabPreloader");

XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");

let gInitialPages = [
  "about:blank",
  "about:newtab",
  "about:home",
  "about:privatebrowsing",
  "about:sessionrestore"
];

#include browser-addons.js
#include browser-feeds.js
#include browser-fullScreen.js
#include browser-fullZoom.js
#include browser-places.js
#include browser-plugins.js
#include browser-safebrowsing.js
#include browser-social.js
#include browser-tabPreviews.js
#include browser-tabview.js
#include browser-thumbnails.js

#ifdef MOZ_SERVICES_SYNC
#include browser-syncui.js
#endif

XPCOMUtils.defineLazyGetter(this, "Win7Features", function () {
#ifdef XP_WIN
  const WINTASKBAR_CONTRACTID = "@mozilla.org/windows-taskbar;1";
  if (WINTASKBAR_CONTRACTID in Cc &&
      Cc[WINTASKBAR_CONTRACTID].getService(Ci.nsIWinTaskbar).available) {
    let temp = {};
    Cu.import("resource:///modules/WindowsPreviewPerTab.jsm", temp);
    let AeroPeek = temp.AeroPeek;
    return {
      onOpenWindow: function () {
        AeroPeek.onOpenWindow(window);
      },
      onCloseWindow: function () {
        AeroPeek.onCloseWindow(window);
      }
    };
  }
#endif
  return null;
});

#ifdef MOZ_CRASHREPORTER
XPCOMUtils.defineLazyServiceGetter(this, "gCrashReporter",
                                   "@mozilla.org/xre/app-info;1",
                                   "nsICrashReporter");
#endif

XPCOMUtils.defineLazyGetter(this, "PageMenu", function() {
  let tmp = {};
  Cu.import("resource://gre/modules/PageMenu.jsm", tmp);
  return new tmp.PageMenu();
});

/**
* We can avoid adding multiple load event listeners and save some time by adding
* one listener that calls all real handlers.
*/
function pageShowEventHandlers(event) {
  charsetLoadListener();
  XULBrowserWindow.asyncUpdateUI();

  // The PluginClickToPlay events are not fired when navigating using the
  // BF cache. |event.persisted| is true when the page is loaded from the
  // BF cache, so this code reshows the notification if necessary.
  if (event.persisted)
    gPluginHandler.reshowClickToPlayNotification();
}

function UpdateBackForwardCommands(aWebNavigation) {
  var backBroadcaster = document.getElementById("Browser:Back");
  var forwardBroadcaster = document.getElementById("Browser:Forward");

  // Avoid setting attributes on broadcasters if the value hasn't changed!
  // Remember, guys, setting attributes on elements is expensive!  They
  // get inherited into anonymous content, broadcast to other widgets, etc.!
  // Don't do it if the value hasn't changed! - dwh

  var backDisabled = backBroadcaster.hasAttribute("disabled");
  var forwardDisabled = forwardBroadcaster.hasAttribute("disabled");
  if (backDisabled == aWebNavigation.canGoBack) {
    if (backDisabled)
      backBroadcaster.removeAttribute("disabled");
    else
      backBroadcaster.setAttribute("disabled", true);
  }

  if (forwardDisabled == aWebNavigation.canGoForward) {
    if (forwardDisabled)
      forwardBroadcaster.removeAttribute("disabled");
    else
      forwardBroadcaster.setAttribute("disabled", true);
  }
}

/**
 * Click-and-Hold implementation for the Back and Forward buttons
 * XXXmano: should this live in toolbarbutton.xml?
 */
function SetClickAndHoldHandlers() {
  var timer;

  function openMenu(aButton) {
    cancelHold(aButton);
    aButton.firstChild.hidden = false;
    aButton.open = true;
  }

  function mousedownHandler(aEvent) {
    if (aEvent.button != 0 ||
        aEvent.currentTarget.open ||
        aEvent.currentTarget.disabled)
      return;

    // Prevent the menupopup from opening immediately
    aEvent.currentTarget.firstChild.hidden = true;

    aEvent.currentTarget.addEventListener("mouseout", mouseoutHandler, false);
    aEvent.currentTarget.addEventListener("mouseup", mouseupHandler, false);
    timer = setTimeout(openMenu, 500, aEvent.currentTarget);
  }

  function mouseoutHandler(aEvent) {
    let buttonRect = aEvent.currentTarget.getBoundingClientRect();
    if (aEvent.clientX >= buttonRect.left &&
        aEvent.clientX <= buttonRect.right &&
        aEvent.clientY >= buttonRect.bottom)
      openMenu(aEvent.currentTarget);
    else
      cancelHold(aEvent.currentTarget);
  }

  function mouseupHandler(aEvent) {
    cancelHold(aEvent.currentTarget);
  }

  function cancelHold(aButton) {
    clearTimeout(timer);
    aButton.removeEventListener("mouseout", mouseoutHandler, false);
    aButton.removeEventListener("mouseup", mouseupHandler, false);
  }

  function clickHandler(aEvent) {
    if (aEvent.button == 0 &&
        aEvent.target == aEvent.currentTarget &&
        !aEvent.currentTarget.open &&
        !aEvent.currentTarget.disabled) {
      let cmdEvent = document.createEvent("xulcommandevent");
      cmdEvent.initCommandEvent("command", true, true, window, 0,
                                aEvent.ctrlKey, aEvent.altKey, aEvent.shiftKey,
                                aEvent.metaKey, null);
      aEvent.currentTarget.dispatchEvent(cmdEvent);
    }
  }

  function _addClickAndHoldListenersOnElement(aElm) {
    aElm.addEventListener("mousedown", mousedownHandler, true);
    aElm.addEventListener("click", clickHandler, true);
  }

  // Bug 414797: Clone unified-back-forward-button's context menu into both the
  // back and the forward buttons.
  var unifiedButton = document.getElementById("unified-back-forward-button");
  if (unifiedButton && !unifiedButton._clickHandlersAttached) {
    unifiedButton._clickHandlersAttached = true;

    let popup = document.getElementById("backForwardMenu").cloneNode(true);
    popup.removeAttribute("id");
    // Prevent the context attribute on unified-back-forward-button from being
    // inherited.
    popup.setAttribute("context", "");

    let backButton = document.getElementById("back-button");
    backButton.setAttribute("type", "menu");
    backButton.appendChild(popup);
    _addClickAndHoldListenersOnElement(backButton);

    let forwardButton = document.getElementById("forward-button");
    popup = popup.cloneNode(true);
    forwardButton.setAttribute("type", "menu");
    forwardButton.appendChild(popup);
    _addClickAndHoldListenersOnElement(forwardButton);
  }
}

const gSessionHistoryObserver = {
  observe: function(subject, topic, data)
  {
    if (topic != "browser:purge-session-history")
      return;

    var backCommand = document.getElementById("Browser:Back");
    backCommand.setAttribute("disabled", "true");
    var fwdCommand = document.getElementById("Browser:Forward");
    fwdCommand.setAttribute("disabled", "true");

    // Hide session restore button on about:home
    window.messageManager.broadcastAsyncMessage("Browser:HideSessionRestoreButton");

    if (gURLBar) {
      // Clear undo history of the URL bar
      gURLBar.editor.transactionManager.clear()
    }
  }
};

/**
 * Given a starting docshell and a URI to look up, find the docshell the URI
 * is loaded in.
 * @param   aDocument
 *          A document to find instead of using just a URI - this is more specific.
 * @param   aDocShell
 *          The doc shell to start at
 * @param   aSoughtURI
 *          The URI that we're looking for
 * @returns The doc shell that the sought URI is loaded in. Can be in
 *          subframes.
 */
function findChildShell(aDocument, aDocShell, aSoughtURI) {
  aDocShell.QueryInterface(Components.interfaces.nsIWebNavigation);
  aDocShell.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
  var doc = aDocShell.getInterface(Components.interfaces.nsIDOMDocument);
  if ((aDocument && doc == aDocument) ||
      (aSoughtURI && aSoughtURI.spec == aDocShell.currentURI.spec))
    return aDocShell;

  var node = aDocShell.QueryInterface(Components.interfaces.nsIDocShellTreeNode);
  for (var i = 0; i < node.childCount; ++i) {
    var docShell = node.getChildAt(i);
    docShell = findChildShell(aDocument, docShell, aSoughtURI);
    if (docShell)
      return docShell;
  }
  return null;
}

var gPopupBlockerObserver = {
  _reportButton: null,

  onReportButtonClick: function (aEvent)
  {
    if (aEvent.button != 0 || aEvent.target != this._reportButton)
      return;

    document.getElementById("blockedPopupOptions")
            .openPopup(this._reportButton, "after_end", 0, 2, false, false, aEvent);
  },

  handleEvent: function (aEvent)
  {
    if (aEvent.originalTarget != gBrowser.selectedBrowser)
      return;

    if (!this._reportButton && gURLBar)
      this._reportButton = document.getElementById("page-report-button");

    if (!gBrowser.pageReport) {
      // Hide the icon in the location bar (if the location bar exists)
      if (gURLBar)
        this._reportButton.hidden = true;
      return;
    }

    if (gURLBar)
      this._reportButton.hidden = false;

    // Only show the notification again if we've not already shown it. Since
    // notifications are per-browser, we don't need to worry about re-adding
    // it.
    if (!gBrowser.pageReport.reported) {
      if (gPrefService.getBoolPref("privacy.popups.showBrowserMessage")) {
        var brandBundle = document.getElementById("bundle_brand");
        var brandShortName = brandBundle.getString("brandShortName");
        var message;
        var popupCount = gBrowser.pageReport.length;
#ifdef XP_WIN
        var popupButtonText = gNavigatorBundle.getString("popupWarningButton");
        var popupButtonAccesskey = gNavigatorBundle.getString("popupWarningButton.accesskey");
#else
        var popupButtonText = gNavigatorBundle.getString("popupWarningButtonUnix");
        var popupButtonAccesskey = gNavigatorBundle.getString("popupWarningButtonUnix.accesskey");
#endif
        if (popupCount > 1)
          message = gNavigatorBundle.getFormattedString("popupWarningMultiple", [brandShortName, popupCount]);
        else
          message = gNavigatorBundle.getFormattedString("popupWarning", [brandShortName]);

        var notificationBox = gBrowser.getNotificationBox();
        var notification = notificationBox.getNotificationWithValue("popup-blocked");
        if (notification) {
          notification.label = message;
        }
        else {
          var buttons = [{
            label: popupButtonText,
            accessKey: popupButtonAccesskey,
            popup: "blockedPopupOptions",
            callback: null
          }];

          const priority = notificationBox.PRIORITY_WARNING_MEDIUM;
          notificationBox.appendNotification(message, "popup-blocked",
                                             "chrome://browser/skin/Info.png",
                                             priority, buttons);
        }
      }

      // Record the fact that we've reported this blocked popup, so we don't
      // show it again.
      gBrowser.pageReport.reported = true;
    }
  },

  toggleAllowPopupsForSite: function (aEvent)
  {
    var pm = Services.perms;
    var shouldBlock = aEvent.target.getAttribute("block") == "true";
    var perm = shouldBlock ? pm.DENY_ACTION : pm.ALLOW_ACTION;
    pm.add(gBrowser.currentURI, "popup", perm);

    gBrowser.getNotificationBox().removeCurrentNotification();
  },

  fillPopupList: function (aEvent)
  {
    // XXXben - rather than using |currentURI| here, which breaks down on multi-framed sites
    //          we should really walk the pageReport and create a list of "allow for <host>"
    //          menuitems for the common subset of hosts present in the report, this will
    //          make us frame-safe.
    //
    // XXXjst - Note that when this is fixed to work with multi-framed sites,
    //          also back out the fix for bug 343772 where
    //          nsGlobalWindow::CheckOpenAllow() was changed to also
    //          check if the top window's location is whitelisted.
    var uri = gBrowser.currentURI;
    var blockedPopupAllowSite = document.getElementById("blockedPopupAllowSite");
    try {
      blockedPopupAllowSite.removeAttribute("hidden");

      var pm = Services.perms;
      if (pm.testPermission(uri, "popup") == pm.ALLOW_ACTION) {
        // Offer an item to block popups for this site, if a whitelist entry exists
        // already for it.
        let blockString = gNavigatorBundle.getFormattedString("popupBlock", [uri.host || uri.spec]);
        blockedPopupAllowSite.setAttribute("label", blockString);
        blockedPopupAllowSite.setAttribute("block", "true");
      }
      else {
        // Offer an item to allow popups for this site
        let allowString = gNavigatorBundle.getFormattedString("popupAllow", [uri.host || uri.spec]);
        blockedPopupAllowSite.setAttribute("label", allowString);
        blockedPopupAllowSite.removeAttribute("block");
      }
    }
    catch (e) {
      blockedPopupAllowSite.setAttribute("hidden", "true");
    }

    if (PrivateBrowsingUtils.isWindowPrivate(window))
      blockedPopupAllowSite.setAttribute("disabled", "true");
    else
      blockedPopupAllowSite.removeAttribute("disabled");

    var foundUsablePopupURI = false;
    var pageReports = gBrowser.pageReport;
    if (pageReports) {
      for (let pageReport of pageReports) {
        // popupWindowURI will be null if the file picker popup is blocked.
        // xxxdz this should make the option say "Show file picker" and do it (Bug 590306)
        if (!pageReport.popupWindowURI)
          continue;
        var popupURIspec = pageReport.popupWindowURI.spec;

        // Sometimes the popup URI that we get back from the pageReport
        // isn't useful (for instance, netscape.com's popup URI ends up
        // being "http://www.netscape.com", which isn't really the URI of
        // the popup they're trying to show).  This isn't going to be
        // useful to the user, so we won't create a menu item for it.
        if (popupURIspec == "" || popupURIspec == "about:blank" ||
            popupURIspec == uri.spec)
          continue;

        // Because of the short-circuit above, we may end up in a situation
        // in which we don't have any usable popup addresses to show in
        // the menu, and therefore we shouldn't show the separator.  However,
        // since we got past the short-circuit, we must've found at least
        // one usable popup URI and thus we'll turn on the separator later.
        foundUsablePopupURI = true;

        var menuitem = document.createElement("menuitem");
        var label = gNavigatorBundle.getFormattedString("popupShowPopupPrefix",
                                                        [popupURIspec]);
        menuitem.setAttribute("label", label);
        menuitem.setAttribute("popupWindowURI", popupURIspec);
        menuitem.setAttribute("popupWindowFeatures", pageReport.popupWindowFeatures);
        menuitem.setAttribute("popupWindowName", pageReport.popupWindowName);
        menuitem.setAttribute("oncommand", "gPopupBlockerObserver.showBlockedPopup(event);");
        menuitem.requestingWindow = pageReport.requestingWindow;
        menuitem.requestingDocument = pageReport.requestingDocument;
        aEvent.target.appendChild(menuitem);
      }
    }

    // Show or hide the separator, depending on whether we added any
    // showable popup addresses to the menu.
    var blockedPopupsSeparator =
      document.getElementById("blockedPopupsSeparator");
    if (foundUsablePopupURI)
      blockedPopupsSeparator.removeAttribute("hidden");
    else
      blockedPopupsSeparator.setAttribute("hidden", true);

    var blockedPopupDontShowMessage = document.getElementById("blockedPopupDontShowMessage");
    var showMessage = gPrefService.getBoolPref("privacy.popups.showBrowserMessage");
    blockedPopupDontShowMessage.setAttribute("checked", !showMessage);
    if (aEvent.target.anchorNode.id == "page-report-button") {
      aEvent.target.anchorNode.setAttribute("open", "true");
      blockedPopupDontShowMessage.setAttribute("label", gNavigatorBundle.getString("popupWarningDontShowFromLocationbar"));
    } else
      blockedPopupDontShowMessage.setAttribute("label", gNavigatorBundle.getString("popupWarningDontShowFromMessage"));
  },

  onPopupHiding: function (aEvent) {
    if (aEvent.target.anchorNode.id == "page-report-button")
      aEvent.target.anchorNode.removeAttribute("open");

    let item = aEvent.target.lastChild;
    while (item && item.getAttribute("observes") != "blockedPopupsSeparator") {
      let next = item.previousSibling;
      item.parentNode.removeChild(item);
      item = next;
    }
  },

  showBlockedPopup: function (aEvent)
  {
    var target = aEvent.target;
    var popupWindowURI = target.getAttribute("popupWindowURI");
    var features = target.getAttribute("popupWindowFeatures");
    var name = target.getAttribute("popupWindowName");

    var dwi = target.requestingWindow;

    // If we have a requesting window and the requesting document is
    // still the current document, open the popup.
    if (dwi && dwi.document == target.requestingDocument) {
      dwi.open(popupWindowURI, name, features);
    }
  },

  editPopupSettings: function ()
  {
    var host = "";
    try {
      host = gBrowser.currentURI.host;
    }
    catch (e) { }

    var bundlePreferences = document.getElementById("bundle_preferences");
    var params = { blockVisible   : false,
                   sessionVisible : false,
                   allowVisible   : true,
                   prefilledHost  : host,
                   permissionType : "popup",
                   windowTitle    : bundlePreferences.getString("popuppermissionstitle"),
                   introText      : bundlePreferences.getString("popuppermissionstext") };
    var existingWindow = Services.wm.getMostRecentWindow("Browser:Permissions");
    if (existingWindow) {
      existingWindow.initWithParams(params);
      existingWindow.focus();
    }
    else
      window.openDialog("chrome://browser/content/preferences/permissions.xul",
                        "_blank", "resizable,dialog=no,centerscreen", params);
  },

  dontShowMessage: function ()
  {
    var showMessage = gPrefService.getBoolPref("privacy.popups.showBrowserMessage");
    gPrefService.setBoolPref("privacy.popups.showBrowserMessage", !showMessage);
    gBrowser.getNotificationBox().removeCurrentNotification();
  }
};

const gFormSubmitObserver = {
  QueryInterface : XPCOMUtils.generateQI([Ci.nsIFormSubmitObserver]),

  panel: null,

  init: function()
  {
    this.panel = document.getElementById('invalid-form-popup');
  },

  notifyInvalidSubmit : function (aFormElement, aInvalidElements)
  {
    // We are going to handle invalid form submission attempt by focusing the
    // first invalid element and show the corresponding validation message in a
    // panel attached to the element.
    if (!aInvalidElements.length) {
      return;
    }

    // Don't show the popup if the current tab doesn't contain the invalid form.
    if (gBrowser.contentDocument !=
        aFormElement.ownerDocument.defaultView.top.document) {
      return;
    }

    let element = aInvalidElements.queryElementAt(0, Ci.nsISupports);

    if (!(element instanceof HTMLInputElement ||
          element instanceof HTMLTextAreaElement ||
          element instanceof HTMLSelectElement ||
          element instanceof HTMLButtonElement)) {
      return;
    }

    this.panel.firstChild.textContent = element.validationMessage;

    element.focus();

    // If the user interacts with the element and makes it valid or leaves it,
    // we want to remove the popup.
    // We could check for clicks but a click is already removing the popup.
    function blurHandler() {
      gFormSubmitObserver.panel.hidePopup();
    };
    function inputHandler(e) {
      if (e.originalTarget.validity.valid) {
        gFormSubmitObserver.panel.hidePopup();
      } else {
        // If the element is now invalid for a new reason, we should update the
        // error message.
        if (gFormSubmitObserver.panel.firstChild.textContent !=
            e.originalTarget.validationMessage) {
          gFormSubmitObserver.panel.firstChild.textContent =
            e.originalTarget.validationMessage;
        }
      }
    };
    element.addEventListener("input", inputHandler, false);
    element.addEventListener("blur", blurHandler, false);

    // One event to bring them all and in the darkness bind them.
    this.panel.addEventListener("popuphiding", function onPopupHiding(aEvent) {
      aEvent.target.removeEventListener("popuphiding", onPopupHiding, false);
      element.removeEventListener("input", inputHandler, false);
      element.removeEventListener("blur", blurHandler, false);
    }, false);

    this.panel.hidden = false;

    // We want to show the popup at the middle of checkbox and radio buttons
    // and where the content begin for the other elements.
    let offset = 0;
    let position = "";

    if (element.tagName == 'INPUT' &&
        (element.type == 'radio' || element.type == 'checkbox')) {
      position = "bottomcenter topleft";
    } else {
      let win = element.ownerDocument.defaultView;
      let style = win.getComputedStyle(element, null);
      let utils = win.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                     .getInterface(Components.interfaces.nsIDOMWindowUtils);

      if (style.direction == 'rtl') {
        offset = parseInt(style.paddingRight) + parseInt(style.borderRightWidth);
      } else {
        offset = parseInt(style.paddingLeft) + parseInt(style.borderLeftWidth);
      }

      offset = Math.round(offset * utils.fullZoom);

      position = "after_start";
    }

    this.panel.openPopup(element, position, offset, 0);
  }
};

// Simple gestures support
//
// As per bug #412486, web content must not be allowed to receive any
// simple gesture events.  Multi-touch gesture APIs are in their
// infancy and we do NOT want to be forced into supporting an API that
// will probably have to change in the future.  (The current Mac OS X
// API is undocumented and was reverse-engineered.)  Until support is
// implemented in the event dispatcher to keep these events as
// chrome-only, we must listen for the simple gesture events during
// the capturing phase and call stopPropagation on every event.

let gGestureSupport = {
  /**
   * Add or remove mouse gesture event listeners
   *
   * @param aAddListener
   *        True to add/init listeners and false to remove/uninit
   */
  init: function GS_init(aAddListener) {
    const gestureEvents = ["SwipeGesture",
      "MagnifyGestureStart", "MagnifyGestureUpdate", "MagnifyGesture",
      "RotateGestureStart", "RotateGestureUpdate", "RotateGesture",
      "TapGesture", "PressTapGesture"];

    let addRemove = aAddListener ? window.addEventListener :
      window.removeEventListener;

    gestureEvents.forEach(function (event) addRemove("Moz" + event, this, true),
                          this);
  },

  /**
   * Dispatch events based on the type of mouse gesture event. For now, make
   * sure to stop propagation of every gesture event so that web content cannot
   * receive gesture events.
   *
   * @param aEvent
   *        The gesture event to handle
   */
  handleEvent: function GS_handleEvent(aEvent) {
    aEvent.stopPropagation();

    // Create a preference object with some defaults
    let def = function(aThreshold, aLatched)
      ({ threshold: aThreshold, latched: !!aLatched });

    switch (aEvent.type) {
      case "MozSwipeGesture":
        aEvent.preventDefault();
        this.onSwipe(aEvent);
        break;
      case "MozMagnifyGestureStart":
        aEvent.preventDefault();
#ifdef XP_WIN
        this._setupGesture(aEvent, "pinch", def(25, 0), "out", "in");
#else
        this._setupGesture(aEvent, "pinch", def(150, 1), "out", "in");
#endif
        break;
      case "MozRotateGestureStart":
        aEvent.preventDefault();
        this._setupGesture(aEvent, "twist", def(25, 0), "right", "left");
        break;
      case "MozMagnifyGestureUpdate":
      case "MozRotateGestureUpdate":
        aEvent.preventDefault();
        this._doUpdate(aEvent);
        break;
      case "MozTapGesture":
        aEvent.preventDefault();
        this._doAction(aEvent, ["tap"]);
        break;
      /* case "MozPressTapGesture":
        break; */
    }
  },

  /**
   * Called at the start of "pinch" and "twist" gestures to setup all of the
   * information needed to process the gesture
   *
   * @param aEvent
   *        The continual motion start event to handle
   * @param aGesture
   *        Name of the gesture to handle
   * @param aPref
   *        Preference object with the names of preferences and defaults
   * @param aInc
   *        Command to trigger for increasing motion (without gesture name)
   * @param aDec
   *        Command to trigger for decreasing motion (without gesture name)
   */
  _setupGesture: function GS__setupGesture(aEvent, aGesture, aPref, aInc, aDec) {
    // Try to load user-set values from preferences
    for (let [pref, def] in Iterator(aPref))
      aPref[pref] = this._getPref(aGesture + "." + pref, def);

    // Keep track of the total deltas and latching behavior
    let offset = 0;
    let latchDir = aEvent.delta > 0 ? 1 : -1;
    let isLatched = false;

    // Create the update function here to capture closure state
    this._doUpdate = function GS__doUpdate(aEvent) {
      // Update the offset with new event data
      offset += aEvent.delta;

      // Check if the cumulative deltas exceed the threshold
      if (Math.abs(offset) > aPref["threshold"]) {
        // Trigger the action if we don't care about latching; otherwise, make
        // sure either we're not latched and going the same direction of the
        // initial motion; or we're latched and going the opposite way
        let sameDir = (latchDir ^ offset) >= 0;
        if (!aPref["latched"] || (isLatched ^ sameDir)) {
          this._doAction(aEvent, [aGesture, offset > 0 ? aInc : aDec]);

          // We must be getting latched or leaving it, so just toggle
          isLatched = !isLatched;
        }

        // Reset motion counter to prepare for more of the same gesture
        offset = 0;
      }
    };

    // The start event also contains deltas, so handle an update right away
    this._doUpdate(aEvent);
  },

  /**
   * Generator producing the powerset of the input array where the first result
   * is the complete set and the last result (before StopIteration) is empty.
   *
   * @param aArray
   *        Source array containing any number of elements
   * @yield Array that is a subset of the input array from full set to empty
   */
  _power: function GS__power(aArray) {
    // Create a bitmask based on the length of the array
    let num = 1 << aArray.length;
    while (--num >= 0) {
      // Only select array elements where the current bit is set
      yield aArray.reduce(function (aPrev, aCurr, aIndex) {
        if (num & 1 << aIndex)
          aPrev.push(aCurr);
        return aPrev;
      }, []);
    }
  },

  /**
   * Determine what action to do for the gesture based on which keys are
   * pressed and which commands are set
   *
   * @param aEvent
   *        The original gesture event to convert into a fake click event
   * @param aGesture
   *        Array of gesture name parts (to be joined by periods)
   */
  _doAction: function GS__doAction(aEvent, aGesture) {
    // Create an array of pressed keys in a fixed order so that a command for
    // "meta" is preferred over "ctrl" when both buttons are pressed (and a
    // command for both don't exist)
    let keyCombos = [];
    ["shift", "alt", "ctrl", "meta"].forEach(function (key) {
      if (aEvent[key + "Key"])
        keyCombos.push(key);
    });

    // Try each combination of key presses in decreasing order for commands
    for (let subCombo of this._power(keyCombos)) {
      // Convert a gesture and pressed keys into the corresponding command
      // action where the preference has the gesture before "shift" before
      // "alt" before "ctrl" before "meta" all separated by periods
      let command;
      try {
        command = this._getPref(aGesture.concat(subCombo).join("."));
      } catch (e) {}

      if (!command)
        continue;

      let node = document.getElementById(command);
      if (node) {
        if (node.getAttribute("disabled") != "true") {
          let cmdEvent = document.createEvent("xulcommandevent");
          cmdEvent.initCommandEvent("command", true, true, window, 0,
                                    aEvent.ctrlKey, aEvent.altKey, aEvent.shiftKey,
                                    aEvent.metaKey, null);
          node.dispatchEvent(cmdEvent);
        }
      } else {
        goDoCommand(command);
      }

      break;
    }
  },

  /**
   * Convert continual motion events into an action if it exceeds a threshold
   * in a given direction. This function will be set by _setupGesture to
   * capture state that needs to be shared across multiple gesture updates.
   *
   * @param aEvent
   *        The continual motion update event to handle
   */
  _doUpdate: function(aEvent) {},

  /**
   * Convert the swipe gesture into a browser action based on the direction
   *
   * @param aEvent
   *        The swipe event to handle
   */
  onSwipe: function GS_onSwipe(aEvent) {
    // Figure out which one (and only one) direction was triggered
    for (let dir of ["UP", "RIGHT", "DOWN", "LEFT"]) {
      if (aEvent.direction == aEvent["DIRECTION_" + dir]) {
        this._doAction(aEvent, ["swipe", dir.toLowerCase()]);
        break;
      }
    }
  },

  /**
   * Get a gesture preference or use a default if it doesn't exist
   *
   * @param aPref
   *        Name of the preference to load under the gesture branch
   * @param aDef
   *        Default value if the preference doesn't exist
   */
  _getPref: function GS__getPref(aPref, aDef) {
    // Preferences branch under which all gestures preferences are stored
    const branch = "browser.gesture.";

    try {
      // Determine what type of data to load based on default value's type
      let type = typeof aDef;
      let getFunc = "get" + (type == "boolean" ? "Bool" :
                             type == "number" ? "Int" : "Char") + "Pref";
      return gPrefService[getFunc](branch + aPref);
    }
    catch (e) {
      return aDef;
    }
  },
};

var gBrowserInit = {
  onLoad: function() {
    // window.arguments[0]: URI to load (string), or an nsISupportsArray of
    //                      nsISupportsStrings to load, or a xul:tab of
    //                      a tabbrowser, which will be replaced by this
    //                      window (for this case, all other arguments are
    //                      ignored).
    //                 [1]: character set (string)
    //                 [2]: referrer (nsIURI)
    //                 [3]: postData (nsIInputStream)
    //                 [4]: allowThirdPartyFixup (bool)
    if ("arguments" in window && window.arguments[0])
      var uriToLoad = window.arguments[0];

    var mustLoadSidebar = false;

    gBrowser.addEventListener("DOMUpdatePageReport", gPopupBlockerObserver, false);

    // Note that the XBL binding is untrusted
    gBrowser.addEventListener("PluginBindingAttached", gPluginHandler, true, true);
    gBrowser.addEventListener("PluginCrashed",         gPluginHandler, true);
    gBrowser.addEventListener("PluginOutdated",        gPluginHandler, true);

    gBrowser.addEventListener("NewPluginInstalled", gPluginHandler.newPluginInstalled, true);
#ifdef XP_MACOSX
    gBrowser.addEventListener("npapi-carbon-event-model-failure", gPluginHandler, true);
#endif

    Services.obs.addObserver(gPluginHandler.pluginCrashed, "plugin-crashed", false);

    window.addEventListener("AppCommand", HandleAppCommandEvent, true);

    messageManager.loadFrameScript("chrome://browser/content/content.js", true);

    // initialize observers and listeners
    // and give C++ access to gBrowser
    gBrowser.init();
    XULBrowserWindow.init();
    window.QueryInterface(Ci.nsIInterfaceRequestor)
          .getInterface(nsIWebNavigation)
          .QueryInterface(Ci.nsIDocShellTreeItem).treeOwner
          .QueryInterface(Ci.nsIInterfaceRequestor)
          .getInterface(Ci.nsIXULWindow)
          .XULBrowserWindow = window.XULBrowserWindow;
    window.QueryInterface(Ci.nsIDOMChromeWindow).browserDOMWindow =
      new nsBrowserAccess();

    // set default character set if provided
    if ("arguments" in window && window.arguments.length > 1 && window.arguments[1]) {
      if (window.arguments[1].startsWith("charset=")) {
        var arrayArgComponents = window.arguments[1].split("=");
        if (arrayArgComponents) {
          //we should "inherit" the charset menu setting in a new window
          getMarkupDocumentViewer().defaultCharacterSet = arrayArgComponents[1];
        }
      }
    }

    // Manually hook up session and global history for the first browser
    // so that we don't have to load global history before bringing up a
    // window.
    // Wire up session and global history before any possible
    // progress notifications for back/forward button updating
    gBrowser.webNavigation.sessionHistory = Cc["@mozilla.org/browser/shistory;1"].
                                            createInstance(Ci.nsISHistory);
    Services.obs.addObserver(gBrowser.browsers[0], "browser:purge-session-history", false);

    // remove the disablehistory attribute so the browser cleans up, as
    // though it had done this work itself
    gBrowser.browsers[0].removeAttribute("disablehistory");

    // enable global history
    try {
      gBrowser.docShell.QueryInterface(Ci.nsIDocShellHistory).useGlobalHistory = true;
    } catch(ex) {
      Cu.reportError("Places database may be locked: " + ex);
    }

#ifdef MOZ_E10S_COMPAT
    // Bug 666801 - WebProgress support for e10s
#else
    // hook up UI through progress listener
    gBrowser.addProgressListener(window.XULBrowserWindow);
    gBrowser.addTabsProgressListener(window.TabsProgressListener);
#endif

    // setup our common DOMLinkAdded listener
    gBrowser.addEventListener("DOMLinkAdded", DOMLinkHandler, false);

    // setup our MozApplicationManifest listener
    gBrowser.addEventListener("MozApplicationManifest",
                              OfflineApps, false);

    // setup simple gestures support
    gGestureSupport.init(true);

    if (window.opener && !window.opener.closed) {
      let openerSidebarBox = window.opener.document.getElementById("sidebar-box");
      // If the opener had a sidebar, open the same sidebar in our window.
      // The opener can be the hidden window too, if we're coming from the state
      // where no windows are open, and the hidden window has no sidebar box.
      if (openerSidebarBox && !openerSidebarBox.hidden) {
        let sidebarCmd = openerSidebarBox.getAttribute("sidebarcommand");
        let sidebarCmdElem = document.getElementById(sidebarCmd);

        // dynamically generated sidebars will fail this check.
        if (sidebarCmdElem) {
          let sidebarBox = document.getElementById("sidebar-box");
          let sidebarTitle = document.getElementById("sidebar-title");

          sidebarTitle.setAttribute(
            "value", window.opener.document.getElementById("sidebar-title").getAttribute("value"));
          sidebarBox.setAttribute("width", openerSidebarBox.boxObject.width);

          sidebarBox.setAttribute("sidebarcommand", sidebarCmd);
          // Note: we're setting 'src' on sidebarBox, which is a <vbox>, not on
          // the <browser id="sidebar">. This lets us delay the actual load until
          // delayedStartup().
          sidebarBox.setAttribute(
            "src", window.opener.document.getElementById("sidebar").getAttribute("src"));
          mustLoadSidebar = true;

          sidebarBox.hidden = false;
          document.getElementById("sidebar-splitter").hidden = false;
          sidebarCmdElem.setAttribute("checked", "true");
        }
      }
    }
    else {
      let box = document.getElementById("sidebar-box");
      if (box.hasAttribute("sidebarcommand")) {
        let commandID = box.getAttribute("sidebarcommand");
        if (commandID) {
          let command = document.getElementById(commandID);
          if (command) {
            mustLoadSidebar = true;
            box.hidden = false;
            document.getElementById("sidebar-splitter").hidden = false;
            command.setAttribute("checked", "true");
          }
          else {
            // Remove the |sidebarcommand| attribute, because the element it
            // refers to no longer exists, so we should assume this sidebar
            // panel has been uninstalled. (249883)
            box.removeAttribute("sidebarcommand");
          }
        }
      }
    }

    // Certain kinds of automigration rely on this notification to complete their
    // tasks BEFORE the browser window is shown.
    Services.obs.notifyObservers(null, "browser-window-before-show", "");

    // Set a sane starting width/height for all resolutions on new profiles.
    if (!document.documentElement.hasAttribute("width")) {
      let defaultWidth = 994;
      let defaultHeight;
      if (screen.availHeight <= 600) {
        document.documentElement.setAttribute("sizemode", "maximized");
        defaultWidth = 610;
        defaultHeight = 450;
      }
      else {
        // Create a narrower window for large or wide-aspect displays, to suggest
        // side-by-side page view.
        if (screen.availWidth >= 1600)
          defaultWidth = (screen.availWidth / 2) - 20;
        defaultHeight = screen.availHeight - 10;
#ifdef MOZ_WIDGET_GTK2
        // On X, we're not currently able to account for the size of the window
        // border.  Use 28px as a guess (titlebar + bottom window border)
        defaultHeight -= 28;
#endif
      }
      document.documentElement.setAttribute("width", defaultWidth);
      document.documentElement.setAttribute("height", defaultHeight);
    }

    if (!gShowPageResizers)
      document.getElementById("status-bar").setAttribute("hideresizer", "true");

    if (!window.toolbar.visible) {
      // adjust browser UI for popups
      if (gURLBar) {
        gURLBar.setAttribute("readonly", "true");
        gURLBar.setAttribute("enablehistory", "false");
      }
      goSetCommandEnabled("cmd_newNavigatorTab", false);
    }

#ifdef MENUBAR_CAN_AUTOHIDE
    updateAppButtonDisplay();
#endif

    // Misc. inits.
    CombinedStopReload.init();
    allTabs.readPref();
    TabsOnTop.init();
    BookmarksMenuButton.init();
    TabsInTitlebar.init();
    gPrivateBrowsingUI.init();
    retrieveToolbarIconsizesFromTheme();

    // Wait until chrome is painted before executing code not critical to making the window visible
    this._boundDelayedStartup = this._delayedStartup.bind(this, uriToLoad, mustLoadSidebar);
    window.addEventListener("MozAfterPaint", this._boundDelayedStartup);

    gStartupRan = true;
  },

  _cancelDelayedStartup: function () {
    window.removeEventListener("MozAfterPaint", this._boundDelayedStartup);
    this._boundDelayedStartup = null;
  },

  _delayedStartup: function(uriToLoad, mustLoadSidebar) {
    let tmp = {};
    Cu.import("resource:///modules/TelemetryTimestamps.jsm", tmp);
    let TelemetryTimestamps = tmp.TelemetryTimestamps;
    TelemetryTimestamps.add("delayedStartupStarted");

    this._cancelDelayedStartup();

    var isLoadingBlank = isBlankPageURL(uriToLoad);

    if (uriToLoad && uriToLoad != "about:blank") {
      if (uriToLoad instanceof Ci.nsISupportsArray) {
        let count = uriToLoad.Count();
        let specs = [];
        for (let i = 0; i < count; i++) {
          let urisstring = uriToLoad.GetElementAt(i).QueryInterface(Ci.nsISupportsString);
          specs.push(urisstring.data);
        }

        // This function throws for certain malformed URIs, so use exception handling
        // so that we don't disrupt startup
        try {
          gBrowser.loadTabs(specs, false, true);
        } catch (e) {}
      }
      else if (uriToLoad instanceof XULElement) {
        // swap the given tab with the default about:blank tab and then close
        // the original tab in the other window.

        // Stop the about:blank load
        gBrowser.stop();
        // make sure it has a docshell
        gBrowser.docShell;

        gBrowser.swapBrowsersAndCloseOther(gBrowser.selectedTab, uriToLoad);
      }
      else if (window.arguments.length >= 3) {
        loadURI(uriToLoad, window.arguments[2], window.arguments[3] || null,
                window.arguments[4] || false);
        window.focus();
      }
      // Note: loadOneOrMoreURIs *must not* be called if window.arguments.length >= 3.
      // Such callers expect that window.arguments[0] is handled as a single URI.
      else
        loadOneOrMoreURIs(uriToLoad);
    }

#ifdef MOZ_SAFE_BROWSING
    // Bug 778855 - Perf regression if we do this here. To be addressed in bug 779008.
    setTimeout(function() { SafeBrowsing.init(); }, 2000);
#endif

    Services.obs.addObserver(gSessionHistoryObserver, "browser:purge-session-history", false);
    Services.obs.addObserver(gXPInstallObserver, "addon-install-disabled", false);
    Services.obs.addObserver(gXPInstallObserver, "addon-install-started", false);
    Services.obs.addObserver(gXPInstallObserver, "addon-install-blocked", false);
    Services.obs.addObserver(gXPInstallObserver, "addon-install-failed", false);
    Services.obs.addObserver(gXPInstallObserver, "addon-install-complete", false);
    Services.obs.addObserver(gFormSubmitObserver, "invalidformsubmit", false);

    BrowserOffline.init();
    OfflineApps.init();
    IndexedDBPromptHelper.init();
    gFormSubmitObserver.init();
    SocialUI.init();
    AddonManager.addAddonListener(AddonsMgrListener);

    gBrowser.addEventListener("pageshow", function(event) {
      // Filter out events that are not about the document load we are interested in
      if (event.target == content.document)
        setTimeout(pageShowEventHandlers, 0, event);
    }, true);

    // Ensure login manager is up and running.
    Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager);

    if (mustLoadSidebar) {
      let sidebar = document.getElementById("sidebar");
      let sidebarBox = document.getElementById("sidebar-box");
      sidebar.setAttribute("src", sidebarBox.getAttribute("src"));
    }

    UpdateUrlbarSearchSplitterState();

    if (!isLoadingBlank || !focusAndSelectUrlBar())
      gBrowser.selectedBrowser.focus();

    gNavToolbox.customizeDone = BrowserToolboxCustomizeDone;
    gNavToolbox.customizeChange = BrowserToolboxCustomizeChange;

    // Set up Sanitize Item
    this._initializeSanitizer();

    // Enable/Disable auto-hide tabbar
    gBrowser.tabContainer.updateVisibility();

    gPrefService.addObserver(gHomeButton.prefDomain, gHomeButton, false);

    var homeButton = document.getElementById("home-button");
    gHomeButton.updateTooltip(homeButton);
    gHomeButton.updatePersonalToolbarStyle(homeButton);

    // BiDi UI
    gBidiUI = isBidiEnabled();
    if (gBidiUI) {
      document.getElementById("documentDirection-separator").hidden = false;
      document.getElementById("documentDirection-swap").hidden = false;
      document.getElementById("textfieldDirection-separator").hidden = false;
      document.getElementById("textfieldDirection-swap").hidden = false;
    }

    // Setup click-and-hold gestures access to the session history
    // menus if global click-and-hold isn't turned on
    if (!getBoolPref("ui.click_hold_context_menus", false))
      SetClickAndHoldHandlers();

    // Initialize the full zoom setting.
    // We do this before the session restore service gets initialized so we can
    // apply full zoom settings to tabs restored by the session restore service.
    FullZoom.init();

#ifdef MOZ_E10S_COMPAT
    // Bug 666804 - NetworkPrioritizer support for e10s
#else
    let NP = {};
    Cu.import("resource:///modules/NetworkPrioritizer.jsm", NP);
    NP.trackBrowserWindow(window);
#endif

    // initialize the session-restore service (in case it's not already running)
    try {
      Cc["@mozilla.org/browser/sessionstore;1"]
        .getService(Ci.nsISessionStore)
        .init(window);
    } catch (ex) {
      dump("nsSessionStore could not be initialized: " + ex + "\n");
    }

    PlacesToolbarHelper.init();

    ctrlTab.readPref();
    gPrefService.addObserver(ctrlTab.prefName, ctrlTab, false);
    gPrefService.addObserver(allTabs.prefName, allTabs, false);

    // Initialize the download manager some time after the app starts so that
    // auto-resume downloads begin (such as after crashing or quitting with
    // active downloads) and speeds up the first-load of the download manager UI.
    // If the user manually opens the download manager before the timeout, the
    // downloads will start right away, and getting the service again won't hurt.
    setTimeout(function() {
      gDownloadMgr = Cc["@mozilla.org/download-manager;1"].
                     getService(Ci.nsIDownloadManager);

#ifdef XP_WIN
      if (Win7Features) {
        let tempScope = {};
        Cu.import("resource://gre/modules/DownloadTaskbarProgress.jsm",
                  tempScope);
        tempScope.DownloadTaskbarProgress.onBrowserWindowLoad(window);
      }
#endif
    }, 10000);

    // The object handling the downloads indicator is also initialized here in the
    // delayed startup function, but the actual indicator element is not loaded
    // unless there are downloads to be displayed.
    DownloadsButton.initializeIndicator();

#ifndef XP_MACOSX
    updateEditUIVisibility();
    let placesContext = document.getElementById("placesContext");
    placesContext.addEventListener("popupshowing", updateEditUIVisibility, false);
    placesContext.addEventListener("popuphiding", updateEditUIVisibility, false);
#endif

    gBrowser.mPanelContainer.addEventListener("InstallBrowserTheme", LightWeightThemeWebInstaller, false, true);
    gBrowser.mPanelContainer.addEventListener("PreviewBrowserTheme", LightWeightThemeWebInstaller, false, true);
    gBrowser.mPanelContainer.addEventListener("ResetBrowserThemePreview", LightWeightThemeWebInstaller, false, true);

#ifdef MOZ_E10S_COMPAT
    // Bug 666808 - AeroPeek support for e10s
#else
    if (Win7Features)
      Win7Features.onOpenWindow();
#endif

   // called when we go into full screen, even if initiated by a web page script
    window.addEventListener("fullscreen", onFullScreen, true);

    // Called when we enter DOM full-screen mode. Note we can already be in browser
    // full-screen mode when we enter DOM full-screen mode.
    window.addEventListener("MozEnteredDomFullscreen", onMozEnteredDomFullscreen, true);

    if (window.fullScreen)
      onFullScreen();
    if (document.mozFullScreen)
      onMozEnteredDomFullscreen();

#ifdef MOZ_SERVICES_SYNC
    // initialize the sync UI
    gSyncUI.init();
#endif

    gBrowserThumbnails.init();
    TabView.init();

    setUrlAndSearchBarWidthForConditionalForwardButton();
    window.addEventListener("resize", function resizeHandler(event) {
      if (event.target == window)
        setUrlAndSearchBarWidthForConditionalForwardButton();
    });

    // Enable developer toolbar?
    let devToolbarEnabled = gPrefService.getBoolPref("devtools.toolbar.enabled");
    if (devToolbarEnabled) {
      let cmd = document.getElementById("Tools:DevToolbar");
      cmd.removeAttribute("disabled");
      cmd.removeAttribute("hidden");
      document.getElementById("Tools:DevToolbarFocus").removeAttribute("disabled");

      // Show the toolbar if it was previously visible
      if (gPrefService.getBoolPref("devtools.toolbar.visible")) {
        DeveloperToolbar.show(false);
      }
    }

    // Enable Chrome Debugger?
    let enabled = gPrefService.getBoolPref("devtools.chrome.enabled") &&
                  gPrefService.getBoolPref("devtools.debugger.chrome-enabled") &&
                  gPrefService.getBoolPref("devtools.debugger.remote-enabled");
    if (enabled) {
      let cmd = document.getElementById("Tools:ChromeDebugger");
      cmd.removeAttribute("disabled");
      cmd.removeAttribute("hidden");
    }

    // Enable Error Console?
    // Temporarily enabled. See bug 798925.
    let consoleEnabled = true || gPrefService.getBoolPref("devtools.errorconsole.enabled") ||
                         gPrefService.getBoolPref("devtools.chrome.enabled");
    if (consoleEnabled) {
      let cmd = document.getElementById("Tools:ErrorConsole");
      cmd.removeAttribute("disabled");
      cmd.removeAttribute("hidden");
    }

    // Enable Scratchpad in the UI, if the preference allows this.
    let scratchpadEnabled = gPrefService.getBoolPref(Scratchpad.prefEnabledName);
    if (scratchpadEnabled) {
      let cmd = document.getElementById("Tools:Scratchpad");
      cmd.removeAttribute("disabled");
      cmd.removeAttribute("hidden");
    }

#ifdef MENUBAR_CAN_AUTOHIDE
    // If the user (or the locale) hasn't enabled the top-level "Character
    // Encoding" menu via the "browser.menu.showCharacterEncoding" preference,
    // hide it.
    if ("true" != gPrefService.getComplexValue("browser.menu.showCharacterEncoding",
                                               Ci.nsIPrefLocalizedString).data)
      document.getElementById("appmenu_charsetMenu").hidden = true;
#endif

    // Enable Responsive UI?
    let responsiveUIEnabled = gPrefService.getBoolPref("devtools.responsiveUI.enabled");
    if (responsiveUIEnabled) {
      let cmd = document.getElementById("Tools:ResponsiveUI");
      cmd.removeAttribute("disabled");
      cmd.removeAttribute("hidden");
    }

    // Add Devtools menuitems and listeners
    gDevToolsBrowser.registerBrowserWindow(window);

    let appMenuButton = document.getElementById("appmenu-button");
    let appMenuPopup = document.getElementById("appmenu-popup");
    if (appMenuButton && appMenuPopup) {
      let appMenuOpening = null;
      appMenuButton.addEventListener("mousedown", function(event) {
        if (event.button == 0)
          appMenuOpening = new Date();
      }, false);
      appMenuPopup.addEventListener("popupshown", function(event) {
        if (event.target != appMenuPopup || !appMenuOpening)
          return;
        let duration = new Date() - appMenuOpening;
        appMenuOpening = null;
        Services.telemetry.getHistogramById("FX_APP_MENU_OPEN_MS").add(duration);
      }, false);
    }

    window.addEventListener("mousemove", MousePosTracker, false);
    window.addEventListener("dragover", MousePosTracker, false);

    // End startup crash tracking after a delay to catch crashes while restoring
    // tabs and to postpone saving the pref to disk.
    try {
      let appStartup = Cc["@mozilla.org/toolkit/app-startup;1"].
                       getService(Ci.nsIAppStartup);
      const startupCrashEndDelay = 30 * 1000;
      setTimeout(appStartup.trackStartupCrashEnd, startupCrashEndDelay);
    } catch (ex) {
      Cu.reportError("Could not end startup crash tracking: " + ex);
    }

    Services.obs.notifyObservers(window, "browser-delayed-startup-finished", "");
    TelemetryTimestamps.add("delayedStartupFinished");
  },

  onUnload: function() {
    // In certain scenarios it's possible for unload to be fired before onload,
    // (e.g. if the window is being closed after browser.js loads but before the
    // load completes). In that case, there's nothing to do here.
    if (!gStartupRan)
      return;

    gDevToolsBrowser.forgetBrowserWindow(window);

    // First clean up services initialized in gBrowserInit.onLoad (or those whose
    // uninit methods don't depend on the services having been initialized).
    allTabs.uninit();

    CombinedStopReload.uninit();

    gGestureSupport.init(false);

    FullScreen.cleanup();

    Services.obs.removeObserver(gPluginHandler.pluginCrashed, "plugin-crashed");

    try {
      gBrowser.removeProgressListener(window.XULBrowserWindow);
      gBrowser.removeTabsProgressListener(window.TabsProgressListener);
    } catch (ex) {
    }

    PlacesStarButton.uninit();

#ifndef MOZ_PER_WINDOW_PRIVATE_BROWSING
    gPrivateBrowsingUI.uninit();
#endif

    TabsOnTop.uninit();

    TabsInTitlebar.uninit();

    var enumerator = Services.wm.getEnumerator(null);
    enumerator.getNext();
    if (!enumerator.hasMoreElements()) {
      document.persist("sidebar-box", "sidebarcommand");
      document.persist("sidebar-box", "width");
      document.persist("sidebar-box", "src");
      document.persist("sidebar-title", "value");
    }

    // Now either cancel delayedStartup, or clean up the services initialized from
    // it.
    if (this._boundDelayedStartup) {
      this._cancelDelayedStartup();
    } else {
      if (Win7Features)
        Win7Features.onCloseWindow();

      gPrefService.removeObserver(ctrlTab.prefName, ctrlTab);
      gPrefService.removeObserver(allTabs.prefName, allTabs);
      ctrlTab.uninit();
      TabView.uninit();
      gBrowserThumbnails.uninit();
      FullZoom.destroy();

      Services.obs.removeObserver(gSessionHistoryObserver, "browser:purge-session-history");
      Services.obs.removeObserver(gXPInstallObserver, "addon-install-disabled");
      Services.obs.removeObserver(gXPInstallObserver, "addon-install-started");
      Services.obs.removeObserver(gXPInstallObserver, "addon-install-blocked");
      Services.obs.removeObserver(gXPInstallObserver, "addon-install-failed");
      Services.obs.removeObserver(gXPInstallObserver, "addon-install-complete");
      Services.obs.removeObserver(gFormSubmitObserver, "invalidformsubmit");

      try {
        gPrefService.removeObserver(gHomeButton.prefDomain, gHomeButton);
      } catch (ex) {
        Cu.reportError(ex);
      }

      BrowserOffline.uninit();
      OfflineApps.uninit();
      IndexedDBPromptHelper.uninit();
      AddonManager.removeAddonListener(AddonsMgrListener);
      SocialUI.uninit();
    }

    // Final window teardown, do this last.
    window.XULBrowserWindow.destroy();
    window.XULBrowserWindow = null;
    window.QueryInterface(Ci.nsIInterfaceRequestor)
          .getInterface(Ci.nsIWebNavigation)
          .QueryInterface(Ci.nsIDocShellTreeItem).treeOwner
          .QueryInterface(Ci.nsIInterfaceRequestor)
          .getInterface(Ci.nsIXULWindow)
          .XULBrowserWindow = null;
    window.QueryInterface(Ci.nsIDOMChromeWindow).browserDOMWindow = null;
  },

#ifdef XP_MACOSX
  // nonBrowserWindowStartup(), nonBrowserWindowDelayedStartup(), and
  // nonBrowserWindowShutdown() are used for non-browser windows in
  // macBrowserOverlay
  nonBrowserWindowStartup: function() {
    // Disable inappropriate commands / submenus
    var disabledItems = ['Browser:SavePage',
                         'Browser:SendLink', 'cmd_pageSetup', 'cmd_print', 'cmd_find', 'cmd_findAgain',
                         'viewToolbarsMenu', 'viewSidebarMenuMenu', 'Browser:Reload',
                         'viewFullZoomMenu', 'pageStyleMenu', 'charsetMenu', 'View:PageSource', 'View:FullScreen',
                         'viewHistorySidebar', 'Browser:AddBookmarkAs', 'Browser:BookmarkAllTabs',
                         'View:PageInfo', 'Browser:ToggleTabView', 'Browser:ToggleAddonBar'];
    var element;

    for (let disabledItem of disabledItems) {
      element = document.getElementById(disabledItem);
      if (element)
        element.setAttribute("disabled", "true");
    }

    // If no windows are active (i.e. we're the hidden window), disable the close, minimize
    // and zoom menu commands as well
    if (window.location.href == "chrome://browser/content/hiddenWindow.xul") {
      var hiddenWindowDisabledItems = ['cmd_close', 'minimizeWindow', 'zoomWindow'];
      for (let hiddenWindowDisabledItem of hiddenWindowDisabledItems) {
        element = document.getElementById(hiddenWindowDisabledItem);
        if (element)
          element.setAttribute("disabled", "true");
      }

      // also hide the window-list separator
      element = document.getElementById("sep-window-list");
      element.setAttribute("hidden", "true");

      // Setup the dock menu.
      let dockMenuElement = document.getElementById("menu_mac_dockmenu");
      if (dockMenuElement != null) {
        let nativeMenu = Cc["@mozilla.org/widget/standalonenativemenu;1"]
                         .createInstance(Ci.nsIStandaloneNativeMenu);

        try {
          nativeMenu.init(dockMenuElement);

          let dockSupport = Cc["@mozilla.org/widget/macdocksupport;1"]
                            .getService(Ci.nsIMacDockSupport);
          dockSupport.dockMenu = nativeMenu;
        }
        catch (e) {
        }
      }
    }

    this._delayedStartupTimeoutId = setTimeout(this.nonBrowserWindowDelayedStartup.bind(this), 0);
  },

  nonBrowserWindowDelayedStartup: function() {
    this._delayedStartupTimeoutId = null;

    // initialise the offline listener
    BrowserOffline.init();

    // Set up Sanitize Item
    this._initializeSanitizer();

    // initialize the private browsing UI
    gPrivateBrowsingUI.init();

#ifdef MOZ_SERVICES_SYNC
    // initialize the sync UI
    gSyncUI.init();
#endif

    gStartupRan = true;
  },

  nonBrowserWindowShutdown: function() {
    // If nonBrowserWindowDelayedStartup hasn't run yet, we have no work to do -
    // just cancel the pending timeout and return;
    if (this._delayedStartupTimeoutId) {
      clearTimeout(this._delayedStartupTimeoutId);
      return;
    }

    BrowserOffline.uninit();

#ifndef MOZ_PER_WINDOW_PRIVATE_BROWSING
    gPrivateBrowsingUI.uninit();
#endif
  },
#endif

  _initializeSanitizer: function() {
    const kDidSanitizeDomain = "privacy.sanitize.didShutdownSanitize";
    if (gPrefService.prefHasUserValue(kDidSanitizeDomain)) {
      gPrefService.clearUserPref(kDidSanitizeDomain);
      // We need to persist this preference change, since we want to
      // check it at next app start even if the browser exits abruptly
      gPrefService.savePrefFile(null);
    }

    /**
     * Migrate Firefox 3.0 privacy.item prefs under one of these conditions:
     *
     * a) User has customized any privacy.item prefs
     * b) privacy.sanitize.sanitizeOnShutdown is set
     */
    if (!gPrefService.getBoolPref("privacy.sanitize.migrateFx3Prefs")) {
      let itemBranch = gPrefService.getBranch("privacy.item.");
      let itemArray = itemBranch.getChildList("");

      // See if any privacy.item prefs are set
      let doMigrate = itemArray.some(function (name) itemBranch.prefHasUserValue(name));
      // Or if sanitizeOnShutdown is set
      if (!doMigrate)
        doMigrate = gPrefService.getBoolPref("privacy.sanitize.sanitizeOnShutdown");

      if (doMigrate) {
        let cpdBranch = gPrefService.getBranch("privacy.cpd.");
        let clearOnShutdownBranch = gPrefService.getBranch("privacy.clearOnShutdown.");
        for (let name of itemArray) {
          try {
            // don't migrate password or offlineApps clearing in the CRH dialog since
            // there's no UI for those anymore. They default to false. bug 497656
            if (name != "passwords" && name != "offlineApps")
              cpdBranch.setBoolPref(name, itemBranch.getBoolPref(name));
            clearOnShutdownBranch.setBoolPref(name, itemBranch.getBoolPref(name));
          }
          catch(e) {
            Cu.reportError("Exception thrown during privacy pref migration: " + e);
          }
        }
      }

      gPrefService.setBoolPref("privacy.sanitize.migrateFx3Prefs", true);
    }
  },
}

/* Legacy global init functions */
var BrowserStartup        = gBrowserInit.onLoad.bind(gBrowserInit);
var BrowserShutdown       = gBrowserInit.onUnload.bind(gBrowserInit);
#ifdef XP_MACOSX
var nonBrowserWindowStartup        = gBrowserInit.nonBrowserWindowStartup.bind(gBrowserInit);
var nonBrowserWindowDelayedStartup = gBrowserInit.nonBrowserWindowDelayedStartup.bind(gBrowserInit);
var nonBrowserWindowShutdown       = gBrowserInit.nonBrowserWindowShutdown.bind(gBrowserInit);
#endif


function HandleAppCommandEvent(evt) {
  evt.stopPropagation();
  switch (evt.command) {
  case "Back":
    BrowserBack();
    break;
  case "Forward":
    BrowserForward();
    break;
  case "Reload":
    BrowserReloadSkipCache();
    break;
  case "Stop":
    BrowserStop();
    break;
  case "Search":
    BrowserSearch.webSearch();
    break;
  case "Bookmarks":
    toggleSidebar('viewBookmarksSidebar');
    break;
  case "Home":
    BrowserHome();
    break;
  default:
    break;
  }
}

function gotoHistoryIndex(aEvent) {
  let index = aEvent.target.getAttribute("index");
  if (!index)
    return false;

  let where = whereToOpenLink(aEvent);

  if (where == "current") {
    // Normal click. Go there in the current tab and update session history.

    try {
      gBrowser.gotoIndex(index);
    }
    catch(ex) {
      return false;
    }
    return true;
  }
  // Modified click. Go there in a new tab/window.

  duplicateTabIn(gBrowser.selectedTab, where, index - gBrowser.sessionHistory.index);
  return true;
}

function BrowserForward(aEvent) {
  let where = whereToOpenLink(aEvent, false, true);

  if (where == "current") {
    try {
      gBrowser.goForward();
    }
    catch(ex) {
    }
  }
  else {
    duplicateTabIn(gBrowser.selectedTab, where, 1);
  }
}

function BrowserBack(aEvent) {
  let where = whereToOpenLink(aEvent, false, true);

  if (where == "current") {
    try {
      gBrowser.goBack();
    }
    catch(ex) {
    }
  }
  else {
    duplicateTabIn(gBrowser.selectedTab, where, -1);
  }
}

function BrowserHandleBackspace()
{
  switch (gPrefService.getIntPref("browser.backspace_action")) {
  case 0:
    BrowserBack();
    break;
  case 1:
    goDoCommand("cmd_scrollPageUp");
    break;
  }
}

function BrowserHandleShiftBackspace()
{
  switch (gPrefService.getIntPref("browser.backspace_action")) {
  case 0:
    BrowserForward();
    break;
  case 1:
    goDoCommand("cmd_scrollPageDown");
    break;
  }
}

function BrowserStop() {
  const stopFlags = nsIWebNavigation.STOP_ALL;
  gBrowser.webNavigation.stop(stopFlags);
}

function BrowserReloadOrDuplicate(aEvent) {
  var backgroundTabModifier = aEvent.button == 1 ||
#ifdef XP_MACOSX
    aEvent.metaKey;
#else
    aEvent.ctrlKey;
#endif
  if (aEvent.shiftKey && !backgroundTabModifier) {
    BrowserReloadSkipCache();
    return;
  }

  let where = whereToOpenLink(aEvent, false, true);
  if (where == "current")
    BrowserReload();
  else
    duplicateTabIn(gBrowser.selectedTab, where);
}

function BrowserReload() {
  const reloadFlags = nsIWebNavigation.LOAD_FLAGS_NONE;
  BrowserReloadWithFlags(reloadFlags);
}

function BrowserReloadSkipCache() {
  // Bypass proxy and cache.
  const reloadFlags = nsIWebNavigation.LOAD_FLAGS_BYPASS_PROXY | nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE;
  BrowserReloadWithFlags(reloadFlags);
}

var BrowserHome = BrowserGoHome;
function BrowserGoHome(aEvent) {
  if (aEvent && "button" in aEvent &&
      aEvent.button == 2) // right-click: do nothing
    return;

  var homePage = gHomeButton.getHomePage();
  var where = whereToOpenLink(aEvent, false, true);
  var urls;

  // Home page should open in a new tab when current tab is an app tab
  if (where == "current" &&
      gBrowser &&
      gBrowser.selectedTab.pinned)
    where = "tab";

  // openUILinkIn in utilityOverlay.js doesn't handle loading multiple pages
  switch (where) {
  case "current":
    loadOneOrMoreURIs(homePage);
    break;
  case "tabshifted":
  case "tab":
    urls = homePage.split("|");
    var loadInBackground = getBoolPref("browser.tabs.loadBookmarksInBackground", false);
    gBrowser.loadTabs(urls, loadInBackground);
    break;
  case "window":
    OpenBrowserWindow();
    break;
  }
}

function loadOneOrMoreURIs(aURIString)
{
#ifdef XP_MACOSX
  // we're not a browser window, pass the URI string to a new browser window
  if (window.location.href != getBrowserURL())
  {
    window.openDialog(getBrowserURL(), "_blank", "all,dialog=no", aURIString);
    return;
  }
#endif
  // This function throws for certain malformed URIs, so use exception handling
  // so that we don't disrupt startup
  try {
    gBrowser.loadTabs(aURIString.split("|"), false, true);
  }
  catch (e) {
  }
}

function focusAndSelectUrlBar() {
  if (gURLBar) {
    if (window.fullScreen)
      FullScreen.mouseoverToggle(true);

    gURLBar.select();
    if (document.activeElement == gURLBar.inputField)
      return true;
  }
  return false;
}

function openLocation() {
  if (focusAndSelectUrlBar())
    return;

#ifdef XP_MACOSX
  if (window.location.href != getBrowserURL()) {
    var win = getTopWin();
    if (win) {
      // If there's an open browser window, it should handle this command
      win.focus()
      win.openLocation();
    }
    else {
      // If there are no open browser windows, open a new one
      win = window.openDialog("chrome://browser/content/", "_blank",
                              "chrome,all,dialog=no", BROWSER_NEW_TAB_URL);
      win.addEventListener("load", openLocationCallback, false);
    }
    return;
  }
#endif
  openDialog("chrome://browser/content/openLocation.xul", "_blank",
             "chrome,modal,titlebar", window);
}

function openLocationCallback()
{
  // make sure the DOM is ready
  setTimeout(function() { this.openLocation(); }, 0);
}

function BrowserOpenTab()
{
  openUILinkIn(BROWSER_NEW_TAB_URL, "tab");
}

/* Called from the openLocation dialog. This allows that dialog to instruct
   its opener to open a new window and then step completely out of the way.
   Anything less byzantine is causing horrible crashes, rather believably,
   though oddly only on Linux. */
function delayedOpenWindow(chrome, flags, href, postData)
{
  // The other way to use setTimeout,
  // setTimeout(openDialog, 10, chrome, "_blank", flags, url),
  // doesn't work here.  The extra "magic" extra argument setTimeout adds to
  // the callback function would confuse gBrowserInit.onLoad() by making
  // window.arguments[1] be an integer instead of null.
  setTimeout(function() { openDialog(chrome, "_blank", flags, href, null, null, postData); }, 10);
}

/* Required because the tab needs time to set up its content viewers and get the load of
   the URI kicked off before becoming the active content area. */
function delayedOpenTab(aUrl, aReferrer, aCharset, aPostData, aAllowThirdPartyFixup)
{
  gBrowser.loadOneTab(aUrl, {
                      referrerURI: aReferrer,
                      charset: aCharset,
                      postData: aPostData,
                      inBackground: false,
                      allowThirdPartyFixup: aAllowThirdPartyFixup});
}

var gLastOpenDirectory = {
  _lastDir: null,
  get path() {
    if (!this._lastDir || !this._lastDir.exists()) {
      try {
        this._lastDir = gPrefService.getComplexValue("browser.open.lastDir",
                                                     Ci.nsILocalFile);
        if (!this._lastDir.exists())
          this._lastDir = null;
      }
      catch(e) {}
    }
    return this._lastDir;
  },
  set path(val) {
    try {
      if (!val || !val.isDirectory())
        return;
    } catch(e) {
      return;
    }
    this._lastDir = val.clone();

    // Don't save the last open directory pref inside the Private Browsing mode
    if (!PrivateBrowsingUtils.isWindowPrivate(window))
      gPrefService.setComplexValue("browser.open.lastDir", Ci.nsILocalFile,
                                   this._lastDir);
  },
  reset: function() {
    this._lastDir = null;
  }
};

function BrowserOpenFileWindow()
{
  // Get filepicker component.
  try {
    const nsIFilePicker = Ci.nsIFilePicker;
    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
    let fpCallback = function fpCallback_done(aResult) {
      if (aResult == nsIFilePicker.returnOK) {
        try {
          if (fp.file) {
            gLastOpenDirectory.path =
              fp.file.parent.QueryInterface(Ci.nsILocalFile);
          }
        } catch (ex) {
        }
        openUILinkIn(fp.fileURL.spec, "current");
      }
    };

    fp.init(window, gNavigatorBundle.getString("openFile"),
            nsIFilePicker.modeOpen);
    fp.appendFilters(nsIFilePicker.filterAll | nsIFilePicker.filterText |
                     nsIFilePicker.filterImages | nsIFilePicker.filterXML |
                     nsIFilePicker.filterHTML);
    fp.displayDirectory = gLastOpenDirectory.path;
    fp.open(fpCallback);
  } catch (ex) {
  }
}

function BrowserCloseTabOrWindow() {
#ifdef XP_MACOSX
  // If we're not a browser window, just close the window
  if (window.location.href != getBrowserURL()) {
    closeWindow(true);
    return;
  }
#endif

  // If the current tab is the last one, this will close the window.
  gBrowser.removeCurrentTab({animate: true});
}

function BrowserTryToCloseWindow()
{
  if (WindowIsClosing())
    window.close();     // WindowIsClosing does all the necessary checks
}

function loadURI(uri, referrer, postData, allowThirdPartyFixup) {
  if (postData === undefined)
    postData = null;

  var flags = nsIWebNavigation.LOAD_FLAGS_NONE;
  if (allowThirdPartyFixup)
    flags |= nsIWebNavigation.LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;

  try {
    gBrowser.loadURIWithFlags(uri, flags, referrer, null, postData);
  } catch (e) {}
}

function getShortcutOrURI(aURL, aPostDataRef, aMayInheritPrincipal) {
  // Initialize outparam to false
  if (aMayInheritPrincipal)
    aMayInheritPrincipal.value = false;

  var shortcutURL = null;
  var keyword = aURL;
  var param = "";

  var offset = aURL.indexOf(" ");
  if (offset > 0) {
    keyword = aURL.substr(0, offset);
    param = aURL.substr(offset + 1);
  }

  if (!aPostDataRef)
    aPostDataRef = {};

  var engine = Services.search.getEngineByAlias(keyword);
  if (engine) {
    var submission = engine.getSubmission(param);
    aPostDataRef.value = submission.postData;
    return submission.uri.spec;
  }

  [shortcutURL, aPostDataRef.value] =
    PlacesUtils.getURLAndPostDataForKeyword(keyword);

  if (!shortcutURL)
    return aURL;

  var postData = "";
  if (aPostDataRef.value)
    postData = unescape(aPostDataRef.value);

  if (/%s/i.test(shortcutURL) || /%s/i.test(postData)) {
    var charset = "";
    const re = /^(.*)\&mozcharset=([a-zA-Z][_\-a-zA-Z0-9]+)\s*$/;
    var matches = shortcutURL.match(re);
    if (matches)
      [, shortcutURL, charset] = matches;
    else {
      // Try to get the saved character-set.
      try {
        // makeURI throws if URI is invalid.
        // Will return an empty string if character-set is not found.
        charset = PlacesUtils.history.getCharsetForURI(makeURI(shortcutURL));
      } catch (e) {}
    }

    // encodeURIComponent produces UTF-8, and cannot be used for other charsets.
    // escape() works in those cases, but it doesn't uri-encode +, @, and /.
    // Therefore we need to manually replace these ASCII characters by their
    // encodeURIComponent result, to match the behavior of nsEscape() with
    // url_XPAlphas
    var encodedParam = "";
    if (charset && charset != "UTF-8")
      encodedParam = escape(convertFromUnicode(charset, param)).
                     replace(/[+@\/]+/g, encodeURIComponent);
    else // Default charset is UTF-8
      encodedParam = encodeURIComponent(param);

    shortcutURL = shortcutURL.replace(/%s/g, encodedParam).replace(/%S/g, param);

    if (/%s/i.test(postData)) // POST keyword
      aPostDataRef.value = getPostDataStream(postData, param, encodedParam,
                                             "application/x-www-form-urlencoded");
  }
  else if (param) {
    // This keyword doesn't take a parameter, but one was provided. Just return
    // the original URL.
    aPostDataRef.value = null;

    return aURL;
  }

  // This URL came from a bookmark, so it's safe to let it inherit the current
  // document's principal.
  if (aMayInheritPrincipal)
    aMayInheritPrincipal.value = true;

  return shortcutURL;
}

function getPostDataStream(aStringData, aKeyword, aEncKeyword, aType) {
  var dataStream = Cc["@mozilla.org/io/string-input-stream;1"].
                   createInstance(Ci.nsIStringInputStream);
  aStringData = aStringData.replace(/%s/g, aEncKeyword).replace(/%S/g, aKeyword);
  dataStream.data = aStringData;

  var mimeStream = Cc["@mozilla.org/network/mime-input-stream;1"].
                   createInstance(Ci.nsIMIMEInputStream);
  mimeStream.addHeader("Content-Type", aType);
  mimeStream.addContentLength = true;
  mimeStream.setData(dataStream);
  return mimeStream.QueryInterface(Ci.nsIInputStream);
}

function getLoadContext() {
  return window.QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIWebNavigation)
               .QueryInterface(Ci.nsILoadContext);
}

function readFromClipboard()
{
  var url;

  try {
    // Get clipboard.
    var clipboard = Components.classes["@mozilla.org/widget/clipboard;1"]
                              .getService(Components.interfaces.nsIClipboard);

    // Create transferable that will transfer the text.
    var trans = Components.classes["@mozilla.org/widget/transferable;1"]
                          .createInstance(Components.interfaces.nsITransferable);
    trans.init(getLoadContext());

    trans.addDataFlavor("text/unicode");

    // If available, use selection clipboard, otherwise global one
    if (clipboard.supportsSelectionClipboard())
      clipboard.getData(trans, clipboard.kSelectionClipboard);
    else
      clipboard.getData(trans, clipboard.kGlobalClipboard);

    var data = {};
    var dataLen = {};
    trans.getTransferData("text/unicode", data, dataLen);

    if (data) {
      data = data.value.QueryInterface(Components.interfaces.nsISupportsString);
      url = data.data.substring(0, dataLen.value / 2);
    }
  } catch (ex) {
  }

  return url;
}

function BrowserViewSourceOfDocument(aDocument)
{
  var pageCookie;
  var webNav;

  // Get the document charset
  var docCharset = "charset=" + aDocument.characterSet;

  // Get the nsIWebNavigation associated with the document
  try {
      var win;
      var ifRequestor;

      // Get the DOMWindow for the requested document.  If the DOMWindow
      // cannot be found, then just use the content window...
      //
      // XXX:  This is a bit of a hack...
      win = aDocument.defaultView;
      if (win == window) {
        win = content;
      }
      ifRequestor = win.QueryInterface(Components.interfaces.nsIInterfaceRequestor);

      webNav = ifRequestor.getInterface(nsIWebNavigation);
  } catch(err) {
      // If nsIWebNavigation cannot be found, just get the one for the whole
      // window...
      webNav = gBrowser.webNavigation;
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

  top.gViewSourceUtils.viewSource(webNav.currentURI.spec, pageCookie, aDocument);
}

// doc - document to use for source, or null for this window's document
// initialTab - name of the initial tab to display, or null for the first tab
// imageElement - image to load in the Media Tab of the Page Info window; can be null/omitted
function BrowserPageInfo(doc, initialTab, imageElement) {
  var args = {doc: doc, initialTab: initialTab, imageElement: imageElement};
  var windows = Cc['@mozilla.org/appshell/window-mediator;1']
                  .getService(Ci.nsIWindowMediator)
                  .getEnumerator("Browser:page-info");

  var documentURL = doc ? doc.location : window.content.document.location;

  // Check for windows matching the url
  while (windows.hasMoreElements()) {
    var currentWindow = windows.getNext();
    if (currentWindow.document.documentElement.getAttribute("relatedUrl") == documentURL) {
      currentWindow.focus();
      currentWindow.resetPageInfo(args);
      return currentWindow;
    }
  }

  // We didn't find a matching window, so open a new one.
  return openDialog("chrome://browser/content/pageinfo/pageInfo.xul", "",
                    "chrome,toolbar,dialog=no,resizable", args);
}

function URLBarSetURI(aURI) {
  var value = gBrowser.userTypedValue;
  var valid = false;

  if (value == null) {
    let uri = aURI || gBrowser.currentURI;

    // Replace initial page URIs with an empty string
    // only if there's no opener (bug 370555).
    if (gInitialPages.indexOf(uri.spec) != -1)
      value = content.opener ? uri.spec : "";
    else
      value = losslessDecodeURI(uri);

    valid = !isBlankPageURL(uri.spec);
  }

  gURLBar.value = value;
  gURLBar.valueIsTyped = !valid;
  SetPageProxyState(valid ? "valid" : "invalid");
}

function losslessDecodeURI(aURI) {
  var value = aURI.spec;
  // Try to decode as UTF-8 if there's no encoding sequence that we would break.
  if (!/%25(?:3B|2F|3F|3A|40|26|3D|2B|24|2C|23)/i.test(value))
    try {
      value = decodeURI(value)
                // 1. decodeURI decodes %25 to %, which creates unintended
                //    encoding sequences. Re-encode it, unless it's part of
                //    a sequence that survived decodeURI, i.e. one for:
                //    ';', '/', '?', ':', '@', '&', '=', '+', '$', ',', '#'
                //    (RFC 3987 section 3.2)
                // 2. Re-encode whitespace so that it doesn't get eaten away
                //    by the location bar (bug 410726).
                .replace(/%(?!3B|2F|3F|3A|40|26|3D|2B|24|2C|23)|[\r\n\t]/ig,
                         encodeURIComponent);
    } catch (e) {}

  // Encode invisible characters (line and paragraph separator,
  // object replacement character) (bug 452979)
  value = value.replace(/[\v\x0c\x1c\x1d\x1e\x1f\u2028\u2029\ufffc]/g,
                        encodeURIComponent);

  // Encode default ignorable characters (bug 546013)
  // except ZWNJ (U+200C) and ZWJ (U+200D) (bug 582186).
  // This includes all bidirectional formatting characters.
  // (RFC 3987 sections 3.2 and 4.1 paragraph 6)
  value = value.replace(/[\u00ad\u034f\u115f-\u1160\u17b4-\u17b5\u180b-\u180d\u200b\u200e-\u200f\u202a-\u202e\u2060-\u206f\u3164\ufe00-\ufe0f\ufeff\uffa0\ufff0-\ufff8]|\ud834[\udd73-\udd7a]|[\udb40-\udb43][\udc00-\udfff]/g,
                        encodeURIComponent);
  return value;
}

function UpdateUrlbarSearchSplitterState()
{
  var splitter = document.getElementById("urlbar-search-splitter");
  var urlbar = document.getElementById("urlbar-container");
  var searchbar = document.getElementById("search-container");
  var stop = document.getElementById("stop-button");

  var ibefore = null;
  if (urlbar && searchbar) {
    if (urlbar.nextSibling == searchbar ||
        urlbar.getAttribute("combined") &&
        stop && stop.nextSibling == searchbar)
      ibefore = searchbar;
    else if (searchbar.nextSibling == urlbar)
      ibefore = urlbar;
  }

  if (ibefore) {
    if (!splitter) {
      splitter = document.createElement("splitter");
      splitter.id = "urlbar-search-splitter";
      splitter.setAttribute("resizebefore", "flex");
      splitter.setAttribute("resizeafter", "flex");
      splitter.setAttribute("skipintoolbarset", "true");
      splitter.className = "chromeclass-toolbar-additional";
    }
    urlbar.parentNode.insertBefore(splitter, ibefore);
  } else if (splitter)
    splitter.parentNode.removeChild(splitter);
}

function setUrlAndSearchBarWidthForConditionalForwardButton() {
  // Workaround for bug 694084: Showing/hiding the conditional forward button resizes
  // the search bar when the url/search bar splitter hasn't been used.
  var urlbarContainer = document.getElementById("urlbar-container");
  var searchbarContainer = document.getElementById("search-container");
  if (!urlbarContainer ||
      !searchbarContainer ||
      urlbarContainer.hasAttribute("width") ||
      searchbarContainer.hasAttribute("width") ||
      urlbarContainer.parentNode != searchbarContainer.parentNode)
    return;
  urlbarContainer.style.width = searchbarContainer.style.width = "";
  var urlbarWidth = urlbarContainer.clientWidth;
  var searchbarWidth = searchbarContainer.clientWidth;
  urlbarContainer.style.width = urlbarWidth + "px";
  searchbarContainer.style.width = searchbarWidth + "px";
}

function UpdatePageProxyState()
{
  if (gURLBar && gURLBar.value != gLastValidURLStr)
    SetPageProxyState("invalid");
}

function SetPageProxyState(aState)
{
  if (!gURLBar)
    return;

  if (!gProxyFavIcon)
    gProxyFavIcon = document.getElementById("page-proxy-favicon");

  gURLBar.setAttribute("pageproxystate", aState);
  gProxyFavIcon.setAttribute("pageproxystate", aState);

  // the page proxy state is set to valid via OnLocationChange, which
  // gets called when we switch tabs.
  if (aState == "valid") {
    gLastValidURLStr = gURLBar.value;
    gURLBar.addEventListener("input", UpdatePageProxyState, false);
  } else if (aState == "invalid") {
    gURLBar.removeEventListener("input", UpdatePageProxyState, false);
  }
}

function PageProxyClickHandler(aEvent)
{
  if (aEvent.button == 1 && gPrefService.getBoolPref("middlemouse.paste"))
    middleMousePaste(aEvent);
}

/**
 *  Handle load of some pages (about:*) so that we can make modifications
 *  to the DOM for unprivileged pages.
 */
function BrowserOnAboutPageLoad(document) {
  if (/^about:home$/i.test(document.documentURI)) {
    // XXX bug 738646 - when Marketplace is launched, remove this statement and
    // the hidden attribute set on the apps button in aboutHome.xhtml
    if (getBoolPref("browser.aboutHome.apps", false))
      document.getElementById("apps").removeAttribute("hidden");

    let ss = Components.classes["@mozilla.org/browser/sessionstore;1"].
             getService(Components.interfaces.nsISessionStore);
    if (ss.canRestoreLastSession)
      document.getElementById("launcher").setAttribute("session", "true");

    // Inject search engine and snippets URL.
    let docElt = document.documentElement;
    docElt.setAttribute("snippetsURL", AboutHomeUtils.snippetsURL);
    docElt.setAttribute("searchEngineName",
                        AboutHomeUtils.defaultSearchEngine.name);
    docElt.setAttribute("searchEngineURL",
                        AboutHomeUtils.defaultSearchEngine.searchURL);
  }
}

/**
 * Handle command events bubbling up from error page content
 */
let BrowserOnClick = {
  handleEvent: function BrowserOnClick_handleEvent(aEvent) {
    if (!aEvent.isTrusted || // Don't trust synthetic events
        aEvent.button == 2 || aEvent.target.localName != "button") {
      return;
    }

    let originalTarget = aEvent.originalTarget;
    let ownerDoc = originalTarget.ownerDocument;

    // If the event came from an ssl error page, it is probably either the "Add
    // Exception…" or "Get me out of here!" button
    if (ownerDoc.documentURI.startsWith("about:certerror")) {
      this.onAboutCertError(originalTarget, ownerDoc);
    }
    else if (ownerDoc.documentURI.startsWith("about:blocked")) {
      this.onAboutBlocked(originalTarget, ownerDoc);
    }
    else if (ownerDoc.documentURI.startsWith("about:neterror")) {
      this.onAboutNetError(originalTarget, ownerDoc);
    }
    else if (/^about:home$/i.test(ownerDoc.documentURI)) {
      this.onAboutHome(originalTarget, ownerDoc);
    }
  },

  onAboutCertError: function BrowserOnClick_onAboutCertError(aTargetElm, aOwnerDoc) {
    let elmId = aTargetElm.getAttribute("id");
    let secHistogram = Cc["@mozilla.org/base/telemetry;1"].
                        getService(Ci.nsITelemetry).
                        getHistogramById("SECURITY_UI");

    switch (elmId) {
      case "exceptionDialogButton":
        secHistogram.add(Ci.nsISecurityUITelemetry.WARNING_BAD_CERT_CLICK_ADD_EXCEPTION);
        let params = { exceptionAdded : false };

        try {
          switch (Services.prefs.getIntPref("browser.ssl_override_behavior")) {
            case 2 : // Pre-fetch & pre-populate
              params.prefetchCert = true;
            case 1 : // Pre-populate
              params.location = aOwnerDoc.location.href;
          }
        } catch (e) {
          Components.utils.reportError("Couldn't get ssl_override pref: " + e);
        }

        window.openDialog('chrome://pippki/content/exceptionDialog.xul',
                          '','chrome,centerscreen,modal', params);

        // If the user added the exception cert, attempt to reload the page
        if (params.exceptionAdded) {
          aOwnerDoc.location.reload();
        }
        break;

      case "getMeOutOfHereButton":
        secHistogram.add(Ci.nsISecurityUITelemetry.WARNING_BAD_CERT_GET_ME_OUT_OF_HERE);
        getMeOutOfHere();
        break;

      case "technicalContent":
        secHistogram.add(Ci.nsISecurityUITelemetry.WARNING_BAD_CERT_TECHNICAL_DETAILS);
        break;

      case "expertContent":
        secHistogram.add(Ci.nsISecurityUITelemetry.WARNING_BAD_CERT_UNDERSTAND_RISKS);
        break;

    }
  },

  onAboutBlocked: function BrowserOnClick_onAboutBlocked(aTargetElm, aOwnerDoc) {
    let elmId = aTargetElm.getAttribute("id");
    let secHistogram = Cc["@mozilla.org/base/telemetry;1"].
                        getService(Ci.nsITelemetry).
                        getHistogramById("SECURITY_UI");

    // The event came from a button on a malware/phishing block page
    // First check whether it's malware or phishing, so that we can
    // use the right strings/links
    let isMalware = /e=malwareBlocked/.test(aOwnerDoc.documentURI);
    let bucketName = isMalware ? "WARNING_MALWARE_PAGE_":"WARNING_PHISHING_PAGE_";
    let nsISecTel = Ci.nsISecurityUITelemetry;

    switch (elmId) {
      case "getMeOutButton":
        secHistogram.add(nsISecTel[bucketName + "GET_ME_OUT_OF_HERE"]);
        getMeOutOfHere();
        break;

      case "reportButton":
        // This is the "Why is this site blocked" button.  For malware,
        // we can fetch a site-specific report, for phishing, we redirect
        // to the generic page describing phishing protection.

        // We log even if malware/phishing info URL couldn't be found: 
        // the measurement is for how many users clicked the WHY BLOCKED button
        secHistogram.add(nsISecTel[bucketName + "WHY_BLOCKED"]);

        if (isMalware) {
          // Get the stop badware "why is this blocked" report url,
          // append the current url, and go there.
          try {
            let reportURL = formatURL("browser.safebrowsing.malware.reportURL", true);
            reportURL += aOwnerDoc.location.href;
            content.location = reportURL;
          } catch (e) {
            Components.utils.reportError("Couldn't get malware report URL: " + e);
          }
        }
        else { // It's a phishing site, not malware
          try {
            content.location = formatURL("browser.safebrowsing.warning.infoURL", true);
          } catch (e) {
            Components.utils.reportError("Couldn't get phishing info URL: " + e);
          }
        }
        break;

      case "ignoreWarningButton":
        secHistogram.add(nsISecTel[bucketName + "IGNORE_WARNING"]);
        this.ignoreWarningButton(isMalware);
        break;
    }
  },

  ignoreWarningButton: function BrowserOnClick_ignoreWarningButton(aIsMalware) {
    // Allow users to override and continue through to the site,
    // but add a notify bar as a reminder, so that they don't lose
    // track after, e.g., tab switching.
    gBrowser.loadURIWithFlags(content.location.href,
                              nsIWebNavigation.LOAD_FLAGS_BYPASS_CLASSIFIER,
                              null, null, null);

    Services.perms.add(makeURI(content.location.href), "safe-browsing",
                       Ci.nsIPermissionManager.ALLOW_ACTION,
                       Ci.nsIPermissionManager.EXPIRE_SESSION);

    let buttons = [{
      label: gNavigatorBundle.getString("safebrowsing.getMeOutOfHereButton.label"),
      accessKey: gNavigatorBundle.getString("safebrowsing.getMeOutOfHereButton.accessKey"),
      callback: function() { getMeOutOfHere(); }
    }];

    let title;
    if (aIsMalware) {
      title = gNavigatorBundle.getString("safebrowsing.reportedAttackSite");
      buttons[1] = {
        label: gNavigatorBundle.getString("safebrowsing.notAnAttackButton.label"),
        accessKey: gNavigatorBundle.getString("safebrowsing.notAnAttackButton.accessKey"),
        callback: function() {
          openUILinkIn(gSafeBrowsing.getReportURL('MalwareError'), 'tab');
        }
      };
    } else {
      title = gNavigatorBundle.getString("safebrowsing.reportedWebForgery");
      buttons[1] = {
        label: gNavigatorBundle.getString("safebrowsing.notAForgeryButton.label"),
        accessKey: gNavigatorBundle.getString("safebrowsing.notAForgeryButton.accessKey"),
        callback: function() {
          openUILinkIn(gSafeBrowsing.getReportURL('Error'), 'tab');
        }
      };
    }

    let notificationBox = gBrowser.getNotificationBox();
    let value = "blocked-badware-page";

    let previousNotification = notificationBox.getNotificationWithValue(value);
    if (previousNotification) {
      notificationBox.removeNotification(previousNotification);
    }

    let notification = notificationBox.appendNotification(
      title,
      value,
      "chrome://global/skin/icons/blacklist_favicon.png",
      notificationBox.PRIORITY_CRITICAL_HIGH,
      buttons
    );
    // Persist the notification until the user removes so it
    // doesn't get removed on redirects.
    notification.persistence = -1;
  },

  onAboutNetError: function BrowserOnClick_onAboutNetError(aTargetElm, aOwnerDoc) {
    let elmId = aTargetElm.getAttribute("id");
    if (elmId != "errorTryAgain" || !/e=netOffline/.test(aOwnerDoc.documentURI))
      return;
    Services.io.offline = false;
  },

  onAboutHome: function BrowserOnClick_onAboutHome(aTargetElm, aOwnerDoc) {
    let elmId = aTargetElm.getAttribute("id");

    switch (elmId) {
      case "restorePreviousSession":
        let ss = Cc["@mozilla.org/browser/sessionstore;1"].
                 getService(Ci.nsISessionStore);
        if (ss.canRestoreLastSession) {
          ss.restoreLastSession();
        }
        aOwnerDoc.getElementById("launcher").removeAttribute("session");
        break;

      case "downloads":
        BrowserDownloadsUI();
        break;

      case "bookmarks":
        PlacesCommandHook.showPlacesOrganizer("AllBookmarks");
        break;

      case "history":
        PlacesCommandHook.showPlacesOrganizer("History");
        break;

      case "apps":
        openUILinkIn("https://marketplace.mozilla.org/", "tab");
        break;

      case "addons":
        BrowserOpenAddonsMgr();
        break;

      case "sync":
        openPreferences("paneSync");
        break;

      case "settings":
        openPreferences();
        break;
    }
  },
};

/**
 * Re-direct the browser to a known-safe page.  This function is
 * used when, for example, the user browses to a known malware page
 * and is presented with about:blocked.  The "Get me out of here!"
 * button should take the user to the default start page so that even
 * when their own homepage is infected, we can get them somewhere safe.
 */
function getMeOutOfHere() {
  // Get the start page from the *default* pref branch, not the user's
  var prefs = Cc["@mozilla.org/preferences-service;1"]
             .getService(Ci.nsIPrefService).getDefaultBranch(null);
  var url = BROWSER_NEW_TAB_URL;
  try {
    url = prefs.getComplexValue("browser.startup.homepage",
                                Ci.nsIPrefLocalizedString).data;
    // If url is a pipe-delimited set of pages, just take the first one.
    if (url.contains("|"))
      url = url.split("|")[0];
  } catch(e) {
    Components.utils.reportError("Couldn't get homepage pref: " + e);
  }
  content.location = url;
}

function BrowserFullScreen()
{
  window.fullScreen = !window.fullScreen;
}

function onFullScreen(event) {
  FullScreen.toggle(event);
}

function onMozEnteredDomFullscreen(event) {
  FullScreen.enterDomFullscreen(event);
}

function getWebNavigation()
{
  return gBrowser.webNavigation;
}

function BrowserReloadWithFlags(reloadFlags) {
  /* First, we'll try to use the session history object to reload so
   * that framesets are handled properly. If we're in a special
   * window (such as view-source) that has no session history, fall
   * back on using the web navigation's reload method.
   */

  var webNav = gBrowser.webNavigation;
  try {
    var sh = webNav.sessionHistory;
    if (sh)
      webNav = sh.QueryInterface(nsIWebNavigation);
  } catch (e) {
  }

  try {
    webNav.reload(reloadFlags);
  } catch (e) {
  }
}

var PrintPreviewListener = {
  _printPreviewTab: null,
  _tabBeforePrintPreview: null,

  getPrintPreviewBrowser: function () {
    if (!this._printPreviewTab) {
      this._tabBeforePrintPreview = gBrowser.selectedTab;
      this._printPreviewTab = gBrowser.loadOneTab("about:blank",
                                                  { inBackground: false });
      gBrowser.selectedTab = this._printPreviewTab;
    }
    return gBrowser.getBrowserForTab(this._printPreviewTab);
  },
  getSourceBrowser: function () {
    return this._tabBeforePrintPreview ?
      this._tabBeforePrintPreview.linkedBrowser : gBrowser.selectedBrowser;
  },
  getNavToolbox: function () {
    return gNavToolbox;
  },
  onEnter: function () {
    gInPrintPreviewMode = true;
    this._toggleAffectedChrome();
  },
  onExit: function () {
    gBrowser.selectedTab = this._tabBeforePrintPreview;
    this._tabBeforePrintPreview = null;
    gInPrintPreviewMode = false;
    this._toggleAffectedChrome();
    gBrowser.removeTab(this._printPreviewTab);
    this._printPreviewTab = null;
  },
  _toggleAffectedChrome: function () {
    gNavToolbox.collapsed = gInPrintPreviewMode;

    if (gInPrintPreviewMode)
      this._hideChrome();
    else
      this._showChrome();

    if (this._chromeState.sidebarOpen)
      toggleSidebar(this._sidebarCommand);

#ifdef MENUBAR_CAN_AUTOHIDE
    updateAppButtonDisplay();
#endif
  },
  _hideChrome: function () {
    this._chromeState = {};

    var sidebar = document.getElementById("sidebar-box");
    this._chromeState.sidebarOpen = !sidebar.hidden;
    this._sidebarCommand = sidebar.getAttribute("sidebarcommand");

    var notificationBox = gBrowser.getNotificationBox();
    this._chromeState.notificationsOpen = !notificationBox.notificationsHidden;
    notificationBox.notificationsHidden = true;

    document.getElementById("sidebar").setAttribute("src", "about:blank");
    var addonBar = document.getElementById("addon-bar");
    this._chromeState.addonBarOpen = !addonBar.collapsed;
    addonBar.collapsed = true;
    gBrowser.updateWindowResizers();

    this._chromeState.findOpen = gFindBarInitialized && !gFindBar.hidden;
    if (gFindBarInitialized)
      gFindBar.close();

    this._chromeState.syncNotificationsOpen = false;
    var syncNotifications = document.getElementById("sync-notifications");
    if (syncNotifications) {
      this._chromeState.syncNotificationsOpen = !syncNotifications.notificationsHidden;
      syncNotifications.notificationsHidden = true;
    }
  },
  _showChrome: function () {
    if (this._chromeState.notificationsOpen)
      gBrowser.getNotificationBox().notificationsHidden = false;

    if (this._chromeState.addonBarOpen) {
      document.getElementById("addon-bar").collapsed = false;
      gBrowser.updateWindowResizers();
    }

    if (this._chromeState.findOpen)
      gFindBar.open();

    if (this._chromeState.syncNotificationsOpen)
      document.getElementById("sync-notifications").notificationsHidden = false;
  }
}

function getMarkupDocumentViewer()
{
  return gBrowser.markupDocumentViewer;
}

/**
 * Content area tooltip.
 * XXX - this must move into XBL binding/equiv! Do not want to pollute
 *       browser.js with functionality that can be encapsulated into
 *       browser widget. TEMPORARY!
 *
 * NOTE: Any changes to this routine need to be mirrored in DefaultTooltipTextProvider::GetNodeText()
 *       (located in mozilla/embedding/browser/webBrowser/nsDocShellTreeOwner.cpp)
 *       which performs the same function, but for embedded clients that
 *       don't use a XUL/JS layer. It is important that the logic of
 *       these two routines be kept more or less in sync.
 *       (pinkerton)
 **/
function FillInHTMLTooltip(tipElement)
{
  var retVal = false;
  // Don't show the tooltip if the tooltip node is a document or disconnected.
  if (!tipElement.ownerDocument ||
      (tipElement.ownerDocument.compareDocumentPosition(tipElement) & document.DOCUMENT_POSITION_DISCONNECTED))
    return retVal;

  const XLinkNS = "http://www.w3.org/1999/xlink";
  const XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

  var titleText = null;
  var XLinkTitleText = null;
  var SVGTitleText = null;
  var lookingForSVGTitle = true;
  var direction = tipElement.ownerDocument.dir;

  // If the element is invalid per HTML5 Forms specifications and has no title,
  // show the constraint validation error message.
  if ((tipElement instanceof HTMLInputElement ||
       tipElement instanceof HTMLTextAreaElement ||
       tipElement instanceof HTMLSelectElement ||
       tipElement instanceof HTMLButtonElement) &&
      !tipElement.hasAttribute('title') &&
      (!tipElement.form || !tipElement.form.noValidate)) {
    // If the element is barred from constraint validation or valid,
    // the validation message will be the empty string.
    titleText = tipElement.validationMessage;
  }

  while (!titleText && !XLinkTitleText && !SVGTitleText && tipElement) {
    if (tipElement.nodeType == Node.ELEMENT_NODE &&
        tipElement.namespaceURI != XULNS) {
      titleText = tipElement.getAttribute("title");
      if ((tipElement instanceof HTMLAnchorElement ||
           tipElement instanceof HTMLAreaElement ||
           tipElement instanceof HTMLLinkElement ||
           tipElement instanceof SVGAElement) && tipElement.href) {
        XLinkTitleText = tipElement.getAttributeNS(XLinkNS, "title");
      }
      if (lookingForSVGTitle &&
          (!(tipElement instanceof SVGElement) ||
           tipElement.parentNode.nodeType == Node.DOCUMENT_NODE)) {
        lookingForSVGTitle = false;
      }
      if (lookingForSVGTitle) {
        for (let childNode of tipElement.childNodes) {
          if (childNode instanceof SVGTitleElement) {
            SVGTitleText = childNode.textContent;
            break;
          }
        }
      }
      var defView = tipElement.ownerDocument.defaultView;
      // XXX Work around bug 350679:
      // "Tooltips can be fired in documents with no view".
      if (!defView)
        return retVal;
      direction = defView.getComputedStyle(tipElement, "")
        .getPropertyValue("direction");
    }
    tipElement = tipElement.parentNode;
  }

  var tipNode = document.getElementById("aHTMLTooltip");
  tipNode.style.direction = direction;

  [titleText, XLinkTitleText, SVGTitleText].forEach(function (t) {
    if (t && /\S/.test(t)) {
      // Make CRLF and CR render one line break each.
      t = t.replace(/\r\n?/g, '\n');

      tipNode.setAttribute("label", t);
      retVal = true;
    }
  });
  return retVal;
}

var browserDragAndDrop = {
  canDropLink: function (aEvent) Services.droppedLinkHandler.canDropLink(aEvent, true),

  dragOver: function (aEvent)
  {
    if (this.canDropLink(aEvent)) {
      aEvent.preventDefault();
    }
  },

  drop: function (aEvent, aName, aDisallowInherit) {
    return Services.droppedLinkHandler.dropLink(aEvent, aName, aDisallowInherit);
  }
};

var homeButtonObserver = {
  onDrop: function (aEvent)
    {
      // disallow setting home pages that inherit the principal
      let url = browserDragAndDrop.drop(aEvent, {}, true);
      setTimeout(openHomeDialog, 0, url);
    },

  onDragOver: function (aEvent)
    {
      browserDragAndDrop.dragOver(aEvent);
      aEvent.dropEffect = "link";
    },
  onDragExit: function (aEvent)
    {
    }
}

function openHomeDialog(aURL)
{
  var promptTitle = gNavigatorBundle.getString("droponhometitle");
  var promptMsg   = gNavigatorBundle.getString("droponhomemsg");
  var pressedVal  = Services.prompt.confirmEx(window, promptTitle, promptMsg,
                          Services.prompt.STD_YES_NO_BUTTONS,
                          null, null, null, null, {value:0});

  if (pressedVal == 0) {
    try {
      var str = Components.classes["@mozilla.org/supports-string;1"]
                          .createInstance(Components.interfaces.nsISupportsString);
      str.data = aURL;
      gPrefService.setComplexValue("browser.startup.homepage",
                                   Components.interfaces.nsISupportsString, str);
    } catch (ex) {
      dump("Failed to set the home page.\n"+ex+"\n");
    }
  }
}

var bookmarksButtonObserver = {
  onDrop: function (aEvent)
  {
    let name = { };
    let url = browserDragAndDrop.drop(aEvent, name);
    try {
      PlacesUIUtils.showBookmarkDialog({ action: "add"
                                       , type: "bookmark"
                                       , uri: makeURI(url)
                                       , title: name
                                       , hiddenRows: [ "description"
                                                     , "location"
                                                     , "loadInSidebar"
                                                     , "keyword" ]
                                       }, window);
    } catch(ex) { }
  },

  onDragOver: function (aEvent)
  {
    browserDragAndDrop.dragOver(aEvent);
    aEvent.dropEffect = "link";
  },

  onDragExit: function (aEvent)
  {
  }
}

var newTabButtonObserver = {
  onDragOver: function (aEvent)
  {
    browserDragAndDrop.dragOver(aEvent);
  },

  onDragExit: function (aEvent)
  {
  },

  onDrop: function (aEvent)
  {
    let url = browserDragAndDrop.drop(aEvent, { });
    var postData = {};
    url = getShortcutOrURI(url, postData);
    if (url) {
      // allow third-party services to fixup this URL
      openNewTabWith(url, null, postData.value, aEvent, true);
    }
  }
}

var newWindowButtonObserver = {
  onDragOver: function (aEvent)
  {
    browserDragAndDrop.dragOver(aEvent);
  },
  onDragExit: function (aEvent)
  {
  },
  onDrop: function (aEvent)
  {
    let url = browserDragAndDrop.drop(aEvent, { });
    var postData = {};
    url = getShortcutOrURI(url, postData);
    if (url) {
      // allow third-party services to fixup this URL
      openNewWindowWith(url, null, postData.value, true);
    }
  }
}

const DOMLinkHandler = {
  handleEvent: function (event) {
    switch (event.type) {
      case "DOMLinkAdded":
        this.onLinkAdded(event);
        break;
    }
  },
  getLinkIconURI: function(aLink) {
    let targetDoc = aLink.ownerDocument;
    var uri = makeURI(aLink.href, targetDoc.characterSet);

    // Verify that the load of this icon is legal.
    // Some error or special pages can load their favicon.
    // To be on the safe side, only allow chrome:// favicons.
    var isAllowedPage = [
      /^about:neterror\?/,
      /^about:blocked\?/,
      /^about:certerror\?/,
      /^about:home$/,
    ].some(function (re) re.test(targetDoc.documentURI));

    if (!isAllowedPage || !uri.schemeIs("chrome")) {
      var ssm = Cc["@mozilla.org/scriptsecuritymanager;1"].
                getService(Ci.nsIScriptSecurityManager);
      try {
        ssm.checkLoadURIWithPrincipal(targetDoc.nodePrincipal, uri,
                                      Ci.nsIScriptSecurityManager.DISALLOW_SCRIPT);
      } catch(e) {
        return null;
      }
    }

    try {
      var contentPolicy = Cc["@mozilla.org/layout/content-policy;1"].
                          getService(Ci.nsIContentPolicy);
    } catch(e) {
      return null; // Refuse to load if we can't do a security check.
    }

    // Security says okay, now ask content policy
    if (contentPolicy.shouldLoad(Ci.nsIContentPolicy.TYPE_IMAGE,
                                 uri, targetDoc.documentURIObject,
                                 aLink, aLink.type, null)
                                 != Ci.nsIContentPolicy.ACCEPT)
      return null;
    
    try {
      uri.userPass = "";
    } catch(e) {
      // some URIs are immutable
    }
    return uri;
  },
  onLinkAdded: function (event) {
    var link = event.originalTarget;
    var rel = link.rel && link.rel.toLowerCase();
    if (!link || !link.ownerDocument || !rel || !link.href)
      return;

    var feedAdded = false;
    var iconAdded = false;
    var searchAdded = false;
    var rels = {};
    for (let relString of rel.split(/\s+/))
      rels[relString] = true;

    for (let relVal in rels) {
      switch (relVal) {
        case "feed":
        case "alternate":
          if (!feedAdded) {
            if (!rels.feed && rels.alternate && rels.stylesheet)
              break;

            if (isValidFeed(link, link.ownerDocument.nodePrincipal, rels.feed)) {
              FeedHandler.addFeed(link, link.ownerDocument);
              feedAdded = true;
            }
          }
          break;
        case "icon":
          if (!iconAdded) {
            if (!gPrefService.getBoolPref("browser.chrome.site_icons"))
              break;

            var uri = this.getLinkIconURI(link);
            if (!uri)
              break;

            if (gBrowser.isFailedIcon(uri))
              break;

            var browserIndex = gBrowser.getBrowserIndexForDocument(link.ownerDocument);
            // no browser? no favicon.
            if (browserIndex == -1)
              break;

            let tab = gBrowser.tabs[browserIndex];
            gBrowser.setIcon(tab, uri.spec);
            iconAdded = true;
          }
          break;
        case "search":
          if (!searchAdded) {
            var type = link.type && link.type.toLowerCase();
            type = type.replace(/^\s+|\s*(?:;.*)?$/g, "");

            if (type == "application/opensearchdescription+xml" && link.title &&
                /^(?:https?|ftp):/i.test(link.href) &&
                !PrivateBrowsingUtils.isWindowPrivate(window)) {
              var engine = { title: link.title, href: link.href };
              BrowserSearch.addEngine(engine, link.ownerDocument);
              searchAdded = true;
            }
          }
          break;
      }
    }
  }
}

const BrowserSearch = {
  addEngine: function(engine, targetDoc) {
    if (!this.searchBar)
      return;

    var browser = gBrowser.getBrowserForDocument(targetDoc);
    // ignore search engines from subframes (see bug 479408)
    if (!browser)
      return;

    // Check to see whether we've already added an engine with this title
    if (browser.engines) {
      if (browser.engines.some(function (e) e.title == engine.title))
        return;
    }

    // Append the URI and an appropriate title to the browser data.
    // Use documentURIObject in the check for shouldLoadFavIcon so that we
    // do the right thing with about:-style error pages.  Bug 453442
    var iconURL = null;
    if (gBrowser.shouldLoadFavIcon(targetDoc.documentURIObject))
      iconURL = targetDoc.documentURIObject.prePath + "/favicon.ico";

    var hidden = false;
    // If this engine (identified by title) is already in the list, add it
    // to the list of hidden engines rather than to the main list.
    // XXX This will need to be changed when engines are identified by URL;
    // see bug 335102.
    if (Services.search.getEngineByName(engine.title))
      hidden = true;

    var engines = (hidden ? browser.hiddenEngines : browser.engines) || [];

    engines.push({ uri: engine.href,
                   title: engine.title,
                   icon: iconURL });

    if (hidden)
      browser.hiddenEngines = engines;
    else
      browser.engines = engines;
  },

  /**
   * Gives focus to the search bar, if it is present on the toolbar, or loads
   * the default engine's search form otherwise. For Mac, opens a new window
   * or focuses an existing window, if necessary.
   */
  webSearch: function BrowserSearch_webSearch() {
#ifdef XP_MACOSX
    if (window.location.href != getBrowserURL()) {
      var win = getTopWin();
      if (win) {
        // If there's an open browser window, it should handle this command
        win.focus();
        win.BrowserSearch.webSearch();
      } else {
        // If there are no open browser windows, open a new one
        var observer = function observer(subject, topic, data) {
          if (subject == win) {
            BrowserSearch.webSearch();
            Services.obs.removeObserver(observer, "browser-delayed-startup-finished");
          }
        }
        win = window.openDialog(getBrowserURL(), "_blank",
                                "chrome,all,dialog=no", "about:blank");
        Services.obs.addObserver(observer, "browser-delayed-startup-finished", false);
      }
      return;
    }
#endif
    var searchBar = this.searchBar;
    if (searchBar && window.fullScreen)
      FullScreen.mouseoverToggle(true);
    if (searchBar)
      searchBar.select();
    if (!searchBar || document.activeElement != searchBar.textbox.inputField)
      openUILinkIn(Services.search.defaultEngine.searchForm, "current");
  },

  /**
   * Loads a search results page, given a set of search terms. Uses the current
   * engine if the search bar is visible, or the default engine otherwise.
   *
   * @param searchText
   *        The search terms to use for the search.
   *
   * @param useNewTab
   *        Boolean indicating whether or not the search should load in a new
   *        tab.
   *
   * @param responseType [optional]
   *        The MIME type that we'd like to receive in response
   *        to this submission.  If null or the the response type is not supported
   *        for the search engine, will fallback to "text/html".
   */
  loadSearch: function BrowserSearch_search(searchText, useNewTab, responseType) {
    var engine;

    // If the search bar is visible, use the current engine, otherwise, fall
    // back to the default engine.
    if (isElementVisible(this.searchBar))
      engine = Services.search.currentEngine;
    else
      engine = Services.search.defaultEngine;

    var submission = engine.getSubmission(searchText, responseType);

    // If a response type was specified and getSubmission returned null,
    // fallback to the default response type.
    if (!submission && responseType)
      submission = engine.getSubmission(searchText);

    // getSubmission can return null if the engine doesn't have a URL
    // with a text/html response type.  This is unlikely (since
    // SearchService._addEngineToStore() should fail for such an engine),
    // but let's be on the safe side.
    if (!submission)
      return;

    let inBackground = Services.prefs.getBoolPref("browser.search.context.loadInBackground");
    openLinkIn(submission.uri.spec,
               useNewTab ? "tab" : "current",
               { postData: submission.postData,
                 inBackground: inBackground,
                 relatedToCurrent: true });
  },

  /**
   * Returns the search bar element if it is present in the toolbar, null otherwise.
   */
  get searchBar() {
    return document.getElementById("searchbar");
  },

  loadAddEngines: function BrowserSearch_loadAddEngines() {
    var newWindowPref = gPrefService.getIntPref("browser.link.open_newwindow");
    var where = newWindowPref == 3 ? "tab" : "window";
    var searchEnginesURL = formatURL("browser.search.searchEnginesURL", true);
    openUILinkIn(searchEnginesURL, where);
  }
}

function FillHistoryMenu(aParent) {
  // Lazily add the hover listeners on first showing and never remove them
  if (!aParent.hasStatusListener) {
    // Show history item's uri in the status bar when hovering, and clear on exit
    aParent.addEventListener("DOMMenuItemActive", function(aEvent) {
      // Only the current page should have the checked attribute, so skip it
      if (!aEvent.target.hasAttribute("checked"))
        XULBrowserWindow.setOverLink(aEvent.target.getAttribute("uri"));
    }, false);
    aParent.addEventListener("DOMMenuItemInactive", function() {
      XULBrowserWindow.setOverLink("");
    }, false);

    aParent.hasStatusListener = true;
  }

  // Remove old entries if any
  var children = aParent.childNodes;
  for (var i = children.length - 1; i >= 0; --i) {
    if (children[i].hasAttribute("index"))
      aParent.removeChild(children[i]);
  }

  var webNav = gBrowser.webNavigation;
  var sessionHistory = webNav.sessionHistory;

  var count = sessionHistory.count;
  if (count <= 1) // don't display the popup for a single item
    return false;

  const MAX_HISTORY_MENU_ITEMS = 15;
  var index = sessionHistory.index;
  var half_length = Math.floor(MAX_HISTORY_MENU_ITEMS / 2);
  var start = Math.max(index - half_length, 0);
  var end = Math.min(start == 0 ? MAX_HISTORY_MENU_ITEMS : index + half_length + 1, count);
  if (end == count)
    start = Math.max(count - MAX_HISTORY_MENU_ITEMS, 0);

  var tooltipBack = gNavigatorBundle.getString("tabHistory.goBack");
  var tooltipCurrent = gNavigatorBundle.getString("tabHistory.current");
  var tooltipForward = gNavigatorBundle.getString("tabHistory.goForward");

  for (var j = end - 1; j >= start; j--) {
    let item = document.createElement("menuitem");
    let entry = sessionHistory.getEntryAtIndex(j, false);
    let uri = entry.URI.spec;

    item.setAttribute("uri", uri);
    item.setAttribute("label", entry.title || uri);
    item.setAttribute("index", j);

    if (j != index) {
      PlacesUtils.favicons.getFaviconURLForPage(entry.URI, function (aURI) {
        if (aURI) {
          let iconURL = PlacesUtils.favicons.getFaviconLinkForIcon(aURI).spec;
          item.style.listStyleImage = "url(" + iconURL + ")";
        }
      });
    }

    if (j < index) {
      item.className = "unified-nav-back menuitem-iconic menuitem-with-favicon";
      item.setAttribute("tooltiptext", tooltipBack);
    } else if (j == index) {
      item.setAttribute("type", "radio");
      item.setAttribute("checked", "true");
      item.className = "unified-nav-current";
      item.setAttribute("tooltiptext", tooltipCurrent);
    } else {
      item.className = "unified-nav-forward menuitem-iconic menuitem-with-favicon";
      item.setAttribute("tooltiptext", tooltipForward);
    }

    aParent.appendChild(item);
  }
  return true;
}

function addToUrlbarHistory(aUrlToAdd) {
  if (!PrivateBrowsingUtils.isWindowPrivate(window) &&
      aUrlToAdd &&
      !aUrlToAdd.contains(" ") &&
      !/[\x00-\x1F]/.test(aUrlToAdd))
    PlacesUIUtils.markPageAsTyped(aUrlToAdd);
}

function toJavaScriptConsole()
{
  toOpenWindowByType("global:console", "chrome://global/content/console.xul");
}

function BrowserDownloadsUI()
{
  Cc["@mozilla.org/download-manager-ui;1"].
  getService(Ci.nsIDownloadManagerUI).show(window);
}

function toOpenWindowByType(inType, uri, features)
{
  var topWindow = Services.wm.getMostRecentWindow(inType);

  if (topWindow)
    topWindow.focus();
  else if (features)
    window.open(uri, "_blank", features);
  else
    window.open(uri, "_blank", "chrome,extrachrome,menubar,resizable,scrollbars,status,toolbar");
}

function OpenBrowserWindow(options)
{
  var telemetryObj = {};
  TelemetryStopwatch.start("FX_NEW_WINDOW_MS", telemetryObj);

  function newDocumentShown(doc, topic, data) {
    if (topic == "document-shown" &&
        doc != document &&
        doc.defaultView == win) {
      Services.obs.removeObserver(newDocumentShown, "document-shown");
      TelemetryStopwatch.finish("FX_NEW_WINDOW_MS", telemetryObj);
    }
  };
  Services.obs.addObserver(newDocumentShown, "document-shown", false);

  var charsetArg = new String();
  var handler = Components.classes["@mozilla.org/browser/clh;1"]
                          .getService(Components.interfaces.nsIBrowserHandler);
  var defaultArgs = handler.defaultArgs;
  var wintype = document.documentElement.getAttribute('windowtype');

  var extraFeatures = "";
  var forcePrivate = false;
#ifdef MOZ_PER_WINDOW_PRIVATE_BROWSING
  forcePrivate = typeof options == "object" && "private" in options && options.private;
#else
  forcePrivate = gPrivateBrowsingUI.privateBrowsingEnabled;
#endif

  if (forcePrivate) {
    extraFeatures = ",private";
    // Force the new window to load about:privatebrowsing instead of the default home page
    defaultArgs = "about:privatebrowsing";
  }

  // if and only if the current window is a browser window and it has a document with a character
  // set, then extract the current charset menu setting from the current document and use it to
  // initialize the new browser window...
  var win;
  if (window && (wintype == "navigator:browser") && window.content && window.content.document)
  {
    var DocCharset = window.content.document.characterSet;
    charsetArg = "charset="+DocCharset;

    //we should "inherit" the charset menu setting in a new window
    win = window.openDialog("chrome://browser/content/", "_blank", "chrome,all,dialog=no" + extraFeatures, defaultArgs, charsetArg);
  }
  else // forget about the charset information.
  {
    win = window.openDialog("chrome://browser/content/", "_blank", "chrome,all,dialog=no" + extraFeatures, defaultArgs);
  }

  return win;
}

var gCustomizeSheet = false;
function BrowserCustomizeToolbar() {
  // Disable the toolbar context menu items
  var menubar = document.getElementById("main-menubar");
  for (let childNode of menubar.childNodes)
    childNode.setAttribute("disabled", true);

  var cmd = document.getElementById("cmd_CustomizeToolbars");
  cmd.setAttribute("disabled", "true");

  var splitter = document.getElementById("urlbar-search-splitter");
  if (splitter)
    splitter.parentNode.removeChild(splitter);

  CombinedStopReload.uninit();

  PlacesToolbarHelper.customizeStart();
  BookmarksMenuButton.customizeStart();
  DownloadsButton.customizeStart();

  TabsInTitlebar.allowedBy("customizing-toolbars", false);

  var customizeURL = "chrome://global/content/customizeToolbar.xul";
  gCustomizeSheet = getBoolPref("toolbar.customization.usesheet", false);

  if (gCustomizeSheet) {
    let sheetFrame = document.createElement("iframe");
    let panel = document.getElementById("customizeToolbarSheetPopup");
    sheetFrame.id = "customizeToolbarSheetIFrame";
    sheetFrame.toolbox = gNavToolbox;
    sheetFrame.panel = panel;
    sheetFrame.setAttribute("style", panel.getAttribute("sheetstyle"));
    panel.appendChild(sheetFrame);

    // Open the panel, but make it invisible until the iframe has loaded so
    // that the user doesn't see a white flash.
    panel.style.visibility = "hidden";
    gNavToolbox.addEventListener("beforecustomization", function onBeforeCustomization() {
      gNavToolbox.removeEventListener("beforecustomization", onBeforeCustomization, false);
      panel.style.removeProperty("visibility");
    }, false);

    sheetFrame.setAttribute("src", customizeURL);

    panel.openPopup(gNavToolbox, "after_start", 0, 0);
  } else {
    window.openDialog(customizeURL,
                      "CustomizeToolbar",
                      "chrome,titlebar,toolbar,location,resizable,dependent",
                      gNavToolbox);
  }
}

function BrowserToolboxCustomizeDone(aToolboxChanged) {
  if (gCustomizeSheet) {
    document.getElementById("customizeToolbarSheetPopup").hidePopup();
    let iframe = document.getElementById("customizeToolbarSheetIFrame");
    iframe.parentNode.removeChild(iframe);
  }

  // Update global UI elements that may have been added or removed
  if (aToolboxChanged) {
    gURLBar = document.getElementById("urlbar");

    gProxyFavIcon = document.getElementById("page-proxy-favicon");
    gHomeButton.updateTooltip();
    gIdentityHandler._cacheElements();
    window.XULBrowserWindow.init();

#ifndef XP_MACOSX
    updateEditUIVisibility();
#endif

    // Hacky: update the PopupNotifications' object's reference to the iconBox,
    // if it already exists, since it may have changed if the URL bar was
    // added/removed.
    if (!__lookupGetter__("PopupNotifications"))
      PopupNotifications.iconBox = document.getElementById("notification-popup-box");
  }

  PlacesToolbarHelper.customizeDone();
  BookmarksMenuButton.customizeDone();
  DownloadsButton.customizeDone();

  // The url bar splitter state is dependent on whether stop/reload
  // and the location bar are combined, so we need this ordering
  CombinedStopReload.init();
  UpdateUrlbarSearchSplitterState();
  setUrlAndSearchBarWidthForConditionalForwardButton();

  // Update the urlbar
  if (gURLBar) {
    URLBarSetURI();
    XULBrowserWindow.asyncUpdateUI();
    PlacesStarButton.updateState();
    SocialShareButton.updateShareState();
  }

  TabsInTitlebar.allowedBy("customizing-toolbars", true);

  // Re-enable parts of the UI we disabled during the dialog
  var menubar = document.getElementById("main-menubar");
  for (let childNode of menubar.childNodes)
    childNode.setAttribute("disabled", false);
  var cmd = document.getElementById("cmd_CustomizeToolbars");
  cmd.removeAttribute("disabled");

  // make sure to re-enable click-and-hold
  if (!getBoolPref("ui.click_hold_context_menus", false))
    SetClickAndHoldHandlers();

  gBrowser.selectedBrowser.focus();
}

function BrowserToolboxCustomizeChange(aType) {
  switch (aType) {
    case "iconsize":
    case "mode":
      retrieveToolbarIconsizesFromTheme();
      break;
    default:
      gHomeButton.updatePersonalToolbarStyle();
      BookmarksMenuButton.customizeChange();
      allTabs.readPref();
  }
}

/**
 * Allows themes to override the "iconsize" attribute on toolbars.
 */
function retrieveToolbarIconsizesFromTheme() {
  function retrieveToolbarIconsize(aToolbar) {
    if (aToolbar.localName != "toolbar")
      return;

    // The theme indicates that it wants to override the "iconsize" attribute
    // by specifying a special value for the "counter-reset" property on the
    // toolbar. A custom property cannot be used because getComputedStyle can
    // only return the values of standard CSS properties.
    let counterReset = getComputedStyle(aToolbar).counterReset;
    if (counterReset == "smallicons 0")
      aToolbar.setAttribute("iconsize", "small");
    else if (counterReset == "largeicons 0")
      aToolbar.setAttribute("iconsize", "large");
  }

  Array.forEach(gNavToolbox.childNodes, retrieveToolbarIconsize);
  gNavToolbox.externalToolbars.forEach(retrieveToolbarIconsize);
}

/**
 * Update the global flag that tracks whether or not any edit UI (the Edit menu,
 * edit-related items in the context menu, and edit-related toolbar buttons
 * is visible, then update the edit commands' enabled state accordingly.  We use
 * this flag to skip updating the edit commands on focus or selection changes
 * when no UI is visible to improve performance (including pageload performance,
 * since focus changes when you load a new page).
 *
 * If UI is visible, we use goUpdateGlobalEditMenuItems to set the commands'
 * enabled state so the UI will reflect it appropriately.
 *
 * If the UI isn't visible, we enable all edit commands so keyboard shortcuts
 * still work and just lazily disable them as needed when the user presses a
 * shortcut.
 *
 * This doesn't work on Mac, since Mac menus flash when users press their
 * keyboard shortcuts, so edit UI is essentially always visible on the Mac,
 * and we need to always update the edit commands.  Thus on Mac this function
 * is a no op.
 */
function updateEditUIVisibility()
{
#ifndef XP_MACOSX
  let editMenuPopupState = document.getElementById("menu_EditPopup").state;
  let contextMenuPopupState = document.getElementById("contentAreaContextMenu").state;
  let placesContextMenuPopupState = document.getElementById("placesContext").state;
#ifdef MENUBAR_CAN_AUTOHIDE
  let appMenuPopupState = document.getElementById("appmenu-popup").state;
#endif

  // The UI is visible if the Edit menu is opening or open, if the context menu
  // is open, or if the toolbar has been customized to include the Cut, Copy,
  // or Paste toolbar buttons.
  gEditUIVisible = editMenuPopupState == "showing" ||
                   editMenuPopupState == "open" ||
                   contextMenuPopupState == "showing" ||
                   contextMenuPopupState == "open" ||
                   placesContextMenuPopupState == "showing" ||
                   placesContextMenuPopupState == "open" ||
#ifdef MENUBAR_CAN_AUTOHIDE
                   appMenuPopupState == "showing" ||
                   appMenuPopupState == "open" ||
#endif
                   document.getElementById("cut-button") ||
                   document.getElementById("copy-button") ||
                   document.getElementById("paste-button") ? true : false;

  // If UI is visible, update the edit commands' enabled state to reflect
  // whether or not they are actually enabled for the current focus/selection.
  if (gEditUIVisible)
    goUpdateGlobalEditMenuItems();

  // Otherwise, enable all commands, so that keyboard shortcuts still work,
  // then lazily determine their actual enabled state when the user presses
  // a keyboard shortcut.
  else {
    goSetCommandEnabled("cmd_undo", true);
    goSetCommandEnabled("cmd_redo", true);
    goSetCommandEnabled("cmd_cut", true);
    goSetCommandEnabled("cmd_copy", true);
    goSetCommandEnabled("cmd_paste", true);
    goSetCommandEnabled("cmd_selectAll", true);
    goSetCommandEnabled("cmd_delete", true);
    goSetCommandEnabled("cmd_switchTextDirection", true);
  }
#endif
}

/**
 * Returns true if |aMimeType| is text-based, false otherwise.
 *
 * @param aMimeType
 *        The MIME type to check.
 *
 * If adding types to this function, please also check the similar
 * function in findbar.xml
 */
function mimeTypeIsTextBased(aMimeType)
{
  return /^text\/|\+xml$/.test(aMimeType) ||
         aMimeType == "application/x-javascript" ||
         aMimeType == "application/javascript" ||
         aMimeType == "application/xml" ||
         aMimeType == "mozilla.application/cached-xul";
}

var XULBrowserWindow = {
  // Stored Status, Link and Loading values
  status: "",
  defaultStatus: "",
  jsStatus: "",
  jsDefaultStatus: "",
  overLink: "",
  startTime: 0,
  statusText: "",
  isBusy: false,
  inContentWhitelist: ["about:addons", "about:permissions",
                       "about:sync-progress", "about:preferences"],

  QueryInterface: function (aIID) {
    if (aIID.equals(Ci.nsIWebProgressListener) ||
        aIID.equals(Ci.nsIWebProgressListener2) ||
        aIID.equals(Ci.nsISupportsWeakReference) ||
        aIID.equals(Ci.nsIXULBrowserWindow) ||
        aIID.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_NOINTERFACE;
  },

  get stopCommand () {
    delete this.stopCommand;
    return this.stopCommand = document.getElementById("Browser:Stop");
  },
  get reloadCommand () {
    delete this.reloadCommand;
    return this.reloadCommand = document.getElementById("Browser:Reload");
  },
  get statusTextField () {
    delete this.statusTextField;
    return this.statusTextField = document.getElementById("statusbar-display");
  },
  get isImage () {
    delete this.isImage;
    return this.isImage = document.getElementById("isImage");
  },
  get _uriFixup () {
    delete this._uriFixup;
    return this._uriFixup = Cc["@mozilla.org/docshell/urifixup;1"]
                              .getService(Ci.nsIURIFixup);
  },

  init: function () {
    this.throbberElement = document.getElementById("navigator-throbber");

#ifdef MOZ_E10S_COMPAT
    // Bug 666809 - SecurityUI support for e10s
#else
    // Initialize the security button's state and tooltip text.  Remember to reset
    // _hostChanged, otherwise onSecurityChange will short circuit.
    var securityUI = gBrowser.securityUI;
    this._hostChanged = true;
    this.onSecurityChange(null, null, securityUI.state);
#endif
  },

  destroy: function () {
    // XXXjag to avoid leaks :-/, see bug 60729
    delete this.throbberElement;
    delete this.stopCommand;
    delete this.reloadCommand;
    delete this.statusTextField;
    delete this.statusText;
  },

  setJSStatus: function (status) {
    this.jsStatus = status;
    this.updateStatusField();
  },

  setJSDefaultStatus: function (status) {
    this.jsDefaultStatus = status;
    this.updateStatusField();
  },

  setDefaultStatus: function (status) {
    this.defaultStatus = status;
    this.updateStatusField();
  },

  setOverLink: function (url, anchorElt) {
    // Encode bidirectional formatting characters.
    // (RFC 3987 sections 3.2 and 4.1 paragraph 6)
    url = url.replace(/[\u200e\u200f\u202a\u202b\u202c\u202d\u202e]/g,
                      encodeURIComponent);

    if (gURLBar && gURLBar._mayTrimURLs /* corresponds to browser.urlbar.trimURLs */)
      url = trimURL(url);

    this.overLink = url;
    LinkTargetDisplay.update();
  },

  updateStatusField: function () {
    var text, type, types = ["overLink"];
    if (this._busyUI)
      types.push("status");
    types.push("jsStatus", "jsDefaultStatus", "defaultStatus");
    for (type of types) {
      text = this[type];
      if (text)
        break;
    }

    // check the current value so we don't trigger an attribute change
    // and cause needless (slow!) UI updates
    if (this.statusText != text) {
      let field = this.statusTextField;
      field.setAttribute("previoustype", field.getAttribute("type"));
      field.setAttribute("type", type);
      field.label = text;
      field.setAttribute("crop", type == "overLink" ? "center" : "end");
      this.statusText = text;
    }
  },

  // Called before links are navigated to to allow us to retarget them if needed.
  onBeforeLinkTraversal: function(originalTarget, linkURI, linkNode, isAppTab) {
    let target = this._onBeforeLinkTraversal(originalTarget, linkURI, linkNode, isAppTab);
    SocialUI.closeSocialPanelForLinkTraversal(target, linkNode);
    return target;
  },

  _onBeforeLinkTraversal: function(originalTarget, linkURI, linkNode, isAppTab) {
    // Don't modify non-default targets or targets that aren't in top-level app
    // tab docshells (isAppTab will be false for app tab subframes).
    if (originalTarget != "" || !isAppTab)
      return originalTarget;

    // External links from within app tabs should always open in new tabs
    // instead of replacing the app tab's page (Bug 575561)
    let linkHost;
    let docHost;
    try {
      linkHost = linkURI.host;
      docHost = linkNode.ownerDocument.documentURIObject.host;
    } catch(e) {
      // nsIURI.host can throw for non-nsStandardURL nsIURIs.
      // If we fail to get either host, just return originalTarget.
      return originalTarget;
    }

    if (docHost == linkHost)
      return originalTarget;

    // Special case: ignore "www" prefix if it is part of host string
    let [longHost, shortHost] =
      linkHost.length > docHost.length ? [linkHost, docHost] : [docHost, linkHost];
    if (longHost == "www." + shortHost)
      return originalTarget;

    return "_blank";
  },

  onProgressChange: function (aWebProgress, aRequest,
                              aCurSelfProgress, aMaxSelfProgress,
                              aCurTotalProgress, aMaxTotalProgress) {
    // Do nothing.
  },

  onProgressChange64: function (aWebProgress, aRequest,
                                aCurSelfProgress, aMaxSelfProgress,
                                aCurTotalProgress, aMaxTotalProgress) {
    return this.onProgressChange(aWebProgress, aRequest,
      aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress,
      aMaxTotalProgress);
  },

  onStateChange: function (aWebProgress, aRequest, aStateFlags, aStatus) {
    const nsIWebProgressListener = Ci.nsIWebProgressListener;
    const nsIChannel = Ci.nsIChannel;

    if (aStateFlags & nsIWebProgressListener.STATE_START &&
        aStateFlags & nsIWebProgressListener.STATE_IS_NETWORK) {

      if (aRequest && aWebProgress.DOMWindow == content)
        this.startDocumentLoad(aRequest);

      this.isBusy = true;

      if (!(aStateFlags & nsIWebProgressListener.STATE_RESTORING)) {
        this._busyUI = true;

        // Turn the throbber on.
        if (this.throbberElement)
          this.throbberElement.setAttribute("busy", "true");

        // XXX: This needs to be based on window activity...
        this.stopCommand.removeAttribute("disabled");
        CombinedStopReload.switchToStop();
      }
    }
    else if (aStateFlags & nsIWebProgressListener.STATE_STOP) {
      if (aStateFlags & nsIWebProgressListener.STATE_IS_NETWORK &&
          aWebProgress.DOMWindow == content &&
          aRequest)
        this.endDocumentLoad(aRequest, aStatus);

      // This (thanks to the filter) is a network stop or the last
      // request stop outside of loading the document, stop throbbers
      // and progress bars and such
      if (aRequest) {
        let msg = "";
        let location;
        // Get the URI either from a channel or a pseudo-object
        if (aRequest instanceof nsIChannel || "URI" in aRequest) {
          location = aRequest.URI;

          // For keyword URIs clear the user typed value since they will be changed into real URIs
          if (location.scheme == "keyword" && aWebProgress.DOMWindow == content)
            gBrowser.userTypedValue = null;

          if (location.spec != "about:blank") {
            switch (aStatus) {
              case Components.results.NS_ERROR_NET_TIMEOUT:
                msg = gNavigatorBundle.getString("nv_timeout");
                break;
            }
          }
        }

        this.status = "";
        this.setDefaultStatus(msg);

        // Disable menu entries for images, enable otherwise
        if (content.document && mimeTypeIsTextBased(content.document.contentType))
          this.isImage.removeAttribute('disabled');
        else
          this.isImage.setAttribute('disabled', 'true');
      }

      this.isBusy = false;

      if (this._busyUI) {
        this._busyUI = false;

        // Turn the throbber off.
        if (this.throbberElement)
          this.throbberElement.removeAttribute("busy");

        this.stopCommand.setAttribute("disabled", "true");
        CombinedStopReload.switchToReload(aRequest instanceof Ci.nsIRequest);
      }
    }
  },

  onLocationChange: function (aWebProgress, aRequest, aLocationURI, aFlags) {
    const nsIWebProgressListener = Ci.nsIWebProgressListener;
    var location = aLocationURI ? aLocationURI.spec : "";
    this._hostChanged = true;

    // Hide the form invalid popup.
    if (gFormSubmitObserver.panel) {
      gFormSubmitObserver.panel.hidePopup();
    }

    if (document.tooltipNode) {
      // Optimise for the common case
      if (aWebProgress.DOMWindow == content) {
        document.getElementById("aHTMLTooltip").hidePopup();
        document.tooltipNode = null;
      }
      else {
        for (let tooltipWindow =
               document.tooltipNode.ownerDocument.defaultView;
             tooltipWindow != tooltipWindow.parent;
             tooltipWindow = tooltipWindow.parent) {
          if (tooltipWindow == aWebProgress.DOMWindow) {
            document.getElementById("aHTMLTooltip").hidePopup();
            document.tooltipNode = null;
            break;
          }
        }
      }
    }

    // This code here does not compare uris exactly when determining
    // whether or not the message should be hidden since the message
    // may be prematurely hidden when an install is invoked by a click
    // on a link that looks like this:
    //
    // <a href="#" onclick="return install();">Install Foo</a>
    //
    // - which fires a onLocationChange message to uri + '#'...
    var selectedBrowser = gBrowser.selectedBrowser;
    if (selectedBrowser.lastURI) {
      let oldSpec = selectedBrowser.lastURI.spec;
      let oldIndexOfHash = oldSpec.indexOf("#");
      if (oldIndexOfHash != -1)
        oldSpec = oldSpec.substr(0, oldIndexOfHash);
      let newSpec = location;
      let newIndexOfHash = newSpec.indexOf("#");
      if (newIndexOfHash != -1)
        newSpec = newSpec.substr(0, newSpec.indexOf("#"));
      if (newSpec != oldSpec) {
        // Remove all the notifications, except for those which want to
        // persist across the first location change.
        let nBox = gBrowser.getNotificationBox(selectedBrowser);
        nBox.removeTransientNotifications();

        // Only need to call locationChange if the PopupNotifications object
        // for this window has already been initialized (i.e. its getter no
        // longer exists)
        if (!__lookupGetter__("PopupNotifications"))
          PopupNotifications.locationChange();
      }
    }

    // Disable menu entries for images, enable otherwise
    if (content.document && mimeTypeIsTextBased(content.document.contentType))
      this.isImage.removeAttribute('disabled');
    else
      this.isImage.setAttribute('disabled', 'true');

    this.hideOverLinkImmediately = true;
    this.setOverLink("", null);
    this.hideOverLinkImmediately = false;

    // We should probably not do this if the value has changed since the user
    // searched
    // Update urlbar only if a new page was loaded on the primary content area
    // Do not update urlbar if there was a subframe navigation

    var browser = gBrowser.selectedBrowser;
    if (aWebProgress.DOMWindow == content) {
      if ((location == "about:blank" && !content.opener) ||
          location == "") {  // Second condition is for new tabs, otherwise
                             // reload function is enabled until tab is refreshed.
        this.reloadCommand.setAttribute("disabled", "true");
      } else {
        this.reloadCommand.removeAttribute("disabled");
      }

      if (gURLBar) {
        // Strip off "wyciwyg://" and passwords for the location bar
        let uri = aLocationURI;
        try {
          uri = this._uriFixup.createExposableURI(uri);
        } catch (e) {}
        URLBarSetURI(uri);

        // Update starring UI
        PlacesStarButton.updateState();
        SocialShareButton.updateShareState();
      }

      // Show or hide browser chrome based on the whitelist
      if (this.hideChromeForLocation(location)) {
        document.documentElement.setAttribute("disablechrome", "true");
      } else {
        let ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);
        if (ss.getTabValue(gBrowser.selectedTab, "appOrigin"))
          document.documentElement.setAttribute("disablechrome", "true");
        else
          document.documentElement.removeAttribute("disablechrome");
      }

      // Utility functions for disabling find
      var shouldDisableFind = function shouldDisableFind(aDocument) {
        let docElt = aDocument.documentElement;
        return docElt && docElt.getAttribute("disablefastfind") == "true";
      }

      var disableFindCommands = function disableFindCommands(aDisable) {
        let findCommands = [document.getElementById("cmd_find"),
                            document.getElementById("cmd_findAgain"),
                            document.getElementById("cmd_findPrevious")];
        for (let elt of findCommands) {
          if (aDisable)
            elt.setAttribute("disabled", "true");
          else
            elt.removeAttribute("disabled");
        }
      }

      var onContentRSChange = function onContentRSChange(e) {
        if (e.target.readyState != "interactive" && e.target.readyState != "complete")
          return;

        e.target.removeEventListener("readystate", onContentRSChange);
        disableFindCommands(shouldDisableFind(e.target));
      }

      // Disable find commands in documents that ask for them to be disabled.
      if (aLocationURI &&
          (aLocationURI.schemeIs("about") || aLocationURI.schemeIs("chrome"))) {
        // Don't need to re-enable/disable find commands for same-document location changes
        // (e.g. the replaceStates in about:addons)
        if (!(aFlags & nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT)) {
          if (content.document.readyState == "interactive" || content.document.readyState == "complete")
            disableFindCommands(shouldDisableFind(content.document));
          else {
            content.document.addEventListener("readystatechange", onContentRSChange);
          }
        }
      } else
        disableFindCommands(false);

      if (gFindBarInitialized) {
        if (gFindBar.findMode != gFindBar.FIND_NORMAL) {
          // Close the Find toolbar if we're in old-style TAF mode
          gFindBar.close();
        }

        // fix bug 253793 - turn off highlight when page changes
        gFindBar.getElement("highlight").checked = false;
      }
    }
    UpdateBackForwardCommands(gBrowser.webNavigation);

    // See bug 358202, when tabs are switched during a drag operation,
    // timers don't fire on windows (bug 203573)
    if (aRequest)
      setTimeout(function () { XULBrowserWindow.asyncUpdateUI(); }, 0);
    else
      this.asyncUpdateUI();
  },

  asyncUpdateUI: function () {
    FeedHandler.updateFeeds();
  },

  hideChromeForLocation: function(aLocation) {
    aLocation = aLocation.toLowerCase();
    return this.inContentWhitelist.some(function(aSpec) {
      return aSpec == aLocation;
    });
  },

  onStatusChange: function (aWebProgress, aRequest, aStatus, aMessage) {
    this.status = aMessage;
    this.updateStatusField();
  },

  // Properties used to cache security state used to update the UI
  _state: null,
  _hostChanged: false, // onLocationChange will flip this bit

  onSecurityChange: function (aWebProgress, aRequest, aState) {
    // Don't need to do anything if the data we use to update the UI hasn't
    // changed
    if (this._state == aState &&
        !this._hostChanged) {
#ifdef DEBUG
      try {
        var contentHost = gBrowser.contentWindow.location.host;
        if (this._host !== undefined && this._host != contentHost) {
            Components.utils.reportError(
              "ASSERTION: browser.js host is inconsistent. Content window has " +
              "<" + contentHost + "> but cached host is <" + this._host + ">.\n"
            );
        }
      } catch (ex) {}
#endif
      return;
    }
    this._state = aState;

#ifdef DEBUG
    try {
      this._host = gBrowser.contentWindow.location.host;
    } catch(ex) {
      this._host = null;
    }
#endif

    this._hostChanged = false;

    // aState is defined as a bitmask that may be extended in the future.
    // We filter out any unknown bits before testing for known values.
    const wpl = Components.interfaces.nsIWebProgressListener;
    const wpl_security_bits = wpl.STATE_IS_SECURE |
                              wpl.STATE_IS_BROKEN |
                              wpl.STATE_IS_INSECURE;
    var level;

    switch (this._state & wpl_security_bits) {
      case wpl.STATE_IS_SECURE:
        level = "high";
        break;
      case wpl.STATE_IS_BROKEN:
        level = "broken";
        break;
    }

    if (level) {
      // We don't style the Location Bar based on the the 'level' attribute
      // anymore, but still set it for third-party themes.
      if (gURLBar)
        gURLBar.setAttribute("level", level);
    } else {
      if (gURLBar)
        gURLBar.removeAttribute("level");
    }

    // Don't pass in the actual location object, since it can cause us to
    // hold on to the window object too long.  Just pass in the fields we
    // care about. (bug 424829)
    var location = gBrowser.contentWindow.location;
    var locationObj = {};
    try {
      // about:blank can be used by webpages so pretend it is http
      locationObj.protocol = location == "about:blank" ? "http:" : location.protocol;
      locationObj.host = location.host;
      locationObj.hostname = location.hostname;
      locationObj.port = location.port;
    } catch (ex) {
      // Can sometimes throw if the URL being visited has no host/hostname,
      // e.g. about:blank. The _state for these pages means we won't need these
      // properties anyways, though.
    }
    gIdentityHandler.checkIdentity(this._state, locationObj);
  },

  // simulate all change notifications after switching tabs
  onUpdateCurrentBrowser: function XWB_onUpdateCurrentBrowser(aStateFlags, aStatus, aMessage, aTotalProgress) {
    if (FullZoom.updateBackgroundTabs)
      FullZoom.onLocationChange(gBrowser.currentURI, true);
    var nsIWebProgressListener = Components.interfaces.nsIWebProgressListener;
    var loadingDone = aStateFlags & nsIWebProgressListener.STATE_STOP;
    // use a pseudo-object instead of a (potentially nonexistent) channel for getting
    // a correct error message - and make sure that the UI is always either in
    // loading (STATE_START) or done (STATE_STOP) mode
    this.onStateChange(
      gBrowser.webProgress,
      { URI: gBrowser.currentURI },
      loadingDone ? nsIWebProgressListener.STATE_STOP : nsIWebProgressListener.STATE_START,
      aStatus
    );
    // status message and progress value are undefined if we're done with loading
    if (loadingDone)
      return;
    this.onStatusChange(gBrowser.webProgress, null, 0, aMessage);
  },

  startDocumentLoad: function XWB_startDocumentLoad(aRequest) {
    // clear out feed data
    gBrowser.selectedBrowser.feeds = null;

    // clear out search-engine data
    gBrowser.selectedBrowser.engines = null;

    var uri = aRequest.QueryInterface(Ci.nsIChannel).URI;
    try {
      Services.obs.notifyObservers(content, "StartDocumentLoad", uri.spec);
    } catch (e) {
    }
  },

  endDocumentLoad: function XWB_endDocumentLoad(aRequest, aStatus) {
    var urlStr = aRequest.QueryInterface(Ci.nsIChannel).originalURI.spec;

    var notification = Components.isSuccessCode(aStatus) ? "EndDocumentLoad" : "FailDocumentLoad";
    try {
      Services.obs.notifyObservers(content, notification, urlStr);
    } catch (e) {
    }
  }
};

var LinkTargetDisplay = {
  get DELAY_SHOW() {
     delete this.DELAY_SHOW;
     return this.DELAY_SHOW = Services.prefs.getIntPref("browser.overlink-delay");
  },

  DELAY_HIDE: 250,
  _timer: 0,

  get _isVisible () XULBrowserWindow.statusTextField.label != "",

  update: function () {
    clearTimeout(this._timer);
    window.removeEventListener("mousemove", this, true);

    if (!XULBrowserWindow.overLink) {
      if (XULBrowserWindow.hideOverLinkImmediately)
        this._hide();
      else
        this._timer = setTimeout(this._hide.bind(this), this.DELAY_HIDE);
      return;
    }

    if (this._isVisible) {
      XULBrowserWindow.updateStatusField();
    } else {
      // Let the display appear when the mouse doesn't move within the delay
      this._showDelayed();
      window.addEventListener("mousemove", this, true);
    }
  },

  handleEvent: function (event) {
    switch (event.type) {
      case "mousemove":
        // Restart the delay since the mouse was moved
        clearTimeout(this._timer);
        this._showDelayed();
        break;
    }
  },

  _showDelayed: function () {
    this._timer = setTimeout(function (self) {
      XULBrowserWindow.updateStatusField();
      window.removeEventListener("mousemove", self, true);
    }, this.DELAY_SHOW, this);
  },

  _hide: function () {
    clearTimeout(this._timer);

    XULBrowserWindow.updateStatusField();
  }
};

var CombinedStopReload = {
  init: function () {
    if (this._initialized)
      return;

    var urlbar = document.getElementById("urlbar-container");
    var reload = document.getElementById("reload-button");
    var stop = document.getElementById("stop-button");

    if (urlbar) {
      if (urlbar.parentNode.getAttribute("mode") != "icons" ||
          !reload || urlbar.nextSibling != reload ||
          !stop || reload.nextSibling != stop)
        urlbar.removeAttribute("combined");
      else {
        urlbar.setAttribute("combined", "true");
        reload = document.getElementById("urlbar-reload-button");
        stop = document.getElementById("urlbar-stop-button");
      }
    }
    if (!stop || !reload || reload.nextSibling != stop)
      return;

    this._initialized = true;
    if (XULBrowserWindow.stopCommand.getAttribute("disabled") != "true")
      reload.setAttribute("displaystop", "true");
    stop.addEventListener("click", this, false);
    this.reload = reload;
    this.stop = stop;
  },

  uninit: function () {
    if (!this._initialized)
      return;

    this._cancelTransition();
    this._initialized = false;
    this.stop.removeEventListener("click", this, false);
    this.reload = null;
    this.stop = null;
  },

  handleEvent: function (event) {
    // the only event we listen to is "click" on the stop button
    if (event.button == 0 &&
        !this.stop.disabled)
      this._stopClicked = true;
  },

  switchToStop: function () {
    if (!this._initialized)
      return;

    this._cancelTransition();
    this.reload.setAttribute("displaystop", "true");
  },

  switchToReload: function (aDelay) {
    if (!this._initialized)
      return;

    this.reload.removeAttribute("displaystop");

    if (!aDelay || this._stopClicked) {
      this._stopClicked = false;
      this._cancelTransition();
      this.reload.disabled = XULBrowserWindow.reloadCommand
                                             .getAttribute("disabled") == "true";
      return;
    }

    if (this._timer)
      return;

    // Temporarily disable the reload button to prevent the user from
    // accidentally reloading the page when intending to click the stop button
    this.reload.disabled = true;
    this._timer = setTimeout(function (self) {
      self._timer = 0;
      self.reload.disabled = XULBrowserWindow.reloadCommand
                                             .getAttribute("disabled") == "true";
    }, 650, this);
  },

  _cancelTransition: function () {
    if (this._timer) {
      clearTimeout(this._timer);
      this._timer = 0;
    }
  }
};

var TabsProgressListener = {
  onStateChange: function (aBrowser, aWebProgress, aRequest, aStateFlags, aStatus) {
#ifdef MOZ_CRASHREPORTER
    if (aRequest instanceof Ci.nsIChannel &&
        aStateFlags & Ci.nsIWebProgressListener.STATE_START &&
        aStateFlags & Ci.nsIWebProgressListener.STATE_IS_DOCUMENT &&
        gCrashReporter.enabled) {
      gCrashReporter.annotateCrashReport("URL", aRequest.URI.spec);
    }
#endif

    // Collect telemetry data about tab load times.
    if (aWebProgress.DOMWindow == aWebProgress.DOMWindow.top &&
        aStateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW) {
      if (aStateFlags & Ci.nsIWebProgressListener.STATE_START)
        TelemetryStopwatch.start("FX_PAGE_LOAD_MS", aBrowser);
      else if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP)
        TelemetryStopwatch.finish("FX_PAGE_LOAD_MS", aBrowser);
    }

    // Attach a listener to watch for "click" events bubbling up from error
    // pages and other similar page. This lets us fix bugs like 401575 which
    // require error page UI to do privileged things, without letting error
    // pages have any privilege themselves.
    // We can't look for this during onLocationChange since at that point the
    // document URI is not yet the about:-uri of the error page.

    if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
        Components.isSuccessCode(aStatus) &&
        aWebProgress.DOMWindow.document.documentURI.startsWith("about:")) {
      aBrowser.addEventListener("click", BrowserOnClick, true);
      aBrowser.addEventListener("pagehide", function onPageHide(event) {
        if (event.target.defaultView.frameElement)
          return;
        aBrowser.removeEventListener("click", BrowserOnClick, true);
        aBrowser.removeEventListener("pagehide", onPageHide, true);
      }, true);

      // We also want to make changes to page UI for unprivileged about pages.
      BrowserOnAboutPageLoad(aWebProgress.DOMWindow.document);
    }
  },

  onLocationChange: function (aBrowser, aWebProgress, aRequest, aLocationURI,
                              aFlags) {
    // Filter out any sub-frame loads
    if (aBrowser.contentWindow == aWebProgress.DOMWindow) {
      // Filter out any onLocationChanges triggered by anchor navigation
      // or history.push/pop/replaceState.
      if (aRequest) {
        // Initialize the click-to-play state.
        aBrowser._clickToPlayDoorhangerShown = false;
        aBrowser._clickToPlayPluginsActivated = false;
      }
      FullZoom.onLocationChange(aLocationURI, false, aBrowser);
    }
  },

  onRefreshAttempted: function (aBrowser, aWebProgress, aURI, aDelay, aSameURI) {
    if (gPrefService.getBoolPref("accessibility.blockautorefresh")) {
      let brandBundle = document.getElementById("bundle_brand");
      let brandShortName = brandBundle.getString("brandShortName");
      let refreshButtonText =
        gNavigatorBundle.getString("refreshBlocked.goButton");
      let refreshButtonAccesskey =
        gNavigatorBundle.getString("refreshBlocked.goButton.accesskey");
      let message =
        gNavigatorBundle.getFormattedString(aSameURI ? "refreshBlocked.refreshLabel"
                                                     : "refreshBlocked.redirectLabel",
                                            [brandShortName]);
      let docShell = aWebProgress.DOMWindow
                                 .QueryInterface(Ci.nsIInterfaceRequestor)
                                 .getInterface(Ci.nsIWebNavigation)
                                 .QueryInterface(Ci.nsIDocShell);
      let notificationBox = gBrowser.getNotificationBox(aBrowser);
      let notification = notificationBox.getNotificationWithValue("refresh-blocked");
      if (notification) {
        notification.label = message;
        notification.refreshURI = aURI;
        notification.delay = aDelay;
        notification.docShell = docShell;
      } else {
        let buttons = [{
          label: refreshButtonText,
          accessKey: refreshButtonAccesskey,
          callback: function (aNotification, aButton) {
            var refreshURI = aNotification.docShell
                                          .QueryInterface(Ci.nsIRefreshURI);
            refreshURI.forceRefreshURI(aNotification.refreshURI,
                                       aNotification.delay, true);
          }
        }];
        notification =
          notificationBox.appendNotification(message, "refresh-blocked",
                                             "chrome://browser/skin/Info.png",
                                             notificationBox.PRIORITY_INFO_MEDIUM,
                                             buttons);
        notification.refreshURI = aURI;
        notification.delay = aDelay;
        notification.docShell = docShell;
      }
      return false;
    }
    return true;
  }
}

function nsBrowserAccess() { }

nsBrowserAccess.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIBrowserDOMWindow, Ci.nsISupports]),

  openURI: function (aURI, aOpener, aWhere, aContext) {
    var newWindow = null;
    var isExternal = (aContext == Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL);

    if (isExternal && aURI && aURI.schemeIs("chrome")) {
      dump("use -chrome command-line option to load external chrome urls\n");
      return null;
    }

    if (aWhere == Ci.nsIBrowserDOMWindow.OPEN_DEFAULTWINDOW) {
      if (isExternal &&
          gPrefService.prefHasUserValue("browser.link.open_newwindow.override.external"))
        aWhere = gPrefService.getIntPref("browser.link.open_newwindow.override.external");
      else
        aWhere = gPrefService.getIntPref("browser.link.open_newwindow");
    }
    switch (aWhere) {
      case Ci.nsIBrowserDOMWindow.OPEN_NEWWINDOW :
        // FIXME: Bug 408379. So how come this doesn't send the
        // referrer like the other loads do?
        var url = aURI ? aURI.spec : "about:blank";
        // Pass all params to openDialog to ensure that "url" isn't passed through
        // loadOneOrMoreURIs, which splits based on "|"
        newWindow = openDialog(getBrowserURL(), "_blank", "all,dialog=no", url, null, null, null);
        break;
      case Ci.nsIBrowserDOMWindow.OPEN_NEWTAB :
        let win, needToFocusWin;

        // try the current window.  if we're in a popup, fall back on the most recent browser window
        if (window.toolbar.visible)
          win = window;
        else {
          win = Cc["@mozilla.org/browser/browserglue;1"]
                  .getService(Ci.nsIBrowserGlue)
                  .getMostRecentBrowserWindow();
          needToFocusWin = true;
        }

        if (!win) {
          // we couldn't find a suitable window, a new one needs to be opened.
          return null;
        }

        if (isExternal && (!aURI || aURI.spec == "about:blank")) {
          win.BrowserOpenTab(); // this also focuses the location bar
          win.focus();
          newWindow = win.content;
          break;
        }

        let loadInBackground = gPrefService.getBoolPref("browser.tabs.loadDivertedInBackground");
        let referrer = aOpener ? makeURI(aOpener.location.href) : null;

        let tab = win.gBrowser.loadOneTab(aURI ? aURI.spec : "about:blank", {
                                          referrerURI: referrer,
                                          fromExternal: isExternal,
                                          inBackground: loadInBackground});
        let browser = win.gBrowser.getBrowserForTab(tab);

        newWindow = browser.contentWindow;
        if (needToFocusWin || (!loadInBackground && isExternal))
          newWindow.focus();
        break;
      default : // OPEN_CURRENTWINDOW or an illegal value
        newWindow = content;
        if (aURI) {
          let referrer = aOpener ? makeURI(aOpener.location.href) : null;
          let loadflags = isExternal ?
                            Ci.nsIWebNavigation.LOAD_FLAGS_FROM_EXTERNAL :
                            Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
          gBrowser.loadURIWithFlags(aURI.spec, loadflags, referrer, null, null);
        }
        if (!gPrefService.getBoolPref("browser.tabs.loadDivertedInBackground"))
          window.focus();
    }
    return newWindow;
  },

  isTabContentWindow: function (aWindow) {
    return gBrowser.browsers.some(function (browser) browser.contentWindow == aWindow);
  }
}

function onViewToolbarsPopupShowing(aEvent, aInsertPoint) {
  var popup = aEvent.target;
  if (popup != aEvent.currentTarget)
    return;

  // Empty the menu
  for (var i = popup.childNodes.length-1; i >= 0; --i) {
    var deadItem = popup.childNodes[i];
    if (deadItem.hasAttribute("toolbarId"))
      popup.removeChild(deadItem);
  }

  var firstMenuItem = aInsertPoint || popup.firstChild;

  let toolbarNodes = Array.slice(gNavToolbox.childNodes);
  toolbarNodes.push(document.getElementById("addon-bar"));

  for (let toolbar of toolbarNodes) {
    let toolbarName = toolbar.getAttribute("toolbarname");
    if (toolbarName) {
      let menuItem = document.createElement("menuitem");
      let hidingAttribute = toolbar.getAttribute("type") == "menubar" ?
                            "autohide" : "collapsed";
      menuItem.setAttribute("id", "toggle_" + toolbar.id);
      menuItem.setAttribute("toolbarId", toolbar.id);
      menuItem.setAttribute("type", "checkbox");
      menuItem.setAttribute("label", toolbarName);
      menuItem.setAttribute("checked", toolbar.getAttribute(hidingAttribute) != "true");
      if (popup.id != "appmenu_customizeMenu")
        menuItem.setAttribute("accesskey", toolbar.getAttribute("accesskey"));
      if (popup.id != "toolbar-context-menu")
        menuItem.setAttribute("key", toolbar.getAttribute("key"));

      popup.insertBefore(menuItem, firstMenuItem);

      menuItem.addEventListener("command", onViewToolbarCommand, false);
    }
  }
}

function onViewToolbarCommand(aEvent) {
  var toolbarId = aEvent.originalTarget.getAttribute("toolbarId");
  var toolbar = document.getElementById(toolbarId);
  var isVisible = aEvent.originalTarget.getAttribute("checked") == "true";
  setToolbarVisibility(toolbar, isVisible);
}

function setToolbarVisibility(toolbar, isVisible) {
  var hidingAttribute = toolbar.getAttribute("type") == "menubar" ?
                        "autohide" : "collapsed";

  toolbar.setAttribute(hidingAttribute, !isVisible);
  document.persist(toolbar.id, hidingAttribute);

  PlacesToolbarHelper.init();
  BookmarksMenuButton.updatePosition();
  gBrowser.updateWindowResizers();

#ifdef MENUBAR_CAN_AUTOHIDE
  updateAppButtonDisplay();
#endif
}

var TabsOnTop = {
  init: function TabsOnTop_init() {
    Services.prefs.addObserver(this._prefName, this, false);

    // Only show the toggle UI if the user disabled tabs on top.
    if (Services.prefs.getBoolPref(this._prefName)) {
      for (let item of document.querySelectorAll("menuitem[command=cmd_ToggleTabsOnTop]"))
        item.parentNode.removeChild(item);
    }
  },

  uninit: function TabsOnTop_uninit() {
    Services.prefs.removeObserver(this._prefName, this);
  },

  toggle: function () {
    this.enabled = !Services.prefs.getBoolPref(this._prefName);
  },

  syncUI: function () {
    let userEnabled = Services.prefs.getBoolPref(this._prefName);
    let enabled = userEnabled && gBrowser.tabContainer.visible;

    document.getElementById("cmd_ToggleTabsOnTop")
            .setAttribute("checked", userEnabled);

    document.documentElement.setAttribute("tabsontop", enabled);
    document.getElementById("navigator-toolbox").setAttribute("tabsontop", enabled);
    document.getElementById("TabsToolbar").setAttribute("tabsontop", enabled);
    document.getElementById("nav-bar").setAttribute("tabsontop", enabled);
    gBrowser.tabContainer.setAttribute("tabsontop", enabled);
    TabsInTitlebar.allowedBy("tabs-on-top", enabled);
  },

  get enabled () {
    return gNavToolbox.getAttribute("tabsontop") == "true";
  },

  set enabled (val) {
    Services.prefs.setBoolPref(this._prefName, !!val);
    return val;
  },

  observe: function (subject, topic, data) {
    if (topic == "nsPref:changed")
      this.syncUI();
  },

  _prefName: "browser.tabs.onTop"
}

var TabsInTitlebar = {
  init: function () {
#ifdef CAN_DRAW_IN_TITLEBAR
    this._readPref();
    Services.prefs.addObserver(this._prefName, this, false);

    // Don't trust the initial value of the sizemode attribute; wait for
    // the resize event (handled in tabbrowser.xml).
    this.allowedBy("sizemode", false);

    this._initialized = true;
#endif
  },

  allowedBy: function (condition, allow) {
#ifdef CAN_DRAW_IN_TITLEBAR
    if (allow) {
      if (condition in this._disallowed) {
        delete this._disallowed[condition];
        this._update();
      }
    } else {
      if (!(condition in this._disallowed)) {
        this._disallowed[condition] = null;
        this._update();
      }
    }
#endif
  },

  get enabled() {
    return document.documentElement.getAttribute("tabsintitlebar") == "true";
  },

#ifdef CAN_DRAW_IN_TITLEBAR
  observe: function (subject, topic, data) {
    if (topic == "nsPref:changed")
      this._readPref();
  },

  _initialized: false,
  _disallowed: {},
  _prefName: "browser.tabs.drawInTitlebar",

  _readPref: function () {
    this.allowedBy("pref",
                   Services.prefs.getBoolPref(this._prefName));
  },

  _update: function () {
    if (!this._initialized || window.fullScreen)
      return;

    let allowed = true;
    for (let something in this._disallowed) {
      allowed = false;
      break;
    }

    if (allowed == this.enabled)
      return;

    function $(id) document.getElementById(id);
    let titlebar = $("titlebar");

    if (allowed) {
      function rect(ele)   ele.getBoundingClientRect();

      let tabsToolbar       = $("TabsToolbar");

      let appmenuButtonBox  = $("appmenu-button-container");
      let captionButtonsBox = $("titlebar-buttonbox");
      this._sizePlaceholder("appmenu-button", rect(appmenuButtonBox).width);
      this._sizePlaceholder("caption-buttons", rect(captionButtonsBox).width);

      let tabsToolbarRect = rect(tabsToolbar);
      let titlebarTop = rect($("titlebar-content")).top;
      titlebar.style.marginBottom = - Math.min(tabsToolbarRect.top - titlebarTop,
                                               tabsToolbarRect.height) + "px";

      document.documentElement.setAttribute("tabsintitlebar", "true");

      if (!this._draghandle) {
        let tmp = {};
        Components.utils.import("resource://gre/modules/WindowDraggingUtils.jsm", tmp);
        this._draghandle = new tmp.WindowDraggingElement(tabsToolbar);
        this._draghandle.mouseDownCheck = function () {
          return !this._dragBindingAlive && TabsInTitlebar.enabled;
        };
      }
    } else {
      document.documentElement.removeAttribute("tabsintitlebar");

      titlebar.style.marginBottom = "";
    }
  },

  _sizePlaceholder: function (type, width) {
    Array.forEach(document.querySelectorAll(".titlebar-placeholder[type='"+ type +"']"),
                  function (node) { node.width = width; });
  },
#endif

  uninit: function () {
#ifdef CAN_DRAW_IN_TITLEBAR
    this._initialized = false;
    Services.prefs.removeObserver(this._prefName, this);
#endif
  }
};

#ifdef MENUBAR_CAN_AUTOHIDE
function updateAppButtonDisplay() {
  var displayAppButton =
    !gInPrintPreviewMode &&
    window.menubar.visible &&
    document.getElementById("toolbar-menubar").getAttribute("autohide") == "true";

#ifdef CAN_DRAW_IN_TITLEBAR
  document.getElementById("titlebar").hidden = !displayAppButton;

  if (displayAppButton)
    document.documentElement.setAttribute("chromemargin", "0,2,2,2");
  else
    document.documentElement.removeAttribute("chromemargin");

  TabsInTitlebar.allowedBy("drawing-in-titlebar", displayAppButton);
#else
  document.getElementById("appmenu-toolbar-button").hidden =
    !displayAppButton;
#endif
}
#endif

#ifdef CAN_DRAW_IN_TITLEBAR
function onTitlebarMaxClick() {
  if (window.windowState == window.STATE_MAXIMIZED)
    window.restore();
  else
    window.maximize();
}
#endif

function displaySecurityInfo()
{
  BrowserPageInfo(null, "securityTab");
}

/**
 * Opens or closes the sidebar identified by commandID.
 *
 * @param commandID a string identifying the sidebar to toggle; see the
 *                  note below. (Optional if a sidebar is already open.)
 * @param forceOpen boolean indicating whether the sidebar should be
 *                  opened regardless of its current state (optional).
 * @note
 * We expect to find a xul:broadcaster element with the specified ID.
 * The following attributes on that element may be used and/or modified:
 *  - id           (required) the string to match commandID. The convention
 *                 is to use this naming scheme: 'view<sidebar-name>Sidebar'.
 *  - sidebarurl   (required) specifies the URL to load in this sidebar.
 *  - sidebartitle or label (in that order) specify the title to
 *                 display on the sidebar.
 *  - checked      indicates whether the sidebar is currently displayed.
 *                 Note that toggleSidebar updates this attribute when
 *                 it changes the sidebar's visibility.
 *  - group        this attribute must be set to "sidebar".
 */
function toggleSidebar(commandID, forceOpen) {

  var sidebarBox = document.getElementById("sidebar-box");
  if (!commandID)
    commandID = sidebarBox.getAttribute("sidebarcommand");

  var sidebarBroadcaster = document.getElementById(commandID);
  var sidebar = document.getElementById("sidebar"); // xul:browser
  var sidebarTitle = document.getElementById("sidebar-title");
  var sidebarSplitter = document.getElementById("sidebar-splitter");

  if (sidebarBroadcaster.getAttribute("checked") == "true") {
    if (!forceOpen) {
      // Replace the document currently displayed in the sidebar with about:blank
      // so that we can free memory by unloading the page. We need to explicitly
      // create a new content viewer because the old one doesn't get destroyed
      // until about:blank has loaded (which does not happen as long as the
      // element is hidden).
      sidebar.setAttribute("src", "about:blank");
      sidebar.docShell.createAboutBlankContentViewer(null);

      sidebarBroadcaster.removeAttribute("checked");
      sidebarBox.setAttribute("sidebarcommand", "");
      sidebarTitle.value = "";
      sidebarBox.hidden = true;
      sidebarSplitter.hidden = true;
      gBrowser.selectedBrowser.focus();
    } else {
      fireSidebarFocusedEvent();
    }
    return;
  }

  // now we need to show the specified sidebar

  // ..but first update the 'checked' state of all sidebar broadcasters
  var broadcasters = document.getElementsByAttribute("group", "sidebar");
  for (let broadcaster of broadcasters) {
    // skip elements that observe sidebar broadcasters and random
    // other elements
    if (broadcaster.localName != "broadcaster")
      continue;

    if (broadcaster != sidebarBroadcaster)
      broadcaster.removeAttribute("checked");
    else
      sidebarBroadcaster.setAttribute("checked", "true");
  }

  sidebarBox.hidden = false;
  sidebarSplitter.hidden = false;

  var url = sidebarBroadcaster.getAttribute("sidebarurl");
  var title = sidebarBroadcaster.getAttribute("sidebartitle");
  if (!title)
    title = sidebarBroadcaster.getAttribute("label");
  sidebar.setAttribute("src", url); // kick off async load
  sidebarBox.setAttribute("sidebarcommand", sidebarBroadcaster.id);
  sidebarTitle.value = title;

  // We set this attribute here in addition to setting it on the <browser>
  // element itself, because the code in gBrowserInit.onUnload persists this
  // attribute, not the "src" of the <browser id="sidebar">. The reason it
  // does that is that we want to delay sidebar load a bit when a browser
  // window opens. See delayedStartup().
  sidebarBox.setAttribute("src", url);

  if (sidebar.contentDocument.location.href != url)
    sidebar.addEventListener("load", sidebarOnLoad, true);
  else // older code handled this case, so we do it too
    fireSidebarFocusedEvent();
}

function sidebarOnLoad(event) {
  var sidebar = document.getElementById("sidebar");
  sidebar.removeEventListener("load", sidebarOnLoad, true);
  // We're handling the 'load' event before it bubbles up to the usual
  // (non-capturing) event handlers. Let it bubble up before firing the
  // SidebarFocused event.
  setTimeout(fireSidebarFocusedEvent, 0);
}

/**
 * Fire a "SidebarFocused" event on the sidebar's |window| to give the sidebar
 * a chance to adjust focus as needed. An additional event is needed, because
 * we don't want to focus the sidebar when it's opened on startup or in a new
 * window, only when the user opens the sidebar.
 */
function fireSidebarFocusedEvent() {
  var sidebar = document.getElementById("sidebar");
  var event = document.createEvent("Events");
  event.initEvent("SidebarFocused", true, false);
  sidebar.contentWindow.dispatchEvent(event);
}

var gHomeButton = {
  prefDomain: "browser.startup.homepage",
  observe: function (aSubject, aTopic, aPrefName)
  {
    if (aTopic != "nsPref:changed" || aPrefName != this.prefDomain)
      return;

    this.updateTooltip();
  },

  updateTooltip: function (homeButton)
  {
    if (!homeButton)
      homeButton = document.getElementById("home-button");
    if (homeButton) {
      var homePage = this.getHomePage();
      homePage = homePage.replace(/\|/g,', ');
      if (homePage.toLowerCase() == "about:home")
        homeButton.setAttribute("tooltiptext", homeButton.getAttribute("aboutHomeOverrideTooltip"));
      else
        homeButton.setAttribute("tooltiptext", homePage);
    }
  },

  getHomePage: function ()
  {
    var url;
    try {
      url = gPrefService.getComplexValue(this.prefDomain,
                                Components.interfaces.nsIPrefLocalizedString).data;
    } catch (e) {
    }

    // use this if we can't find the pref
    if (!url) {
      var SBS = Cc["@mozilla.org/intl/stringbundle;1"].getService(Ci.nsIStringBundleService);
      var configBundle = SBS.createBundle("chrome://branding/locale/browserconfig.properties");
      url = configBundle.GetStringFromName(this.prefDomain);
    }

    return url;
  },

  updatePersonalToolbarStyle: function (homeButton)
  {
    if (!homeButton)
      homeButton = document.getElementById("home-button");
    if (homeButton)
      homeButton.className = homeButton.parentNode.id == "PersonalToolbar"
                               || homeButton.parentNode.parentNode.id == "PersonalToolbar" ?
                             homeButton.className.replace("toolbarbutton-1", "bookmark-item") :
                             homeButton.className.replace("bookmark-item", "toolbarbutton-1");
  }
};

/**
 * Gets the selected text in the active browser. Leading and trailing
 * whitespace is removed, and consecutive whitespace is replaced by a single
 * space. A maximum of 150 characters will be returned, regardless of the value
 * of aCharLen.
 *
 * @param aCharLen
 *        The maximum number of characters to return.
 */
function getBrowserSelection(aCharLen) {
  // selections of more than 150 characters aren't useful
  const kMaxSelectionLen = 150;
  const charLen = Math.min(aCharLen || kMaxSelectionLen, kMaxSelectionLen);
  let commandDispatcher = document.commandDispatcher;

  var focusedWindow = commandDispatcher.focusedWindow;
  var selection = focusedWindow.getSelection().toString();
  // try getting a selected text in text input.
  if (!selection) {
    let element = commandDispatcher.focusedElement;
    var isOnTextInput = function isOnTextInput(elem) {
      // we avoid to return a value if a selection is in password field.
      // ref. bug 565717
      return elem instanceof HTMLTextAreaElement ||
             (elem instanceof HTMLInputElement && elem.mozIsTextField(true));
    };

    if (isOnTextInput(element)) {
      selection = element.QueryInterface(Ci.nsIDOMNSEditableElement)
                         .editor.selection.toString();
    }
  }

  if (selection) {
    if (selection.length > charLen) {
      // only use the first charLen important chars. see bug 221361
      var pattern = new RegExp("^(?:\\s*.){0," + charLen + "}");
      pattern.test(selection);
      selection = RegExp.lastMatch;
    }

    selection = selection.replace(/^\s+/, "")
                         .replace(/\s+$/, "")
                         .replace(/\s+/g, " ");

    if (selection.length > charLen)
      selection = selection.substr(0, charLen);
  }
  return selection;
}

var gWebPanelURI;
function openWebPanel(aTitle, aURI)
{
    // Ensure that the web panels sidebar is open.
    toggleSidebar('viewWebPanelsSidebar', true);

    // Set the title of the panel.
    document.getElementById("sidebar-title").value = aTitle;

    // Tell the Web Panels sidebar to load the bookmark.
    var sidebar = document.getElementById("sidebar");
    if (sidebar.docShell && sidebar.contentDocument && sidebar.contentDocument.getElementById('web-panels-browser')) {
        sidebar.contentWindow.loadWebPanel(aURI);
        if (gWebPanelURI) {
            gWebPanelURI = "";
            sidebar.removeEventListener("load", asyncOpenWebPanel, true);
        }
    }
    else {
        // The panel is still being constructed.  Attach an onload handler.
        if (!gWebPanelURI)
            sidebar.addEventListener("load", asyncOpenWebPanel, true);
        gWebPanelURI = aURI;
    }
}

function asyncOpenWebPanel(event)
{
    var sidebar = document.getElementById("sidebar");
    if (gWebPanelURI && sidebar.contentDocument && sidebar.contentDocument.getElementById('web-panels-browser'))
        sidebar.contentWindow.loadWebPanel(gWebPanelURI);
    gWebPanelURI = "";
    sidebar.removeEventListener("load", asyncOpenWebPanel, true);
}

/*
 * - [ Dependencies ] ---------------------------------------------------------
 *  utilityOverlay.js:
 *    - gatherTextUnder
 */

/**
 * Extracts linkNode and href for the current click target.
 *
 * @param event
 *        The click event.
 * @return [href, linkNode].
 *
 * @note linkNode will be null if the click wasn't on an anchor
 *       element (or XLink).
 */
function hrefAndLinkNodeForClickEvent(event)
{
  function isHTMLLink(aNode)
  {
    // Be consistent with what nsContextMenu.js does.
    return ((aNode instanceof HTMLAnchorElement && aNode.href) ||
            (aNode instanceof HTMLAreaElement && aNode.href) ||
            aNode instanceof HTMLLinkElement);
  }

  let node = event.target;
  while (node && !isHTMLLink(node)) {
    node = node.parentNode;
  }

  if (node)
    return [node.href, node];

  // If there is no linkNode, try simple XLink.
  let href, baseURI;
  node = event.target;
  while (node && !href) {
    if (node.nodeType == Node.ELEMENT_NODE) {
      href = node.getAttributeNS("http://www.w3.org/1999/xlink", "href");
      if (href)
        baseURI = node.baseURI;
    }
    node = node.parentNode;
  }

  // In case of XLink, we don't return the node we got href from since
  // callers expect <a>-like elements.
  return [href ? makeURLAbsolute(baseURI, href) : null, null];
}

/**
 * Called whenever the user clicks in the content area.
 *
 * @param event
 *        The click event.
 * @param isPanelClick
 *        Whether the event comes from a web panel.
 * @note default event is prevented if the click is handled.
 */
function contentAreaClick(event, isPanelClick)
{
  if (!event.isTrusted || event.defaultPrevented || event.button == 2)
    return;

  let [href, linkNode] = hrefAndLinkNodeForClickEvent(event);
  if (!href) {
    // Not a link, handle middle mouse navigation.
    if (event.button == 1 &&
        gPrefService.getBoolPref("middlemouse.contentLoadURL") &&
        !gPrefService.getBoolPref("general.autoScroll")) {
      middleMousePaste(event);
      event.preventDefault();
    }
    return;
  }

  // This code only applies if we have a linkNode (i.e. clicks on real anchor
  // elements, as opposed to XLink).
  if (linkNode && event.button == 0 &&
      !event.ctrlKey && !event.shiftKey && !event.altKey && !event.metaKey) {
    // A Web panel's links should target the main content area.  Do this
    // if no modifier keys are down and if there's no target or the target
    // equals _main (the IE convention) or _content (the Mozilla convention).
    let target = linkNode.target;
    let mainTarget = !target || target == "_content" || target  == "_main";
    if (isPanelClick && mainTarget) {
      // javascript and data links should be executed in the current browser.
      if (linkNode.getAttribute("onclick") ||
          href.substr(0, 11) === "javascript:" ||
          href.substr(0, 5) === "data:")
        return;

      try {
        urlSecurityCheck(href, linkNode.ownerDocument.nodePrincipal);
      }
      catch(ex) {
        // Prevent loading unsecure destinations.
        event.preventDefault();
        return;
      }

      loadURI(href, null, null, false);
      event.preventDefault();
      return;
    }

    if (linkNode.getAttribute("rel") == "sidebar") {
      // This is the Opera convention for a special link that, when clicked,
      // allows to add a sidebar panel.  The link's title attribute contains
      // the title that should be used for the sidebar panel.
      PlacesUIUtils.showBookmarkDialog({ action: "add"
                                       , type: "bookmark"
                                       , uri: makeURI(href)
                                       , title: linkNode.getAttribute("title")
                                       , loadBookmarkInSidebar: true
                                       , hiddenRows: [ "description"
                                                     , "location"
                                                     , "keyword" ]
                                       }, window);
      event.preventDefault();
      return;
    }
  }

  handleLinkClick(event, href, linkNode);

  // Mark the page as a user followed link.  This is done so that history can
  // distinguish automatic embed visits from user activated ones.  For example
  // pages loaded in frames are embed visits and lost with the session, while
  // visits across frames should be preserved.
  try {
    if (!PrivateBrowsingUtils.isWindowPrivate(window))
      PlacesUIUtils.markPageAsFollowedLink(href);
  } catch (ex) { /* Skip invalid URIs. */ }
}

/**
 * Handles clicks on links.
 *
 * @return true if the click event was handled, false otherwise.
 */
function handleLinkClick(event, href, linkNode) {
  if (event.button == 2) // right click
    return false;

  var where = whereToOpenLink(event);
  if (where == "current")
    return false;

  var doc = event.target.ownerDocument;

  if (where == "save") {
    saveURL(href, linkNode ? gatherTextUnder(linkNode) : "", null, true,
            true, doc.documentURIObject, doc);
    event.preventDefault();
    return true;
  }

  urlSecurityCheck(href, doc.nodePrincipal);
  openLinkIn(href, where, { referrerURI: doc.documentURIObject,
                            charset: doc.characterSet });
  event.preventDefault();
  return true;
}

function middleMousePaste(event) {
  let clipboard = readFromClipboard();
  if (!clipboard)
    return;

  // Strip embedded newlines and surrounding whitespace, to match the URL
  // bar's behavior (stripsurroundingwhitespace)
  clipboard = clipboard.replace(/\s*\n\s*/g, "");

  let mayInheritPrincipal = { value: false };
  let url = getShortcutOrURI(clipboard, mayInheritPrincipal);
  try {
    makeURI(url);
  } catch (ex) {
    // Not a valid URI.
    return;
  }

  try {
    addToUrlbarHistory(url);
  } catch (ex) {
    // Things may go wrong when adding url to session history,
    // but don't let that interfere with the loading of the url.
    Cu.reportError(ex);
  }

  openUILink(url, event,
             { ignoreButton: true,
               disallowInheritPrincipal: !mayInheritPrincipal.value });

  event.stopPropagation();
}

function handleDroppedLink(event, url, name)
{
  let postData = { };
  let uri = getShortcutOrURI(url, postData);
  if (uri)
    loadURI(uri, null, postData.value, false);

  // Keep the event from being handled by the dragDrop listeners
  // built-in to gecko if they happen to be above us.
  event.preventDefault();
};

function MultiplexHandler(event)
{ try {
    var node = event.target;
    var name = node.getAttribute('name');

    if (name == 'detectorGroup') {
        BrowserCharsetReload();
        SelectDetector(event, false);
    } else if (name == 'charsetGroup') {
        var charset = node.getAttribute('id');
        charset = charset.substring('charset.'.length, charset.length)
        BrowserSetForcedCharacterSet(charset);
    } else if (name == 'charsetCustomize') {
        //do nothing - please remove this else statement, once the charset prefs moves to the pref window
    } else {
        BrowserSetForcedCharacterSet(node.getAttribute('id'));
    }
    } catch(ex) { alert(ex); }
}

function SelectDetector(event, doReload)
{
    var uri =  event.target.getAttribute("id");
    var prefvalue = uri.substring('chardet.'.length, uri.length);
    if ("off" == prefvalue) { // "off" is special value to turn off the detectors
        prefvalue = "";
    }

    try {
        var str =  Cc["@mozilla.org/supports-string;1"].
                   createInstance(Ci.nsISupportsString);

        str.data = prefvalue;
        gPrefService.setComplexValue("intl.charset.detector", Ci.nsISupportsString, str);
        if (doReload)
          window.content.location.reload();
    }
    catch (ex) {
        dump("Failed to set the intl.charset.detector preference.\n");
    }
}

function BrowserSetForcedCharacterSet(aCharset)
{
  gBrowser.docShell.charset = aCharset;
  // Save the forced character-set
  if (!PrivateBrowsingUtils.isWindowPrivate(window))
    PlacesUtils.history.setCharsetForURI(getWebNavigation().currentURI, aCharset);
  BrowserCharsetReload();
}

function BrowserCharsetReload()
{
  BrowserReloadWithFlags(nsIWebNavigation.LOAD_FLAGS_CHARSET_CHANGE);
}

function charsetMenuGetElement(parent, id) {
  return parent.getElementsByAttribute("id", id)[0];
}

function UpdateCurrentCharset(target) {
    // extract the charset from DOM
    var wnd = document.commandDispatcher.focusedWindow;
    if ((window == wnd) || (wnd == null)) wnd = window.content;

    // Uncheck previous item
    if (gPrevCharset) {
        var pref_item = charsetMenuGetElement(target, "charset." + gPrevCharset);
        if (pref_item)
          pref_item.setAttribute('checked', 'false');
    }

    var menuitem = charsetMenuGetElement(target, "charset." + wnd.document.characterSet);
    if (menuitem) {
        menuitem.setAttribute('checked', 'true');
    }
}

function UpdateCharsetDetector(target) {
  var prefvalue;

  try {
    prefvalue = gPrefService.getComplexValue("intl.charset.detector", Ci.nsIPrefLocalizedString).data;
  }
  catch (ex) {}

  if (!prefvalue)
    prefvalue = "off";

  var menuitem = charsetMenuGetElement(target, "chardet." + prefvalue);
  if (menuitem)
    menuitem.setAttribute("checked", "true");
}

function UpdateMenus(event) {
  UpdateCurrentCharset(event.target);
  UpdateCharsetDetector(event.target);
}

function CreateMenu(node) {
  Services.obs.notifyObservers(null, "charsetmenu-selected", node);
}

function charsetLoadListener() {
  var charset = window.content.document.characterSet;

  if (charset.length > 0 && (charset != gLastBrowserCharset)) {
    if (!gCharsetMenu)
      gCharsetMenu = Cc['@mozilla.org/rdf/datasource;1?name=charset-menu'].getService(Ci.nsICurrentCharsetListener);
    gCharsetMenu.SetCurrentCharset(charset);
    gPrevCharset = gLastBrowserCharset;
    gLastBrowserCharset = charset;
  }
}


var gPageStyleMenu = {

  _getAllStyleSheets: function (frameset) {
    var styleSheetsArray = Array.slice(frameset.document.styleSheets);
    for (let i = 0; i < frameset.frames.length; i++) {
      let frameSheets = this._getAllStyleSheets(frameset.frames[i]);
      styleSheetsArray = styleSheetsArray.concat(frameSheets);
    }
    return styleSheetsArray;
  },

  fillPopup: function (menuPopup) {
    var noStyle = menuPopup.firstChild;
    var persistentOnly = noStyle.nextSibling;
    var sep = persistentOnly.nextSibling;
    while (sep.nextSibling)
      menuPopup.removeChild(sep.nextSibling);

    var styleSheets = this._getAllStyleSheets(window.content);
    var currentStyleSheets = {};
    var styleDisabled = getMarkupDocumentViewer().authorStyleDisabled;
    var haveAltSheets = false;
    var altStyleSelected = false;

    for (let currentStyleSheet of styleSheets) {
      if (!currentStyleSheet.title)
        continue;

      // Skip any stylesheets whose media attribute doesn't match.
      if (currentStyleSheet.media.length > 0) {
        let mediaQueryList = currentStyleSheet.media.mediaText;
        if (!window.content.matchMedia(mediaQueryList).matches)
          continue;
      }

      if (!currentStyleSheet.disabled)
        altStyleSelected = true;

      haveAltSheets = true;

      let lastWithSameTitle = null;
      if (currentStyleSheet.title in currentStyleSheets)
        lastWithSameTitle = currentStyleSheets[currentStyleSheet.title];

      if (!lastWithSameTitle) {
        let menuItem = document.createElement("menuitem");
        menuItem.setAttribute("type", "radio");
        menuItem.setAttribute("label", currentStyleSheet.title);
        menuItem.setAttribute("data", currentStyleSheet.title);
        menuItem.setAttribute("checked", !currentStyleSheet.disabled && !styleDisabled);
        menuItem.setAttribute("oncommand", "gPageStyleMenu.switchStyleSheet(this.getAttribute('data'));");
        menuPopup.appendChild(menuItem);
        currentStyleSheets[currentStyleSheet.title] = menuItem;
      } else if (currentStyleSheet.disabled) {
        lastWithSameTitle.removeAttribute("checked");
      }
    }

    noStyle.setAttribute("checked", styleDisabled);
    persistentOnly.setAttribute("checked", !altStyleSelected && !styleDisabled);
    persistentOnly.hidden = (window.content.document.preferredStyleSheetSet) ? haveAltSheets : false;
    sep.hidden = (noStyle.hidden && persistentOnly.hidden) || !haveAltSheets;
  },

  _stylesheetInFrame: function (frame, title) {
    return Array.some(frame.document.styleSheets,
                      function (stylesheet) stylesheet.title == title);
  },

  _stylesheetSwitchFrame: function (frame, title) {
    var docStyleSheets = frame.document.styleSheets;

    for (let i = 0; i < docStyleSheets.length; ++i) {
      let docStyleSheet = docStyleSheets[i];

      if (docStyleSheet.title)
        docStyleSheet.disabled = (docStyleSheet.title != title);
      else if (docStyleSheet.disabled)
        docStyleSheet.disabled = false;
    }
  },

  _stylesheetSwitchAll: function (frameset, title) {
    if (!title || this._stylesheetInFrame(frameset, title))
      this._stylesheetSwitchFrame(frameset, title);

    for (let i = 0; i < frameset.frames.length; i++)
      this._stylesheetSwitchAll(frameset.frames[i], title);
  },

  switchStyleSheet: function (title, contentWindow) {
    getMarkupDocumentViewer().authorStyleDisabled = false;
    this._stylesheetSwitchAll(contentWindow || content, title);
  },

  disableStyle: function () {
    getMarkupDocumentViewer().authorStyleDisabled = true;
  },
};

/* Legacy global page-style functions */
var getAllStyleSheets   = gPageStyleMenu._getAllStyleSheets.bind(gPageStyleMenu);
var stylesheetFillPopup = gPageStyleMenu.fillPopup.bind(gPageStyleMenu);
function stylesheetSwitchAll(contentWindow, title) {
  gPageStyleMenu.switchStyleSheet(title, contentWindow);
}
function setStyleDisabled(disabled) {
  if (disabled)
    gPageStyleMenu.disableStyle();
}


var BrowserOffline = {
  _inited: false,

  /////////////////////////////////////////////////////////////////////////////
  // BrowserOffline Public Methods
  init: function ()
  {
    if (!this._uiElement)
      this._uiElement = document.getElementById("workOfflineMenuitemState");

    Services.obs.addObserver(this, "network:offline-status-changed", false);

    this._updateOfflineUI(Services.io.offline);

    this._inited = true;
  },

  uninit: function ()
  {
    if (this._inited) {
      Services.obs.removeObserver(this, "network:offline-status-changed");
    }
  },

  toggleOfflineStatus: function ()
  {
    var ioService = Services.io;

    // Stop automatic management of the offline status
    try {
      ioService.manageOfflineStatus = false;
    } catch (ex) {
    }

    if (!ioService.offline && !this._canGoOffline()) {
      this._updateOfflineUI(false);
      return;
    }

    ioService.offline = !ioService.offline;
  },

  /////////////////////////////////////////////////////////////////////////////
  // nsIObserver
  observe: function (aSubject, aTopic, aState)
  {
    if (aTopic != "network:offline-status-changed")
      return;

    this._updateOfflineUI(aState == "offline");
  },

  /////////////////////////////////////////////////////////////////////////////
  // BrowserOffline Implementation Methods
  _canGoOffline: function ()
  {
    try {
      var cancelGoOffline = Cc["@mozilla.org/supports-PRBool;1"].createInstance(Ci.nsISupportsPRBool);
      Services.obs.notifyObservers(cancelGoOffline, "offline-requested", null);

      // Something aborted the quit process.
      if (cancelGoOffline.data)
        return false;
    }
    catch (ex) {
    }

    return true;
  },

  _uiElement: null,
  _updateOfflineUI: function (aOffline)
  {
    var offlineLocked = gPrefService.prefIsLocked("network.online");
    if (offlineLocked)
      this._uiElement.setAttribute("disabled", "true");

    this._uiElement.setAttribute("checked", aOffline);
  }
};

var OfflineApps = {
  /////////////////////////////////////////////////////////////////////////////
  // OfflineApps Public Methods
  init: function ()
  {
    Services.obs.addObserver(this, "offline-cache-update-completed", false);
  },

  uninit: function ()
  {
    Services.obs.removeObserver(this, "offline-cache-update-completed");
  },

  handleEvent: function(event) {
    if (event.type == "MozApplicationManifest") {
      this.offlineAppRequested(event.originalTarget.defaultView);
    }
  },

  /////////////////////////////////////////////////////////////////////////////
  // OfflineApps Implementation Methods

  // XXX: _getBrowserWindowForContentWindow and _getBrowserForContentWindow
  // were taken from browser/components/feeds/src/WebContentConverter.
  _getBrowserWindowForContentWindow: function(aContentWindow) {
    return aContentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIWebNavigation)
                         .QueryInterface(Ci.nsIDocShellTreeItem)
                         .rootTreeItem
                         .QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIDOMWindow)
                         .wrappedJSObject;
  },

  _getBrowserForContentWindow: function(aBrowserWindow, aContentWindow) {
    // This depends on pseudo APIs of browser.js and tabbrowser.xml
    aContentWindow = aContentWindow.top;
    var browsers = aBrowserWindow.gBrowser.browsers;
    for (let browser of browsers) {
      if (browser.contentWindow == aContentWindow)
        return browser;
    }
    return null;
  },

  _getManifestURI: function(aWindow) {
    if (!aWindow.document.documentElement)
      return null;

    var attr = aWindow.document.documentElement.getAttribute("manifest");
    if (!attr)
      return null;

    try {
      var contentURI = makeURI(aWindow.location.href, null, null);
      return makeURI(attr, aWindow.document.characterSet, contentURI);
    } catch (e) {
      return null;
    }
  },

  // A cache update isn't tied to a specific window.  Try to find
  // the best browser in which to warn the user about space usage
  _getBrowserForCacheUpdate: function(aCacheUpdate) {
    // Prefer the current browser
    var uri = this._getManifestURI(content);
    if (uri && uri.equals(aCacheUpdate.manifestURI)) {
      return gBrowser.selectedBrowser;
    }

    var browsers = gBrowser.browsers;
    for (let browser of browsers) {
      uri = this._getManifestURI(browser.contentWindow);
      if (uri && uri.equals(aCacheUpdate.manifestURI)) {
        return browser;
      }
    }

    return null;
  },

  _warnUsage: function(aBrowser, aURI) {
    if (!aBrowser)
      return;

    var notificationBox = gBrowser.getNotificationBox(aBrowser);
    var notification = notificationBox.getNotificationWithValue("offline-app-usage");
    if (!notification) {
      var buttons = [{
          label: gNavigatorBundle.getString("offlineApps.manageUsage"),
          accessKey: gNavigatorBundle.getString("offlineApps.manageUsageAccessKey"),
          callback: OfflineApps.manage
        }];

      var warnQuota = gPrefService.getIntPref("offline-apps.quota.warn");
      const priority = notificationBox.PRIORITY_WARNING_MEDIUM;
      var message = gNavigatorBundle.getFormattedString("offlineApps.usage",
                                                        [ aURI.host,
                                                          warnQuota / 1024 ]);

      notificationBox.appendNotification(message, "offline-app-usage",
                                         "chrome://browser/skin/Info.png",
                                         priority, buttons);
    }

    // Now that we've warned once, prevent the warning from showing up
    // again.
    Services.perms.add(aURI, "offline-app",
                       Ci.nsIOfflineCacheUpdateService.ALLOW_NO_WARN);
  },

  // XXX: duplicated in preferences/advanced.js
  _getOfflineAppUsage: function (host, groups)
  {
    var cacheService = Cc["@mozilla.org/network/application-cache-service;1"].
                       getService(Ci.nsIApplicationCacheService);
    if (!groups)
      groups = cacheService.getGroups();

    var usage = 0;
    for (let group of groups) {
      var uri = Services.io.newURI(group, null, null);
      if (uri.asciiHost == host) {
        var cache = cacheService.getActiveCache(group);
        usage += cache.usage;
      }
    }

    return usage;
  },

  _checkUsage: function(aURI) {
    // if the user has already allowed excessive usage, don't bother checking
    if (Services.perms.testExactPermission(aURI, "offline-app") !=
        Ci.nsIOfflineCacheUpdateService.ALLOW_NO_WARN) {
      var usage = this._getOfflineAppUsage(aURI.asciiHost);
      var warnQuota = gPrefService.getIntPref("offline-apps.quota.warn");
      if (usage >= warnQuota * 1024) {
        return true;
      }
    }

    return false;
  },

  offlineAppRequested: function(aContentWindow) {
    if (!gPrefService.getBoolPref("browser.offline-apps.notify")) {
      return;
    }

    var browserWindow = this._getBrowserWindowForContentWindow(aContentWindow);
    var browser = this._getBrowserForContentWindow(browserWindow,
                                                   aContentWindow);

    var currentURI = aContentWindow.document.documentURIObject;

    // don't bother showing UI if the user has already made a decision
    if (Services.perms.testExactPermission(currentURI, "offline-app") != Services.perms.UNKNOWN_ACTION)
      return;

    try {
      if (gPrefService.getBoolPref("offline-apps.allow_by_default")) {
        // all pages can use offline capabilities, no need to ask the user
        return;
      }
    } catch(e) {
      // this pref isn't set by default, ignore failures
    }

    var host = currentURI.asciiHost;
    var notificationBox = gBrowser.getNotificationBox(browser);
    var notificationID = "offline-app-requested-" + host;
    var notification = notificationBox.getNotificationWithValue(notificationID);

    if (notification) {
      notification.documents.push(aContentWindow.document);
    } else {
      var buttons = [{
        label: gNavigatorBundle.getString("offlineApps.allow"),
        accessKey: gNavigatorBundle.getString("offlineApps.allowAccessKey"),
        callback: function() {
          for (let document of notification.documents) {
            OfflineApps.allowSite(document);
          }
        }
      },{
        label: gNavigatorBundle.getString("offlineApps.never"),
        accessKey: gNavigatorBundle.getString("offlineApps.neverAccessKey"),
        callback: function() {
          for (let document of notification.documents) {
            OfflineApps.disallowSite(document);
          }
        }
      },{
        label: gNavigatorBundle.getString("offlineApps.notNow"),
        accessKey: gNavigatorBundle.getString("offlineApps.notNowAccessKey"),
        callback: function() { /* noop */ }
      }];

      const priority = notificationBox.PRIORITY_INFO_LOW;
      var message = gNavigatorBundle.getFormattedString("offlineApps.available",
                                                        [ host ]);
      notification =
        notificationBox.appendNotification(message, notificationID,
                                           "chrome://browser/skin/Info.png",
                                           priority, buttons);
      notification.documents = [ aContentWindow.document ];
    }
  },

  allowSite: function(aDocument) {
    Services.perms.add(aDocument.documentURIObject, "offline-app", Services.perms.ALLOW_ACTION);

    // When a site is enabled while loading, manifest resources will
    // start fetching immediately.  This one time we need to do it
    // ourselves.
    this._startFetching(aDocument);
  },

  disallowSite: function(aDocument) {
    Services.perms.add(aDocument.documentURIObject, "offline-app", Services.perms.DENY_ACTION);
  },

  manage: function() {
    openAdvancedPreferences("networkTab");
  },

  _startFetching: function(aDocument) {
    if (!aDocument.documentElement)
      return;

    var manifest = aDocument.documentElement.getAttribute("manifest");
    if (!manifest)
      return;

    var manifestURI = makeURI(manifest, aDocument.characterSet,
                              aDocument.documentURIObject);

    var updateService = Cc["@mozilla.org/offlinecacheupdate-service;1"].
                        getService(Ci.nsIOfflineCacheUpdateService);
    updateService.scheduleUpdate(manifestURI, aDocument.documentURIObject, window);
  },

  /////////////////////////////////////////////////////////////////////////////
  // nsIObserver
  observe: function (aSubject, aTopic, aState)
  {
    if (aTopic == "offline-cache-update-completed") {
      var cacheUpdate = aSubject.QueryInterface(Ci.nsIOfflineCacheUpdate);

      var uri = cacheUpdate.manifestURI;
      if (OfflineApps._checkUsage(uri)) {
        var browser = this._getBrowserForCacheUpdate(cacheUpdate);
        if (browser) {
          OfflineApps._warnUsage(browser, cacheUpdate.manifestURI);
        }
      }
    }
  }
};

var IndexedDBPromptHelper = {
  _permissionsPrompt: "indexedDB-permissions-prompt",
  _permissionsResponse: "indexedDB-permissions-response",

  _quotaPrompt: "indexedDB-quota-prompt",
  _quotaResponse: "indexedDB-quota-response",
  _quotaCancel: "indexedDB-quota-cancel",

  _notificationIcon: "indexedDB-notification-icon",

  init:
  function IndexedDBPromptHelper_init() {
    Services.obs.addObserver(this, this._permissionsPrompt, false);
    Services.obs.addObserver(this, this._quotaPrompt, false);
    Services.obs.addObserver(this, this._quotaCancel, false);
  },

  uninit:
  function IndexedDBPromptHelper_uninit() {
    Services.obs.removeObserver(this, this._permissionsPrompt, false);
    Services.obs.removeObserver(this, this._quotaPrompt, false);
    Services.obs.removeObserver(this, this._quotaCancel, false);
  },

  observe:
  function IndexedDBPromptHelper_observe(subject, topic, data) {
    if (topic != this._permissionsPrompt &&
        topic != this._quotaPrompt &&
        topic != this._quotaCancel) {
      throw new Error("Unexpected topic!");
    }

    var requestor = subject.QueryInterface(Ci.nsIInterfaceRequestor);

    var contentWindow = requestor.getInterface(Ci.nsIDOMWindow);
    var contentDocument = contentWindow.document;
    var browserWindow =
      OfflineApps._getBrowserWindowForContentWindow(contentWindow);

    if (browserWindow != window) {
      // Must belong to some other window.
      return;
    }

    var browser =
      OfflineApps._getBrowserForContentWindow(browserWindow, contentWindow);

    var host = contentDocument.documentURIObject.asciiHost;

    var message;
    var responseTopic;
    if (topic == this._permissionsPrompt) {
      message = gNavigatorBundle.getFormattedString("offlineApps.available",
                                                    [ host ]);
      responseTopic = this._permissionsResponse;
    }
    else if (topic == this._quotaPrompt) {
      message = gNavigatorBundle.getFormattedString("indexedDB.usage",
                                                    [ host, data ]);
      responseTopic = this._quotaResponse;
    }
    else if (topic == this._quotaCancel) {
      responseTopic = this._quotaResponse;
    }

    const hiddenTimeoutDuration = 30000; // 30 seconds
    const firstTimeoutDuration = 360000; // 5 minutes

    var timeoutId;

    var observer = requestor.getInterface(Ci.nsIObserver);

    var mainAction = {
      label: gNavigatorBundle.getString("offlineApps.allow"),
      accessKey: gNavigatorBundle.getString("offlineApps.allowAccessKey"),
      callback: function() {
        clearTimeout(timeoutId);
        observer.observe(null, responseTopic,
                         Ci.nsIPermissionManager.ALLOW_ACTION);
      }
    };

    var secondaryActions = [
      {
        label: gNavigatorBundle.getString("offlineApps.never"),
        accessKey: gNavigatorBundle.getString("offlineApps.neverAccessKey"),
        callback: function() {
          clearTimeout(timeoutId);
          observer.observe(null, responseTopic,
                           Ci.nsIPermissionManager.DENY_ACTION);
        }
      }
    ];

    // This will be set to the result of PopupNotifications.show() below, or to
    // the result of PopupNotifications.getNotification() if this is a
    // quotaCancel notification.
    var notification;

    function timeoutNotification() {
      // Remove the notification.
      if (notification) {
        notification.remove();
      }

      // Clear all of our timeout stuff. We may be called directly, not just
      // when the timeout actually elapses.
      clearTimeout(timeoutId);

      // And tell the page that the popup timed out.
      observer.observe(null, responseTopic,
                       Ci.nsIPermissionManager.UNKNOWN_ACTION);
    }

    var options = {
      eventCallback: function(state) {
        // Don't do anything if the timeout has not been set yet.
        if (!timeoutId) {
          return;
        }

        // If the popup is being dismissed start the short timeout.
        if (state == "dismissed") {
          clearTimeout(timeoutId);
          timeoutId = setTimeout(timeoutNotification, hiddenTimeoutDuration);
          return;
        }

        // If the popup is being re-shown then clear the timeout allowing
        // unlimited waiting.
        if (state == "shown") {
          clearTimeout(timeoutId);
        }
      }
    };

    if (topic == this._quotaCancel) {
      notification = PopupNotifications.getNotification(this._quotaPrompt,
                                                        browser);
      timeoutNotification();
      return;
    }

    notification = PopupNotifications.show(browser, topic, message,
                                           this._notificationIcon, mainAction,
                                           secondaryActions, options);

    // Set the timeoutId after the popup has been created, and use the long
    // timeout value. If the user doesn't notice the popup after this amount of
    // time then it is most likely not visible and we want to alert the page.
    timeoutId = setTimeout(timeoutNotification, firstTimeoutDuration);
  }
};

function WindowIsClosing()
{
  if (TabView.isVisible()) {
    TabView.hide();
    return false;
  }

  if (!closeWindow(false, warnAboutClosingWindow))
    return false;

  for (let browser of gBrowser.browsers) {
    let ds = browser.docShell;
    if (ds.contentViewer && !ds.contentViewer.permitUnload())
      return false;
  }

  return true;
}

/**
 * Checks if this is the last full *browser* window around. If it is, this will
 * be communicated like quitting. Otherwise, we warn about closing multiple tabs.
 * @returns true if closing can proceed, false if it got cancelled.
 */
function warnAboutClosingWindow() {
  // Popups aren't considered full browser windows.
  let isPBWindow = PrivateBrowsingUtils.isWindowPrivate(window);
  if (!isPBWindow && !toolbar.visible)
    return gBrowser.warnAboutClosingTabs(true);

  // Figure out if there's at least one other browser window around.
  let e = Services.wm.getEnumerator("navigator:browser");
  let otherPBWindowExists = false;
  let warnAboutClosingTabs = false;
  while (e.hasMoreElements()) {
    let win = e.getNext();
    if (win != window) {
      if (isPBWindow && PrivateBrowsingUtils.isWindowPrivate(win))
        otherPBWindowExists = true;
      if (win.toolbar.visible)
        warnAboutClosingTabs = true;
      // If the current window is not in private browsing mode we don't need to 
      // look for other pb windows, we can leave the loop when finding the 
      // first non-popup window. If however the current window is in private 
      // browsing mode then we need at least one other pb and one non-popup 
      // window to break out early.
      if ((!isPBWindow || otherPBWindowExists) && warnAboutClosingTabs)
        break;
    }
  }

  if (isPBWindow && !otherPBWindowExists) {
    let exitingCanceled = Cc["@mozilla.org/supports-PRBool;1"].
                          createInstance(Ci.nsISupportsPRBool);
    exitingCanceled.data = false;
    Services.obs.notifyObservers(exitingCanceled,
                                 "last-pb-context-exiting",
                                 null);
    if (exitingCanceled.data)
      return false;
  }
  if (warnAboutClosingTabs)
    return gBrowser.warnAboutClosingTabs(true);

  let os = Services.obs;

  let closingCanceled = Cc["@mozilla.org/supports-PRBool;1"].
                        createInstance(Ci.nsISupportsPRBool);
  os.notifyObservers(closingCanceled,
                     "browser-lastwindow-close-requested", null);
  if (closingCanceled.data)
    return false;

  os.notifyObservers(null, "browser-lastwindow-close-granted", null);

#ifdef XP_MACOSX
  // OS X doesn't quit the application when the last window is closed, but keeps
  // the session alive. Hence don't prompt users to save tabs, but warn about
  // closing multiple tabs.
  return gBrowser.warnAboutClosingTabs(true);
#else
  return true;
#endif
}

var MailIntegration = {
  sendLinkForWindow: function (aWindow) {
    this.sendMessage(aWindow.location.href,
                     aWindow.document.title);
  },

  sendMessage: function (aBody, aSubject) {
    // generate a mailto url based on the url and the url's title
    var mailtoUrl = "mailto:";
    if (aBody) {
      mailtoUrl += "?body=" + encodeURIComponent(aBody);
      mailtoUrl += "&subject=" + encodeURIComponent(aSubject);
    }

    var uri = makeURI(mailtoUrl);

    // now pass this uri to the operating system
    this._launchExternalUrl(uri);
  },

  // a generic method which can be used to pass arbitrary urls to the operating
  // system.
  // aURL --> a nsIURI which represents the url to launch
  _launchExternalUrl: function (aURL) {
    var extProtocolSvc =
       Cc["@mozilla.org/uriloader/external-protocol-service;1"]
         .getService(Ci.nsIExternalProtocolService);
    if (extProtocolSvc)
      extProtocolSvc.loadUrl(aURL);
  }
};

function BrowserOpenAddonsMgr(aView) {
  if (aView) {
    let emWindow;
    let browserWindow;

    var receivePong = function receivePong(aSubject, aTopic, aData) {
      let browserWin = aSubject.QueryInterface(Ci.nsIInterfaceRequestor)
                               .getInterface(Ci.nsIWebNavigation)
                               .QueryInterface(Ci.nsIDocShellTreeItem)
                               .rootTreeItem
                               .QueryInterface(Ci.nsIInterfaceRequestor)
                               .getInterface(Ci.nsIDOMWindow);
      if (!emWindow || browserWin == window /* favor the current window */) {
        emWindow = aSubject;
        browserWindow = browserWin;
      }
    }
    Services.obs.addObserver(receivePong, "EM-pong", false);
    Services.obs.notifyObservers(null, "EM-ping", "");
    Services.obs.removeObserver(receivePong, "EM-pong");

    if (emWindow) {
      emWindow.loadView(aView);
      browserWindow.gBrowser.selectedTab =
        browserWindow.gBrowser._getTabForContentWindow(emWindow);
      emWindow.focus();
      return;
    }
  }

  var newLoad = !switchToTabHavingURI("about:addons", true);

  if (aView) {
    // This must be a new load, else the ping/pong would have
    // found the window above.
    Services.obs.addObserver(function observer(aSubject, aTopic, aData) {
      Services.obs.removeObserver(observer, aTopic);
      aSubject.loadView(aView);
    }, "EM-loaded", false);
  }
}

function AddKeywordForSearchField() {
  var node = document.popupNode;

  var charset = node.ownerDocument.characterSet;

  var docURI = makeURI(node.ownerDocument.URL,
                       charset);

  var formURI = makeURI(node.form.getAttribute("action"),
                        charset,
                        docURI);

  var spec = formURI.spec;

  var isURLEncoded =
               (node.form.method.toUpperCase() == "POST"
                && (node.form.enctype == "application/x-www-form-urlencoded" ||
                    node.form.enctype == ""));

  var title = gNavigatorBundle.getFormattedString("addKeywordTitleAutoFill",
                                                  [node.ownerDocument.title]);
  var description = PlacesUIUtils.getDescriptionFromDocument(node.ownerDocument);

  var formData = [];

  function escapeNameValuePair(aName, aValue, aIsFormUrlEncoded) {
    if (aIsFormUrlEncoded)
      return escape(aName + "=" + aValue);
    else
      return escape(aName) + "=" + escape(aValue);
  }

  for (let el of node.form.elements) {
    if (!el.type) // happens with fieldsets
      continue;

    if (el == node) {
      formData.push((isURLEncoded) ? escapeNameValuePair(el.name, "%s", true) :
                                     // Don't escape "%s", just append
                                     escapeNameValuePair(el.name, "", false) + "%s");
      continue;
    }

    let type = el.type.toLowerCase();

    if (((el instanceof HTMLInputElement && el.mozIsTextField(true)) ||
        type == "hidden" || type == "textarea") ||
        ((type == "checkbox" || type == "radio") && el.checked)) {
      formData.push(escapeNameValuePair(el.name, el.value, isURLEncoded));
    } else if (el instanceof HTMLSelectElement && el.selectedIndex >= 0) {
      for (var j=0; j < el.options.length; j++) {
        if (el.options[j].selected)
          formData.push(escapeNameValuePair(el.name, el.options[j].value,
                                            isURLEncoded));
      }
    }
  }

  var postData;

  if (isURLEncoded)
    postData = formData.join("&");
  else
    spec += "?" + formData.join("&");

  PlacesUIUtils.showBookmarkDialog({ action: "add"
                                   , type: "bookmark"
                                   , uri: makeURI(spec)
                                   , title: title
                                   , description: description
                                   , keyword: ""
                                   , postData: postData
                                   , charSet: charset
                                   , hiddenRows: [ "location"
                                                 , "description"
                                                 , "tags"
                                                 , "loadInSidebar" ]
                                   }, window);
}

function SwitchDocumentDirection(aWindow) {
  aWindow.document.dir = (aWindow.document.dir == "ltr" ? "rtl" : "ltr");
  for (var run = 0; run < aWindow.frames.length; run++)
    SwitchDocumentDirection(aWindow.frames[run]);
}

function convertFromUnicode(charset, str)
{
  try {
    var unicodeConverter = Components
       .classes["@mozilla.org/intl/scriptableunicodeconverter"]
       .createInstance(Components.interfaces.nsIScriptableUnicodeConverter);
    unicodeConverter.charset = charset;
    str = unicodeConverter.ConvertFromUnicode(str);
    return str + unicodeConverter.Finish();
  } catch(ex) {
    return null;
  }
}

/**
 * Re-open a closed tab.
 * @param aIndex
 *        The index of the tab (via nsSessionStore.getClosedTabData)
 * @returns a reference to the reopened tab.
 */
function undoCloseTab(aIndex) {
  // wallpaper patch to prevent an unnecessary blank tab (bug 343895)
  var blankTabToRemove = null;
  if (gBrowser.tabs.length == 1 &&
      !gPrefService.getBoolPref("browser.tabs.autoHide") &&
      isTabEmpty(gBrowser.selectedTab))
    blankTabToRemove = gBrowser.selectedTab;

  var tab = null;
  var ss = Cc["@mozilla.org/browser/sessionstore;1"].
           getService(Ci.nsISessionStore);
  if (ss.getClosedTabCount(window) > (aIndex || 0)) {
    TabView.prepareUndoCloseTab(blankTabToRemove);
    tab = ss.undoCloseTab(window, aIndex || 0);
    TabView.afterUndoCloseTab();

    if (blankTabToRemove)
      gBrowser.removeTab(blankTabToRemove);
  }

  return tab;
}

/**
 * Re-open a closed window.
 * @param aIndex
 *        The index of the window (via nsSessionStore.getClosedWindowData)
 * @returns a reference to the reopened window.
 */
function undoCloseWindow(aIndex) {
  let ss = Cc["@mozilla.org/browser/sessionstore;1"].
           getService(Ci.nsISessionStore);
  let window = null;
  if (ss.getClosedWindowCount() > (aIndex || 0))
    window = ss.undoCloseWindow(aIndex || 0);

  return window;
}

/*
 * Determines if a tab is "empty", usually used in the context of determining
 * if it's ok to close the tab.
 */
function isTabEmpty(aTab) {
  if (aTab.hasAttribute("busy"))
    return false;

  let browser = aTab.linkedBrowser;
  if (!isBlankPageURL(browser.currentURI.spec))
    return false;

  if (browser.contentWindow.opener)
    return false;

  if (browser.sessionHistory && browser.sessionHistory.count >= 2)
    return false;

  return true;
}

#ifdef MOZ_SERVICES_SYNC
function BrowserOpenSyncTabs() {
  switchToTabHavingURI("about:sync-tabs", true);
}
#endif

/**
 * Format a URL
 * eg:
 * echo formatURL("https://addons.mozilla.org/%LOCALE%/%APP%/%VERSION%/");
 * > https://addons.mozilla.org/en-US/firefox/3.0a1/
 *
 * Currently supported built-ins are LOCALE, APP, and any value from nsIXULAppInfo, uppercased.
 */
function formatURL(aFormat, aIsPref) {
  var formatter = Cc["@mozilla.org/toolkit/URLFormatterService;1"].getService(Ci.nsIURLFormatter);
  return aIsPref ? formatter.formatURLPref(aFormat) : formatter.formatURL(aFormat);
}

/**
 * Utility object to handle manipulations of the identity indicators in the UI
 */
var gIdentityHandler = {
  // Mode strings used to control CSS display
  IDENTITY_MODE_IDENTIFIED       : "verifiedIdentity", // High-quality identity information
  IDENTITY_MODE_DOMAIN_VERIFIED  : "verifiedDomain",   // Minimal SSL CA-signed domain verification
  IDENTITY_MODE_UNKNOWN          : "unknownIdentity",  // No trusted identity information
  IDENTITY_MODE_MIXED_CONTENT    : "unknownIdentity mixedContent",  // SSL with unauthenticated content
  IDENTITY_MODE_CHROMEUI         : "chromeUI",         // Part of the product's UI

  // Cache the most recent SSLStatus and Location seen in checkIdentity
  _lastStatus : null,
  _lastLocation : null,
  _mode : "unknownIdentity",

  // smart getters
  get _encryptionLabel () {
    delete this._encryptionLabel;
    this._encryptionLabel = {};
    this._encryptionLabel[this.IDENTITY_MODE_DOMAIN_VERIFIED] =
      gNavigatorBundle.getString("identity.encrypted");
    this._encryptionLabel[this.IDENTITY_MODE_IDENTIFIED] =
      gNavigatorBundle.getString("identity.encrypted");
    this._encryptionLabel[this.IDENTITY_MODE_UNKNOWN] =
      gNavigatorBundle.getString("identity.unencrypted");
    this._encryptionLabel[this.IDENTITY_MODE_MIXED_CONTENT] =
      gNavigatorBundle.getString("identity.mixed_content");
    return this._encryptionLabel;
  },
  get _identityPopup () {
    delete this._identityPopup;
    return this._identityPopup = document.getElementById("identity-popup");
  },
  get _identityBox () {
    delete this._identityBox;
    return this._identityBox = document.getElementById("identity-box");
  },
  get _identityPopupContentBox () {
    delete this._identityPopupContentBox;
    return this._identityPopupContentBox =
      document.getElementById("identity-popup-content-box");
  },
  get _identityPopupContentHost () {
    delete this._identityPopupContentHost;
    return this._identityPopupContentHost =
      document.getElementById("identity-popup-content-host");
  },
  get _identityPopupContentOwner () {
    delete this._identityPopupContentOwner;
    return this._identityPopupContentOwner =
      document.getElementById("identity-popup-content-owner");
  },
  get _identityPopupContentSupp () {
    delete this._identityPopupContentSupp;
    return this._identityPopupContentSupp =
      document.getElementById("identity-popup-content-supplemental");
  },
  get _identityPopupContentVerif () {
    delete this._identityPopupContentVerif;
    return this._identityPopupContentVerif =
      document.getElementById("identity-popup-content-verifier");
  },
  get _identityPopupEncLabel () {
    delete this._identityPopupEncLabel;
    return this._identityPopupEncLabel =
      document.getElementById("identity-popup-encryption-label");
  },
  get _identityIconLabel () {
    delete this._identityIconLabel;
    return this._identityIconLabel = document.getElementById("identity-icon-label");
  },
  get _overrideService () {
    delete this._overrideService;
    return this._overrideService = Cc["@mozilla.org/security/certoverride;1"]
                                     .getService(Ci.nsICertOverrideService);
  },
  get _identityIconCountryLabel () {
    delete this._identityIconCountryLabel;
    return this._identityIconCountryLabel = document.getElementById("identity-icon-country-label");
  },
  get _identityIcon () {
    delete this._identityIcon;
    return this._identityIcon = document.getElementById("page-proxy-favicon");
  },

  /**
   * Rebuild cache of the elements that may or may not exist depending
   * on whether there's a location bar.
   */
  _cacheElements : function() {
    delete this._identityBox;
    delete this._identityIconLabel;
    delete this._identityIconCountryLabel;
    delete this._identityIcon;
    this._identityBox = document.getElementById("identity-box");
    this._identityIconLabel = document.getElementById("identity-icon-label");
    this._identityIconCountryLabel = document.getElementById("identity-icon-country-label");
    this._identityIcon = document.getElementById("page-proxy-favicon");
  },

  /**
   * Handler for mouseclicks on the "More Information" button in the
   * "identity-popup" panel.
   */
  handleMoreInfoClick : function(event) {
    displaySecurityInfo();
    event.stopPropagation();
  },

  /**
   * Helper to parse out the important parts of _lastStatus (of the SSL cert in
   * particular) for use in constructing identity UI strings
  */
  getIdentityData : function() {
    var result = {};
    var status = this._lastStatus.QueryInterface(Components.interfaces.nsISSLStatus);
    var cert = status.serverCert;

    // Human readable name of Subject
    result.subjectOrg = cert.organization;

    // SubjectName fields, broken up for individual access
    if (cert.subjectName) {
      result.subjectNameFields = {};
      cert.subjectName.split(",").forEach(function(v) {
        var field = v.split("=");
        this[field[0]] = field[1];
      }, result.subjectNameFields);

      // Call out city, state, and country specifically
      result.city = result.subjectNameFields.L;
      result.state = result.subjectNameFields.ST;
      result.country = result.subjectNameFields.C;
    }

    // Human readable name of Certificate Authority
    result.caOrg =  cert.issuerOrganization || cert.issuerCommonName;
    result.cert = cert;

    return result;
  },

  /**
   * Determine the identity of the page being displayed by examining its SSL cert
   * (if available) and, if necessary, update the UI to reflect this.  Intended to
   * be called by onSecurityChange
   *
   * @param PRUint32 state
   * @param JS Object location that mirrors an nsLocation (i.e. has .host and
   *                           .hostname and .port)
   */
  checkIdentity : function(state, location) {
    var currentStatus = gBrowser.securityUI
                                .QueryInterface(Components.interfaces.nsISSLStatusProvider)
                                .SSLStatus;
    this._lastStatus = currentStatus;
    this._lastLocation = location;

    let nsIWebProgressListener = Ci.nsIWebProgressListener;
    if (location.protocol == "chrome:" || location.protocol == "about:")
      this.setMode(this.IDENTITY_MODE_CHROMEUI);
    else if (state & nsIWebProgressListener.STATE_IDENTITY_EV_TOPLEVEL)
      this.setMode(this.IDENTITY_MODE_IDENTIFIED);
    else if (state & nsIWebProgressListener.STATE_IS_SECURE)
      this.setMode(this.IDENTITY_MODE_DOMAIN_VERIFIED);
    else if (state & nsIWebProgressListener.STATE_IS_BROKEN)
      this.setMode(this.IDENTITY_MODE_MIXED_CONTENT);
    else
      this.setMode(this.IDENTITY_MODE_UNKNOWN);
  },

  /**
   * Return the eTLD+1 version of the current hostname
   */
  getEffectiveHost : function() {
    // Cache the eTLDService if this is our first time through
    if (!this._eTLDService)
      this._eTLDService = Cc["@mozilla.org/network/effective-tld-service;1"]
                         .getService(Ci.nsIEffectiveTLDService);
    if (!this._IDNService)
      this._IDNService = Cc["@mozilla.org/network/idn-service;1"]
                         .getService(Ci.nsIIDNService);
    try {
      let baseDomain =
        this._eTLDService.getBaseDomainFromHost(this._lastLocation.hostname);
      return this._IDNService.convertToDisplayIDN(baseDomain, {});
    } catch (e) {
      // If something goes wrong (e.g. hostname is an IP address) just fail back
      // to the full domain.
      return this._lastLocation.hostname;
    }
  },

  /**
   * Update the UI to reflect the specified mode, which should be one of the
   * IDENTITY_MODE_* constants.
   */
  setMode : function(newMode) {
    if (!this._identityBox) {
      // No identity box means the identity box is not visible, in which
      // case there's nothing to do.
      return;
    }

    this._identityBox.className = newMode;
    this.setIdentityMessages(newMode);

    // Update the popup too, if it's open
    if (this._identityPopup.state == "open")
      this.setPopupMessages(newMode);

    this._mode = newMode;
  },

  /**
   * Set up the messages for the primary identity UI based on the specified mode,
   * and the details of the SSL cert, where applicable
   *
   * @param newMode The newly set identity mode.  Should be one of the IDENTITY_MODE_* constants.
   */
  setIdentityMessages : function(newMode) {
    let icon_label = "";
    let tooltip = "";
    let icon_country_label = "";
    let icon_labels_dir = "ltr";

    switch (newMode) {
    case this.IDENTITY_MODE_DOMAIN_VERIFIED: {
      let iData = this.getIdentityData();

      // Verifier is either the CA Org, for a normal cert, or a special string
      // for certs that are trusted because of a security exception.
      tooltip = gNavigatorBundle.getFormattedString("identity.identified.verifier",
                                                    [iData.caOrg]);

      // Check whether this site is a security exception. XPConnect does the right
      // thing here in terms of converting _lastLocation.port from string to int, but
      // the overrideService doesn't like undefined ports, so make sure we have
      // something in the default case (bug 432241).
      // .hostname can return an empty string in some exceptional cases -
      // hasMatchingOverride does not handle that, so avoid calling it.
      // Updating the tooltip value in those cases isn't critical.
      // FIXME: Fixing bug 646690 would probably makes this check unnecessary
      if (this._lastLocation.hostname &&
          this._overrideService.hasMatchingOverride(this._lastLocation.hostname,
                                                    (this._lastLocation.port || 443),
                                                    iData.cert, {}, {}))
        tooltip = gNavigatorBundle.getString("identity.identified.verified_by_you");
      break; }
    case this.IDENTITY_MODE_IDENTIFIED: {
      // If it's identified, then we can populate the dialog with credentials
      let iData = this.getIdentityData();
      tooltip = gNavigatorBundle.getFormattedString("identity.identified.verifier",
                                                    [iData.caOrg]);
      icon_label = iData.subjectOrg;
      if (iData.country)
        icon_country_label = "(" + iData.country + ")";

      // If the organization name starts with an RTL character, then
      // swap the positions of the organization and country code labels.
      // The Unicode ranges reflect the definition of the UCS2_CHAR_IS_BIDI
      // macro in intl/unicharutil/util/nsBidiUtils.h. When bug 218823 gets
      // fixed, this test should be replaced by one adhering to the
      // Unicode Bidirectional Algorithm proper (at the paragraph level).
      icon_labels_dir = /^[\u0590-\u08ff\ufb1d-\ufdff\ufe70-\ufefc]/.test(icon_label) ?
                        "rtl" : "ltr";
      break; }
    case this.IDENTITY_MODE_CHROMEUI:
      break;
    default:
      tooltip = gNavigatorBundle.getString("identity.unknown.tooltip");
    }

    // Push the appropriate strings out to the UI
    this._identityBox.tooltipText = tooltip;
    this._identityIconLabel.value = icon_label;
    this._identityIconCountryLabel.value = icon_country_label;
    // Set cropping and direction
    this._identityIconLabel.crop = icon_country_label ? "end" : "center";
    this._identityIconLabel.parentNode.style.direction = icon_labels_dir;
    // Hide completely if the organization label is empty
    this._identityIconLabel.parentNode.collapsed = icon_label ? false : true;
  },

  /**
   * Set up the title and content messages for the identity message popup,
   * based on the specified mode, and the details of the SSL cert, where
   * applicable
   *
   * @param newMode The newly set identity mode.  Should be one of the IDENTITY_MODE_* constants.
   */
  setPopupMessages : function(newMode) {

    this._identityPopup.className = newMode;
    this._identityPopupContentBox.className = newMode;

    // Set the static strings up front
    this._identityPopupEncLabel.textContent = this._encryptionLabel[newMode];

    // Initialize the optional strings to empty values
    let supplemental = "";
    let verifier = "";
    let host = "";
    let owner = "";

    switch (newMode) {
    case this.IDENTITY_MODE_DOMAIN_VERIFIED:
      host = this.getEffectiveHost();
      owner = gNavigatorBundle.getString("identity.ownerUnknown2");
      verifier = this._identityBox.tooltipText;
      break;
    case this.IDENTITY_MODE_IDENTIFIED: {
      // If it's identified, then we can populate the dialog with credentials
      let iData = this.getIdentityData();
      host = this.getEffectiveHost();
      owner = iData.subjectOrg;
      verifier = this._identityBox.tooltipText;

      // Build an appropriate supplemental block out of whatever location data we have
      if (iData.city)
        supplemental += iData.city + "\n";
      if (iData.state && iData.country)
        supplemental += gNavigatorBundle.getFormattedString("identity.identified.state_and_country",
                                                            [iData.state, iData.country]);
      else if (iData.state) // State only
        supplemental += iData.state;
      else if (iData.country) // Country only
        supplemental += iData.country;
      break; }
    }

    // Push the appropriate strings out to the UI
    this._identityPopupContentHost.textContent = host;
    this._identityPopupContentOwner.textContent = owner;
    this._identityPopupContentSupp.textContent = supplemental;
    this._identityPopupContentVerif.textContent = verifier;
  },

  hideIdentityPopup : function() {
    this._identityPopup.hidePopup();
  },

  _popupOpenTime : null,

  /**
   * Click handler for the identity-box element in primary chrome.
   */
  handleIdentityButtonEvent : function(event) {
    this._popupOpenTime = new Date();
    event.stopPropagation();

    if ((event.type == "click" && event.button != 0) ||
        (event.type == "keypress" && event.charCode != KeyEvent.DOM_VK_SPACE &&
         event.keyCode != KeyEvent.DOM_VK_RETURN))
      return; // Left click, space or enter only

    // Don't allow left click, space or enter if the location
    // is chrome UI or the location has been modified.
    if (this._mode == this.IDENTITY_MODE_CHROMEUI ||
        gURLBar.getAttribute("pageproxystate") != "valid")
      return;

    // Make sure that the display:none style we set in xul is removed now that
    // the popup is actually needed
    this._identityPopup.hidden = false;

    // Update the popup strings
    this.setPopupMessages(this._identityBox.className);

    // Add the "open" attribute to the identity box for styling
    this._identityBox.setAttribute("open", "true");
    var self = this;
    this._identityPopup.addEventListener("popuphidden", function onPopupHidden(e) {
      e.currentTarget.removeEventListener("popuphidden", onPopupHidden, false);
      self._identityBox.removeAttribute("open");
    }, false);

    // Now open the popup, anchored off the primary chrome element
    this._identityPopup.openPopup(this._identityIcon, "bottomcenter topleft");
  },

  onPopupShown : function(event) {
    let openingDuration = new Date() - this._popupOpenTime;
    this._popupOpenTime = null;
    try {
      Services.telemetry.getHistogramById("FX_IDENTITY_POPUP_OPEN_MS").add(openingDuration);
    } catch (ex) {
      Components.utils.reportError("Unable to report telemetry for FX_IDENTITY_POPUP_OPEN_MS.");
    }
    document.getElementById('identity-popup-more-info-button').focus();
  },

  onDragStart: function (event) {
    if (gURLBar.getAttribute("pageproxystate") != "valid")
      return;

    var value = content.location.href;
    var urlString = value + "\n" + content.document.title;
    var htmlString = "<a href=\"" + value + "\">" + value + "</a>";

    var dt = event.dataTransfer;
    dt.setData("text/x-moz-url", urlString);
    dt.setData("text/uri-list", value);
    dt.setData("text/plain", value);
    dt.setData("text/html", htmlString);
    dt.setDragImage(gProxyFavIcon, 16, 16);
  }
};

let DownloadMonitorPanel = {
  //////////////////////////////////////////////////////////////////////////////
  //// DownloadMonitorPanel Member Variables

  _panel: null,
  _activeStr: null,
  _pausedStr: null,
  _lastTime: Infinity,
  _listening: false,

  get DownloadUtils() {
    delete this.DownloadUtils;
    Cu.import("resource://gre/modules/DownloadUtils.jsm", this);
    return this.DownloadUtils;
  },

  //////////////////////////////////////////////////////////////////////////////
  //// DownloadMonitorPanel Public Methods

  /**
   * Initialize the status panel and member variables
   */
  init: function DMP_init() {
    // Initialize "private" member variables
    this._panel = document.getElementById("download-monitor");

    // Cache the status strings
    this._activeStr = gNavigatorBundle.getString("activeDownloads1");
    this._pausedStr = gNavigatorBundle.getString("pausedDownloads1");

    gDownloadMgr.addListener(this);
    this._listening = true;

    this.updateStatus();
  },

  uninit: function DMP_uninit() {
    if (this._listening)
      gDownloadMgr.removeListener(this);
  },

  inited: function DMP_inited() {
    return this._panel != null;
  },

  /**
   * Update status based on the number of active and paused downloads
   */
  updateStatus: function DMP_updateStatus() {
    if (!this.inited())
      return;

    let numActive = gDownloadMgr.activeDownloadCount;

    // Hide the panel and reset the "last time" if there's no downloads
    if (numActive == 0) {
      this._panel.hidden = true;
      this._lastTime = Infinity;

      return;
    }

    // Find the download with the longest remaining time
    let numPaused = 0;
    let maxTime = -Infinity;
    let dls = gDownloadMgr.activeDownloads;
    while (dls.hasMoreElements()) {
      let dl = dls.getNext();
      if (dl.state == gDownloadMgr.DOWNLOAD_DOWNLOADING) {
        // Figure out if this download takes longer
        if (dl.speed > 0 && dl.size > 0)
          maxTime = Math.max(maxTime, (dl.size - dl.amountTransferred) / dl.speed);
        else
          maxTime = -1;
      }
      else if (dl.state == gDownloadMgr.DOWNLOAD_PAUSED)
        numPaused++;
    }

    // Get the remaining time string and last sec for time estimation
    let timeLeft;
    [timeLeft, this._lastTime] =
      this.DownloadUtils.getTimeLeft(maxTime, this._lastTime);

    // Figure out how many downloads are currently downloading
    let numDls = numActive - numPaused;
    let status = this._activeStr;

    // If all downloads are paused, show the paused message instead
    if (numDls == 0) {
      numDls = numPaused;
      status = this._pausedStr;
    }

    // Get the correct plural form and insert the number of downloads and time
    // left message if necessary
    status = PluralForm.get(numDls, status);
    status = status.replace("#1", numDls);
    status = status.replace("#2", timeLeft);

    // Update the panel and show it
    this._panel.label = status;
    this._panel.hidden = false;
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsIDownloadProgressListener

  /**
   * Update status for download progress changes
   */
  onProgressChange: function() {
    this.updateStatus();
  },

  /**
   * Update status for download state changes
   */
  onDownloadStateChange: function() {
    this.updateStatus();
  },

  onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus, aDownload) {
  },

  onSecurityChange: function(aWebProgress, aRequest, aState, aDownload) {
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsISupports

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDownloadProgressListener]),
};

function getNotificationBox(aWindow) {
  var foundBrowser = gBrowser.getBrowserForDocument(aWindow.document);
  if (foundBrowser)
    return gBrowser.getNotificationBox(foundBrowser)
  return null;
};

function getTabModalPromptBox(aWindow) {
  var foundBrowser = gBrowser.getBrowserForDocument(aWindow.document);
  if (foundBrowser)
    return gBrowser.getTabModalPromptBox(foundBrowser);
  return null;
};

/* DEPRECATED */
function getBrowser() gBrowser;
function getNavToolbox() gNavToolbox;

#ifdef MOZ_PER_WINDOW_PRIVATE_BROWSING

# We define a new gPrivateBrowsingUI object, as the per-window PB implementation
# is completely different to the global PB one.  Specifically, the per-window
# PB implementation does not expose many APIs on the gPrivateBrowsingUI object,
# and only uses it as a way to initialize and uninitialize private browsing
# windows.  While we could use #ifdefs all around the global PB mode code to
# make it work in both modes, the amount of duplicated code is small and the
# code is much more readable this way.
let gPrivateBrowsingUI = {
  init: function PBUI_init() {
    // Do nothing for normal windows
    if (!PrivateBrowsingUtils.isWindowPrivate(window)) {
      return;
    }

    // Disable the Clear Recent History... menu item when in PB mode
    // temporary fix until bug 463607 is fixed
    document.getElementById("Tools:Sanitize").setAttribute("disabled", "true");

    // Adjust the window's title
    if (window.location.href == getBrowserURL()) {
      let docElement = document.documentElement;
      docElement.setAttribute("title",
        docElement.getAttribute("title_privatebrowsing"));
      docElement.setAttribute("titlemodifier",
        docElement.getAttribute("titlemodifier_privatebrowsing"));
      docElement.setAttribute("privatebrowsingmode", "temporary");
      gBrowser.updateTitlebar();
    }

    if (gURLBar) {
      // Disable switch to tab autocompletion for private windows
      gURLBar.setAttribute("autocompletesearchparam", "");
    }
  }
};

#else

let gPrivateBrowsingUI = {
  _privateBrowsingService: null,
  _searchBarValue: null,
  _findBarValue: null,
  _inited: false,

  init: function PBUI_init() {
    Services.obs.addObserver(this, "private-browsing", false);
    Services.obs.addObserver(this, "private-browsing-transition-complete", false);

    this._privateBrowsingService = Cc["@mozilla.org/privatebrowsing;1"].
                                   getService(Ci.nsIPrivateBrowsingService);

    if (this.privateBrowsingEnabled)
      this.onEnterPrivateBrowsing(true);

    this._inited = true;
  },

  uninit: function PBUI_unint() {
    if (!this._inited)
      return;

    Services.obs.removeObserver(this, "private-browsing");
    Services.obs.removeObserver(this, "private-browsing-transition-complete");
  },

  get _disableUIOnToggle() {
    if (PrivateBrowsingUtils.permanentPrivateBrowsing)
      return false;

    try {
      return !gPrefService.getBoolPref("browser.privatebrowsing.keep_current_session");
    }
    catch (e) {
      return true;
    }
  },

  observe: function PBUI_observe(aSubject, aTopic, aData) {
    if (aTopic == "private-browsing") {
      if (aData == "enter")
        this.onEnterPrivateBrowsing();
      else if (aData == "exit")
        this.onExitPrivateBrowsing();
    }
    else if (aTopic == "private-browsing-transition-complete") {
      if (this._disableUIOnToggle) {
        document.getElementById("Tools:PrivateBrowsing")
                .removeAttribute("disabled");
      }
    }
  },

  _shouldEnter: function PBUI__shouldEnter() {
    try {
      // Never prompt if the session is not going to be closed, or if user has
      // already requested not to be prompted.
      if (gPrefService.getBoolPref("browser.privatebrowsing.dont_prompt_on_enter") ||
          gPrefService.getBoolPref("browser.privatebrowsing.keep_current_session"))
        return true;
    }
    catch (ex) { }

    var bundleService = Cc["@mozilla.org/intl/stringbundle;1"].
                        getService(Ci.nsIStringBundleService);
    var pbBundle = bundleService.createBundle("chrome://browser/locale/browser.properties");
    var brandBundle = bundleService.createBundle("chrome://branding/locale/brand.properties");

    var appName = brandBundle.GetStringFromName("brandShortName");
# On Mac, use the header as the title.
#ifdef XP_MACOSX
    var dialogTitle = pbBundle.GetStringFromName("privateBrowsingMessageHeader");
    var header = "";
#else
    var dialogTitle = pbBundle.GetStringFromName("privateBrowsingDialogTitle");
    var header = pbBundle.GetStringFromName("privateBrowsingMessageHeader") + "\n\n";
#endif
    var message = pbBundle.formatStringFromName("privateBrowsingMessage", [appName], 1);

    var ps = Services.prompt;

    var flags = ps.BUTTON_TITLE_IS_STRING * ps.BUTTON_POS_0 +
                ps.BUTTON_TITLE_IS_STRING * ps.BUTTON_POS_1 +
                ps.BUTTON_POS_0_DEFAULT;

    var neverAsk = {value:false};
    var button0Title = pbBundle.GetStringFromName("privateBrowsingYesTitle");
    var button1Title = pbBundle.GetStringFromName("privateBrowsingNoTitle");
    var neverAskText = pbBundle.GetStringFromName("privateBrowsingNeverAsk");

    var result;
    var choice = ps.confirmEx(null, dialogTitle, header + message,
                              flags, button0Title, button1Title, null,
                              neverAskText, neverAsk);

    switch (choice) {
    case 0: // Start Private Browsing
      result = true;
      if (neverAsk.value)
        gPrefService.setBoolPref("browser.privatebrowsing.dont_prompt_on_enter", true);
      break;
    case 1: // Keep
      result = false;
      break;
    }

    return result;
  },

  onEnterPrivateBrowsing: function PBUI_onEnterPrivateBrowsing(aOnWindowOpen) {
    if (BrowserSearch.searchBar)
      this._searchBarValue = BrowserSearch.searchBar.textbox.value;

    if (gFindBarInitialized)
      this._findBarValue = gFindBar.getElement("findbar-textbox").value;

    this._setPBMenuTitle("stop");

    // Disable the Clear Recent History... menu item when in PB mode
    // temporary fix until bug 463607 is fixed
    document.getElementById("Tools:Sanitize").setAttribute("disabled", "true");

    let docElement = document.documentElement;
    if (PrivateBrowsingUtils.permanentPrivateBrowsing) {
      // Disable the menu item in auto-start mode
      document.getElementById("privateBrowsingItem")
              .setAttribute("disabled", "true");
#ifdef MENUBAR_CAN_AUTOHIDE
      document.getElementById("appmenu_privateBrowsing")
              .setAttribute("disabled", "true");
#endif
      document.getElementById("Tools:PrivateBrowsing")
              .setAttribute("disabled", "true");
      if (window.location.href == getBrowserURL())
        docElement.setAttribute("privatebrowsingmode", "permanent");
    }
    else if (window.location.href == getBrowserURL()) {
      // Adjust the window's title
      docElement.setAttribute("title",
        docElement.getAttribute("title_privatebrowsing"));
      docElement.setAttribute("titlemodifier",
        docElement.getAttribute("titlemodifier_privatebrowsing"));
      docElement.setAttribute("privatebrowsingmode", "temporary");
      gBrowser.updateTitlebar();
    }

    if (!aOnWindowOpen && this._disableUIOnToggle)
      document.getElementById("Tools:PrivateBrowsing")
              .setAttribute("disabled", "true");
  },

  onExitPrivateBrowsing: function PBUI_onExitPrivateBrowsing() {
    if (BrowserSearch.searchBar) {
      let searchBox = BrowserSearch.searchBar.textbox;
      searchBox.reset();
      if (this._searchBarValue) {
        searchBox.value = this._searchBarValue;
        this._searchBarValue = null;
      }
    }

    if (gURLBar) {
      gURLBar.editor.transactionManager.clear();
    }

    // Re-enable the Clear Recent History... menu item on exit of PB mode
    // temporary fix until bug 463607 is fixed
    document.getElementById("Tools:Sanitize").removeAttribute("disabled");

    if (gFindBarInitialized) {
      let findbox = gFindBar.getElement("findbar-textbox");
      findbox.reset();
      if (this._findBarValue) {
        findbox.value = this._findBarValue;
        this._findBarValue = null;
      }
    }

    this._setPBMenuTitle("start");

    if (window.location.href == getBrowserURL()) {
      // Adjust the window's title
      let docElement = document.documentElement;
      docElement.setAttribute("title",
        docElement.getAttribute("title_normal"));
      docElement.setAttribute("titlemodifier",
        docElement.getAttribute("titlemodifier_normal"));
      docElement.removeAttribute("privatebrowsingmode");
    }

    // Enable the menu item in after exiting the auto-start mode
    document.getElementById("privateBrowsingItem")
            .removeAttribute("disabled");
#ifdef MENUBAR_CAN_AUTOHIDE
    document.getElementById("appmenu_privateBrowsing")
            .removeAttribute("disabled");
#endif
    document.getElementById("Tools:PrivateBrowsing")
            .removeAttribute("disabled");

    gLastOpenDirectory.reset();

    if (this._disableUIOnToggle)
      document.getElementById("Tools:PrivateBrowsing")
              .setAttribute("disabled", "true");
  },

  _setPBMenuTitle: function PBUI__setPBMenuTitle(aMode) {
    let pbMenuItem = document.getElementById("privateBrowsingItem");
    pbMenuItem.setAttribute("label", pbMenuItem.getAttribute(aMode + "label"));
    pbMenuItem.setAttribute("accesskey", pbMenuItem.getAttribute(aMode + "accesskey"));
#ifdef MENUBAR_CAN_AUTOHIDE
    let appmenupbMenuItem = document.getElementById("appmenu_privateBrowsing");
    appmenupbMenuItem.setAttribute("label", appmenupbMenuItem.getAttribute(aMode + "label"));
    appmenupbMenuItem.setAttribute("accesskey", appmenupbMenuItem.getAttribute(aMode + "accesskey"));
#endif
  },

  toggleMode: function PBUI_toggleMode() {
    // prompt the users on entering the private mode, if needed
    if (!this.privateBrowsingEnabled)
      if (!this._shouldEnter())
        return;

    this._privateBrowsingService.privateBrowsingEnabled =
      !this.privateBrowsingEnabled;
  },

  get privateBrowsingEnabled() {
    return this._privateBrowsingService.privateBrowsingEnabled;
  }
};

#endif


/**
 * Switch to a tab that has a given URI, and focusses its browser window.
 * If a matching tab is in this window, it will be switched to. Otherwise, other
 * windows will be searched.
 *
 * @param aURI
 *        URI to search for
 * @param aOpenNew
 *        True to open a new tab and switch to it, if no existing tab is found.
 *        If no suitable window is found, a new one will be opened.
 * @return True if an existing tab was found, false otherwise
 */
function switchToTabHavingURI(aURI, aOpenNew) {
  // This will switch to the tab in aWindow having aURI, if present.
  function switchIfURIInWindow(aWindow) {
#ifdef MOZ_PER_WINDOW_PRIVATE_BROWSING
    // Only switch to the tab if neither the source and desination window are
    // private.
    if (PrivateBrowsingUtils.isWindowPrivate(window) ||
        PrivateBrowsingUtils.isWindowPrivate(aWindow)) {
      return false;
    }
#endif

    let browsers = aWindow.gBrowser.browsers;
    for (let i = 0; i < browsers.length; i++) {
      let browser = browsers[i];
      if (browser.currentURI.equals(aURI)) {
        // Focus the matching window & tab
        aWindow.focus();
        aWindow.gBrowser.tabContainer.selectedIndex = i;
        return true;
      }
    }
    return false;
  }

  // This can be passed either nsIURI or a string.
  if (!(aURI instanceof Ci.nsIURI))
    aURI = Services.io.newURI(aURI, null, null);

  let isBrowserWindow = !!window.gBrowser;

  // Prioritise this window.
  if (isBrowserWindow && switchIfURIInWindow(window))
    return true;

  let winEnum = Services.wm.getEnumerator("navigator:browser");
  while (winEnum.hasMoreElements()) {
    let browserWin = winEnum.getNext();
    // Skip closed (but not yet destroyed) windows,
    // and the current window (which was checked earlier).
    if (browserWin.closed || browserWin == window)
      continue;
    if (switchIfURIInWindow(browserWin))
      return true;
  }

  // No opened tab has that url.
  if (aOpenNew) {
    if (isBrowserWindow && isTabEmpty(gBrowser.selectedTab))
      gBrowser.selectedBrowser.loadURI(aURI.spec);
    else
      openUILinkIn(aURI.spec, "tab");
  }

  return false;
}

function restoreLastSession() {
  let ss = Cc["@mozilla.org/browser/sessionstore;1"].
           getService(Ci.nsISessionStore);
  ss.restoreLastSession();
}

var TabContextMenu = {
  contextTab: null,
  updateContextMenu: function updateContextMenu(aPopupMenu) {
    this.contextTab = aPopupMenu.triggerNode.localName == "tab" ?
                      aPopupMenu.triggerNode : gBrowser.selectedTab;
    let disabled = gBrowser.tabs.length == 1;

    // Enable the "Close Tab" menuitem when the window doesn't close with the last tab.
    document.getElementById("context_closeTab").disabled =
      disabled && gBrowser.tabContainer._closeWindowWithLastTab;

    var menuItems = aPopupMenu.getElementsByAttribute("tbattr", "tabbrowser-multiple");
    for (let menuItem of menuItems)
      menuItem.disabled = disabled;

    disabled = gBrowser.visibleTabs.length == 1;
    menuItems = aPopupMenu.getElementsByAttribute("tbattr", "tabbrowser-multiple-visible");
    for (let menuItem of menuItems)
      menuItem.disabled = disabled;

    // Session store
    document.getElementById("context_undoCloseTab").disabled =
      Cc["@mozilla.org/browser/sessionstore;1"].
      getService(Ci.nsISessionStore).
      getClosedTabCount(window) == 0;

    // Only one of pin/unpin should be visible
    document.getElementById("context_pinTab").hidden = this.contextTab.pinned;
    document.getElementById("context_unpinTab").hidden = !this.contextTab.pinned;

    // Disable "Close other Tabs" if there is only one unpinned tab and
    // hide it when the user rightclicked on a pinned tab.
    let unpinnedTabs = gBrowser.visibleTabs.length - gBrowser._numPinnedTabs;
    document.getElementById("context_closeOtherTabs").disabled = unpinnedTabs <= 1;
    document.getElementById("context_closeOtherTabs").hidden = this.contextTab.pinned;

    // Hide "Bookmark All Tabs" for a pinned tab.  Update its state if visible.
    let bookmarkAllTabs = document.getElementById("context_bookmarkAllTabs");
    bookmarkAllTabs.hidden = this.contextTab.pinned;
    if (!bookmarkAllTabs.hidden)
      PlacesCommandHook.updateBookmarkAllTabsCommand();

    // Hide "Move to Group" if it's a pinned tab.
    document.getElementById("context_tabViewMenu").hidden =
      (this.contextTab.pinned || !TabView.firstUseExperienced);
  }
};

XPCOMUtils.defineLazyModuleGetter(this, "gDevTools",
                                  "resource:///modules/devtools/gDevTools.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "gDevToolsBrowser",
                                  "resource:///modules/devtools/gDevTools.jsm");

XPCOMUtils.defineLazyGetter(this, "HUDConsoleUI", function () {
  let tempScope = {};
  Cu.import("resource:///modules/HUDService.jsm", tempScope);
  try {
    return tempScope.HUDService.consoleUI;
  }
  catch (ex) {
    Components.utils.reportError(ex);
    return null;
  }
});

// Prompt user to restart the browser in safe mode
function safeModeRestart()
{
  // prompt the user to confirm
  let promptTitle = gNavigatorBundle.getString("safeModeRestartPromptTitle");
  let promptMessage =
    gNavigatorBundle.getString("safeModeRestartPromptMessage");
  let restartText = gNavigatorBundle.getString("safeModeRestartButton");
  let buttonFlags = (Services.prompt.BUTTON_POS_0 *
                     Services.prompt.BUTTON_TITLE_IS_STRING) +
                    (Services.prompt.BUTTON_POS_1 *
                     Services.prompt.BUTTON_TITLE_CANCEL) +
                    Services.prompt.BUTTON_POS_0_DEFAULT;

  let rv = Services.prompt.confirmEx(window, promptTitle, promptMessage,
                                     buttonFlags, restartText, null, null,
                                     null, {});
  if (rv == 0) {
    let appStartup = Cc["@mozilla.org/toolkit/app-startup;1"].
                     getService(Ci.nsIAppStartup);
    appStartup.restartInSafeMode(Ci.nsIAppStartup.eAttemptQuit);
  }
}

/* duplicateTabIn duplicates tab in a place specified by the parameter |where|.
 *
 * |where| can be:
 *  "tab"         new tab
 *  "tabshifted"  same as "tab" but in background if default is to select new
 *                tabs, and vice versa
 *  "window"      new window
 *
 * delta is the offset to the history entry that you want to load.
 */
function duplicateTabIn(aTab, where, delta) {
  let newTab = Cc['@mozilla.org/browser/sessionstore;1']
                 .getService(Ci.nsISessionStore)
                 .duplicateTab(window, aTab, delta);

  switch (where) {
    case "window":
      gBrowser.hideTab(newTab);
      gBrowser.replaceTabWithWindow(newTab);
      break;
    case "tabshifted":
      // A background tab has been opened, nothing else to do here.
      break;
    case "tab":
      gBrowser.selectedTab = newTab;
      break;
  }
}

function toggleAddonBar() {
  let addonBar = document.getElementById("addon-bar");
  setToolbarVisibility(addonBar, addonBar.collapsed);
}

var Scratchpad = {
  prefEnabledName: "devtools.scratchpad.enabled",

  openScratchpad: function SP_openScratchpad() {
    return this.ScratchpadManager.openScratchpad();
  }
};

XPCOMUtils.defineLazyGetter(Scratchpad, "ScratchpadManager", function() {
  let tmp = {};
  Cu.import("resource:///modules/devtools/scratchpad-manager.jsm", tmp);
  return tmp.ScratchpadManager;
});

var ResponsiveUI = {
  toggle: function RUI_toggle() {
    this.ResponsiveUIManager.toggle(window, gBrowser.selectedTab);
  }
};

XPCOMUtils.defineLazyGetter(ResponsiveUI, "ResponsiveUIManager", function() {
  let tmp = {};
  Cu.import("resource:///modules/devtools/responsivedesign.jsm", tmp);
  return tmp.ResponsiveUIManager;
});

XPCOMUtils.defineLazyGetter(window, "gShowPageResizers", function () {
#ifdef XP_WIN
  // Only show resizers on Windows 2000 and XP
  let sysInfo = Components.classes["@mozilla.org/system-info;1"]
                          .getService(Components.interfaces.nsIPropertyBag2);
  return parseFloat(sysInfo.getProperty("version")) < 6;
#else
  return false;
#endif
});

var MousePosTracker = {
  _listeners: [],
  _x: 0,
  _y: 0,
  get _windowUtils() {
    delete this._windowUtils;
    return this._windowUtils = window.getInterface(Ci.nsIDOMWindowUtils);
  },

  addListener: function (listener) {
    if (this._listeners.indexOf(listener) >= 0)
      return;

    listener._hover = false;
    this._listeners.push(listener);

    this._callListener(listener);
  },

  removeListener: function (listener) {
    var index = this._listeners.indexOf(listener);
    if (index < 0)
      return;

    this._listeners.splice(index, 1);
  },

  handleEvent: function (event) {
    var fullZoom = this._windowUtils.fullZoom;
    this._x = event.screenX / fullZoom - window.mozInnerScreenX;
    this._y = event.screenY / fullZoom - window.mozInnerScreenY;

    this._listeners.forEach(function (listener) {
      try {
        this._callListener(listener);
      } catch (e) {
        Cu.reportError(e);
      }
    }, this);
  },

  _callListener: function (listener) {
    let rect = listener.getMouseTargetRect();
    let hover = this._x >= rect.left &&
                this._x <= rect.right &&
                this._y >= rect.top &&
                this._y <= rect.bottom;

    if (hover == listener._hover)
      return;

    listener._hover = hover;

    if (hover) {
      if (listener.onMouseEnter)
        listener.onMouseEnter();
    } else {
      if (listener.onMouseLeave)
        listener.onMouseLeave();
    }
  }
};

function focusNextFrame(event) {
  let fm = Cc["@mozilla.org/focus-manager;1"].getService(Ci.nsIFocusManager);
  let dir = event.shiftKey ? fm.MOVEFOCUS_BACKWARDDOC : fm.MOVEFOCUS_FORWARDDOC;
  let element = fm.moveFocus(window, null, dir, fm.FLAG_BYKEY);
  if (element.ownerDocument == document)
    focusAndSelectUrlBar();
}
