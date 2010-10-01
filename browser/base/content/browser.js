# -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Blake Ross <blake@cs.stanford.edu>
#   David Hyatt <hyatt@mozilla.org>
#   Peter Annema <disttsc@bart.nl>
#   Dean Tessman <dean_tessman@hotmail.com>
#   Kevin Puetz <puetzk@iastate.edu>
#   Ben Goodger <ben@netscape.com>
#   Pierre Chanial <chanial@noos.fr>
#   Jason Eager <jce2@po.cwru.edu>
#   Joe Hewitt <hewitt@netscape.com>
#   Alec Flett <alecf@netscape.com>
#   Asaf Romano <mozilla.mano@sent.com>
#   Jason Barnabe <jason_barnabe@fastmail.fm>
#   Peter Parente <parente@cs.unc.edu>
#   Giorgio Maone <g.maone@informaction.com>
#   Tom Germeau <tom.germeau@epigoon.com>
#   Jesse Ruderman <jruderman@gmail.com>
#   Joe Hughes <joe@retrovirus.com>
#   Pamela Greene <pamg.bugs@gmail.com>
#   Michael Ventnor <m.ventnor@gmail.com>
#   Simon Bünzli <zeniko@gmail.com>
#   Johnathan Nightingale <johnath@mozilla.com>
#   Ehsan Akhgari <ehsan.akhgari@gmail.com>
#   Dão Gottwald <dao@mozilla.com>
#   Thomas K. Dyas <tdyas@zecador.org>
#   Edward Lee <edward.lee@engineering.uiuc.edu>
#   Paul O’Shannessy <paul@oshannessy.com>
#   Nils Maier <maierman@web.de>
#   Rob Arnold <robarnold@cmu.edu>
#   Dietrich Ayala <dietrich@mozilla.com>
#   Gavin Sharp <gavin@gavinsharp.com>
#   Justin Dolske <dolske@mozilla.com>
#   Rob Campbell <rcampbell@mozilla.com>
#   David Dahl <ddahl@mozilla.com>
#   Patrick Walton <pcwalton@mozilla.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

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
  findbar.setAttribute("browserid", "content");
  findbar.id = "FindToolbar";

  let browserBottomBox = document.getElementById("browser-bottombox");
  browserBottomBox.insertBefore(findbar, browserBottomBox.firstChild);

  // Force a style flush to ensure that our binding is attached.
  findbar.clientTop;
  window.gFindBarInitialized = true;
  return findbar;
});

__defineGetter__("gPrefService", function() {
  delete this.gPrefService;
  return this.gPrefService = Services.prefs;
});

__defineGetter__("AddonManager", function() {
  Cu.import("resource://gre/modules/AddonManager.jsm");
  return this.AddonManager;
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

#ifdef MOZ_SERVICES_SYNC
XPCOMUtils.defineLazyGetter(this, "Weave", function() {
  let tmp = {};
  Cu.import("resource://services-sync/main.js", tmp);
  return tmp.Weave;
});
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
  }
});

let gInitialPages = [
  "about:blank",
  "about:privatebrowsing",
  "about:sessionrestore"
];

#include browser-fullZoom.js
#include inspector.js
#include browser-places.js
#include browser-tabPreviews.js
#include browser-tabview.js

#ifdef MOZ_SERVICES_SYNC
#include browser-syncui.js
#endif

XPCOMUtils.defineLazyGetter(this, "Win7Features", function () {
#ifdef XP_WIN
#ifndef WINCE
  const WINTASKBAR_CONTRACTID = "@mozilla.org/windows-taskbar;1";
  if (WINTASKBAR_CONTRACTID in Cc &&
      Cc[WINTASKBAR_CONTRACTID].getService(Ci.nsIWinTaskbar).available) {
    let temp = {};
    Cu.import("resource://gre/modules/WindowsPreviewPerTab.jsm", temp);
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
#endif
  return null;
});

#ifdef MOZ_CRASHREPORTER
XPCOMUtils.defineLazyServiceGetter(this, "gCrashReporter",
                                   "@mozilla.org/xre/app-info;1",
                                   "nsICrashReporter");
#endif

/**
* We can avoid adding multiple load event listeners and save some time by adding
* one listener that calls all real handlers.
*/
function pageShowEventHandlers(event) {
  // Filter out events that are not about the document load we are interested in
  if (event.originalTarget == content.document) {
    charsetLoadListener(event);
    XULBrowserWindow.asyncUpdateUI();
  }
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

#ifdef XP_MACOSX
/**
 * Click-and-Hold implementation for the Back and Forward buttons
 * XXXmano: should this live in toolbarbutton.xml?
 */
function SetClickAndHoldHandlers() {
  var timer;

  function timerCallback(aButton) {
    aButton.firstChild.hidden = false;
    aButton.open = true;
    timer = null;
  }

  function mousedownHandler(aEvent) {
    if (aEvent.button != 0 ||
        aEvent.currentTarget.open ||
        aEvent.currentTarget.disabled)
      return;

    // Prevent the menupopup from opening immediately
    aEvent.currentTarget.firstChild.hidden = true;

    timer = setTimeout(timerCallback, 500, aEvent.currentTarget);
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

  function stopTimer(aEvent) {
    if (timer) {
      clearTimeout(timer);
      timer = null;
    }
  }

  function _addClickAndHoldListenersOnElement(aElm) {
    aElm.addEventListener("mousedown", mousedownHandler, true);
    aElm.addEventListener("mouseup", stopTimer, false);
    aElm.addEventListener("mouseout", stopTimer, false);
    aElm.addEventListener("click", clickHandler, true);
  }

  // Bug 414797: Clone the dropmarker's menu into both the back and
  // the forward buttons.
  var unifiedButton = document.getElementById("unified-back-forward-button");
  if (unifiedButton && !unifiedButton._clickHandlersAttached) {
    var popup = document.getElementById("back-forward-dropmarker")
                        .firstChild.cloneNode(true);
    var backButton = document.getElementById("back-button");
    backButton.setAttribute("type", "menu");
    backButton.appendChild(popup);
    _addClickAndHoldListenersOnElement(backButton);
    var forwardButton = document.getElementById("forward-button");
    popup = popup.cloneNode(true);
    forwardButton.setAttribute("type", "menu");
    forwardButton.appendChild(popup);
    _addClickAndHoldListenersOnElement(forwardButton);
    unifiedButton._clickHandlersAttached = true;
  }
}
#endif

const gSessionHistoryObserver = {
  observe: function(subject, topic, data)
  {
    if (topic != "browser:purge-session-history")
      return;

    var backCommand = document.getElementById("Browser:Back");
    backCommand.setAttribute("disabled", "true");
    var fwdCommand = document.getElementById("Browser:Forward");
    fwdCommand.setAttribute("disabled", "true");

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

const gPopupBlockerObserver = {

  onUpdatePageReport: function (aEvent)
  {
    if (aEvent.originalTarget != gBrowser.selectedBrowser)
      return;

    if (!gBrowser.pageReport)
      return;

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
        let blockString = gNavigatorBundle.getFormattedString("popupBlock", [uri.host]);
        blockedPopupAllowSite.setAttribute("label", blockString);
        blockedPopupAllowSite.setAttribute("block", "true");
      }
      else {
        // Offer an item to allow popups for this site
        let allowString = gNavigatorBundle.getFormattedString("popupAllow", [uri.host]);
        blockedPopupAllowSite.setAttribute("label", allowString);
        blockedPopupAllowSite.removeAttribute("block");
      }
    }
    catch (e) {
      blockedPopupAllowSite.setAttribute("hidden", "true");
    }

    if (gPrivateBrowsingUI.privateBrowsingEnabled)
      blockedPopupAllowSite.setAttribute("disabled", "true");
    else
      blockedPopupAllowSite.removeAttribute("disabled");

    var item = aEvent.target.lastChild;
    while (item && item.getAttribute("observes") != "blockedPopupsSeparator") {
      var next = item.previousSibling;
      item.parentNode.removeChild(item);
      item = next;
    }

    var foundUsablePopupURI = false;
    var pageReport = gBrowser.pageReport;
    if (pageReport) {
      for (var i = 0; i < pageReport.length; ++i) {
        // popupWindowURI will be null if the file picker popup is blocked.
        // xxxdz this should make the option say "Show file picker" and do it (Bug 590306) 
        if (!pageReport[i].popupWindowURI)
          continue;
        var popupURIspec = pageReport[i].popupWindowURI.spec;

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
        menuitem.setAttribute("popupWindowFeatures", pageReport[i].popupWindowFeatures);
        menuitem.setAttribute("popupWindowName", pageReport[i].popupWindowName);
        menuitem.setAttribute("oncommand", "gPopupBlockerObserver.showBlockedPopup(event);");
        menuitem.requestingWindow = pageReport[i].requestingWindow;
        menuitem.requestingDocument = pageReport[i].requestingDocument;
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
    blockedPopupDontShowMessage.setAttribute("label", gNavigatorBundle.getString("popupWarningDontShowFromMessage"));
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
#if 0 
    // Disabled until bug 594294 is fixed.
    var showMessage = gPrefService.getBoolPref("privacy.popups.showBrowserMessage");
    var firstTime = gPrefService.getBoolPref("privacy.popups.firstTime");

    // If the info message is showing at the top of the window, and the user has never
    // hidden the message before, show an info box telling the user where the info
    // will be displayed.
    if (showMessage && firstTime)
      this._displayPageReportFirstTime();

    gPrefService.setBoolPref("privacy.popups.showBrowserMessage", !showMessage);
#endif

    gBrowser.getNotificationBox().removeCurrentNotification();
  },

  _displayPageReportFirstTime: function ()
  {
    window.openDialog("chrome://browser/content/pageReportFirstTime.xul", "_blank",
                      "dependent");
  }
};

const gXPInstallObserver = {
  _findChildShell: function (aDocShell, aSoughtShell)
  {
    if (aDocShell == aSoughtShell)
      return aDocShell;

    var node = aDocShell.QueryInterface(Components.interfaces.nsIDocShellTreeNode);
    for (var i = 0; i < node.childCount; ++i) {
      var docShell = node.getChildAt(i);
      docShell = this._findChildShell(docShell, aSoughtShell);
      if (docShell == aSoughtShell)
        return docShell;
    }
    return null;
  },

  _getBrowser: function (aDocShell)
  {
    for (var i = 0; i < gBrowser.browsers.length; ++i) {
      var browser = gBrowser.getBrowserAtIndex(i);
      if (this._findChildShell(browser.docShell, aDocShell))
        return browser;
    }
    return null;
  },

  observe: function (aSubject, aTopic, aData)
  {
    var brandBundle = document.getElementById("bundle_brand");
    var installInfo = aSubject.QueryInterface(Components.interfaces.amIWebInstallInfo);
    var win = installInfo.originatingWindow;
    var shell = win.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                   .getInterface(Components.interfaces.nsIWebNavigation)
                   .QueryInterface(Components.interfaces.nsIDocShell);
    var browser = this._getBrowser(shell);
    if (!browser)
      return;
    const anchorID = "addons-notification-icon";
    var messageString, action;
    var brandShortName = brandBundle.getString("brandShortName");

    var notificationID = aTopic;
    // Make notifications persist a minimum of 30 seconds
    var options = {
      timeout: Date.now() + 30000
    };

    switch (aTopic) {
    case "addon-install-blocked":
      var enabled = true;
      try {
        enabled = gPrefService.getBoolPref("xpinstall.enabled");
      }
      catch (e) {
      }

      if (!enabled) {
        notificationID = "xpinstall-disabled"

        if (gPrefService.prefIsLocked("xpinstall.enabled")) {
          messageString = gNavigatorBundle.getString("xpinstallDisabledMessageLocked");
          buttons = [];
        }
        else {
          messageString = gNavigatorBundle.getString("xpinstallDisabledMessage");

          action = {
            label: gNavigatorBundle.getString("xpinstallDisabledButton"),
            accessKey: gNavigatorBundle.getString("xpinstallDisabledButton.accesskey"),
            callback: function editPrefs() {
              gPrefService.setBoolPref("xpinstall.enabled", true);
            }
          };
        }
      }
      else {
        messageString = gNavigatorBundle.getFormattedString("xpinstallPromptWarning",
                          [brandShortName, installInfo.originatingURI.host]);

        action = {
          label: gNavigatorBundle.getString("xpinstallPromptAllowButton"),
          accessKey: gNavigatorBundle.getString("xpinstallPromptAllowButton.accesskey"),
          callback: function() {
            installInfo.install();
          }
        };
      }

      PopupNotifications.show(browser, notificationID, messageString, anchorID,
                              action, null, options);
      break;
    case "addon-install-failed":
      // TODO This isn't terribly ideal for the multiple failure case
      installInfo.installs.forEach(function(aInstall) {
        var host = (installInfo.originatingURI instanceof Ci.nsIStandardURL) &&
                   installInfo.originatingURI.host;
        if (!host)
          host = (aInstall.sourceURI instanceof Ci.nsIStandardURL) &&
                 aInstall.sourceURI.host;

        var error = (host || aInstall.error == 0) ? "addonError" : "addonLocalError";
        if (aInstall.error != 0)
          error += aInstall.error;
        else if (aInstall.addon.blocklistState == Ci.nsIBlocklistService.STATE_BLOCKED)
          error += "Blocklisted";
        else
          error += "Incompatible";

        messageString = gNavigatorBundle.getString(error);
        messageString = messageString.replace("#1", aInstall.name);
        if (host)
          messageString = messageString.replace("#2", host);
        messageString = messageString.replace("#3", brandShortName);
        messageString = messageString.replace("#4", Services.appinfo.version);

        PopupNotifications.show(browser, notificationID, messageString, anchorID,
                                action, null, options);
      });
      break;
    case "addon-install-complete":
      var needsRestart = installInfo.installs.some(function(i) {
        return i.addon.pendingOperations != AddonManager.PENDING_NONE;
      });

      if (needsRestart) {
        messageString = gNavigatorBundle.getString("addonsInstalledNeedsRestart");
        action = {
          label: gNavigatorBundle.getString("addonInstallRestartButton"),
          accessKey: gNavigatorBundle.getString("addonInstallRestartButton.accesskey"),
          callback: function() {
            Application.restart();
          }
        };
      }
      else {
        messageString = gNavigatorBundle.getString("addonsInstalled");
        action = {
          label: gNavigatorBundle.getString("addonInstallManage"),
          accessKey: gNavigatorBundle.getString("addonInstallManage.accesskey"),
          callback: function() {
            // Calculate the add-on type that is most popular in the list of
            // installs
            var types = {};
            var bestType = null;
            installInfo.installs.forEach(function(aInstall) {
              if (aInstall.type in types)
                types[aInstall.type]++;
              else
                types[aInstall.type] = 1;
              if (!bestType || types[aInstall.type] > types[bestType])
                bestType = aInstall.type;
            });

            BrowserOpenAddonsMgr("addons://list/" + bestType);
          }
        };
      }

      messageString = PluralForm.get(installInfo.installs.length, messageString);
      messageString = messageString.replace("#1", installInfo.installs[0].name);
      messageString = messageString.replace("#2", installInfo.installs.length);
      messageString = messageString.replace("#3", brandShortName);

      PopupNotifications.show(browser, notificationID, messageString, anchorID,
                              action, null, options);
      break;
    }
  }
};

const gFormSubmitObserver = {
  QueryInterface : XPCOMUtils.generateQI([Ci.nsIFormSubmitObserver]),

  panel: null,

  init: function()
  {
    this.panel = document.getElementById('invalid-form-popup');
    this.panel.appendChild(document.createTextNode(""));
  },

  panelIsOpen: function()
  {
    return this.panel && this.panel.state != "hiding" &&
           this.panel.state != "closed";
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

    // Limit the message to 256 characters.
    this.panel.firstChild.nodeValue = element.validationMessage.substring(0, 256);

    element.focus();

    // If the user type something or blur the element, we want to remove the popup.
    // We could check for clicks but a click is already removing the popup.
    let eventHandler = function(e) {
      gFormSubmitObserver.panel.hidePopup();
    };
    element.addEventListener("input", eventHandler, false);
    element.addEventListener("blur", eventHandler, false);

    // One event to bring them all and in the darkness bind them all.
    this.panel.addEventListener("popuphiding", function(aEvent) {
      aEvent.target.removeEventListener("popuphiding", arguments.callee, false);
      element.removeEventListener("input", eventHandler, false);
      element.removeEventListener("blur", eventHandler, false);
    }, false);

    this.panel.hidden = false;
    this.panel.openPopup(element, "after_start", 0, 0);
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
        return this.onSwipe(aEvent);
      case "MozMagnifyGestureStart":
        aEvent.preventDefault();
#ifdef XP_WIN
        return this._setupGesture(aEvent, "pinch", def(25, 0), "out", "in");
#else
        return this._setupGesture(aEvent, "pinch", def(150, 1), "out", "in");
#endif
      case "MozRotateGestureStart":
        aEvent.preventDefault();
        return this._setupGesture(aEvent, "twist", def(25, 0), "right", "left");
      case "MozMagnifyGestureUpdate":
      case "MozRotateGestureUpdate":
        aEvent.preventDefault();
        return this._doUpdate(aEvent);
      case "MozTapGesture":
        aEvent.preventDefault();
        return this._doAction(aEvent, ["tap"]);
      case "MozPressTapGesture":
      // Fall through to default behavior
      return;
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
   * @return Name of the command found for the event's keys and gesture. If no
   *         command is found, no value is returned (undefined).
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
    for each (let subCombo in this._power(keyCombos)) {
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

      return command;
    }
    return null;
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
    ["UP", "RIGHT", "DOWN", "LEFT"].forEach(function (dir) {
      if (aEvent.direction == aEvent["DIRECTION_" + dir])
        return this._doAction(aEvent, ["swipe", dir.toLowerCase()]);
    }, this);
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

function BrowserStartup() {
  var uriToLoad = null;

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
    uriToLoad = window.arguments[0];

  var isLoadingBlank = uriToLoad == "about:blank";
  var mustLoadSidebar = false;

  prepareForStartup();

  if (uriToLoad && !isLoadingBlank) {
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
      content.focus();
    }
    // Note: loadOneOrMoreURIs *must not* be called if window.arguments.length >= 3.
    // Such callers expect that window.arguments[0] is handled as a single URI.
    else
      loadOneOrMoreURIs(uriToLoad);
  }

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

  if (!window.toolbar.visible) {
    // adjust browser UI for popups
    if (gURLBar) {
      gURLBar.setAttribute("readonly", "true");
      gURLBar.setAttribute("enablehistory", "false");
    }
    goSetCommandEnabled("Browser:OpenLocation", false);
    goSetCommandEnabled("cmd_newNavigatorTab", false);
  }

#ifdef MENUBAR_CAN_AUTOHIDE
  updateAppButtonDisplay();
#endif

  CombinedStopReload.init();

  allTabs.readPref();

  TabsOnTop.syncCommand();

  BookmarksMenuButton.init();

  setTimeout(delayedStartup, 0, isLoadingBlank, mustLoadSidebar);
}

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

function prepareForStartup() {
  gBrowser.addEventListener("DOMUpdatePageReport", gPopupBlockerObserver.onUpdatePageReport, false);

  gBrowser.addEventListener("PluginNotFound",     gPluginHandler, true);
  gBrowser.addEventListener("PluginCrashed",      gPluginHandler, true);
  gBrowser.addEventListener("PluginBlocklisted",  gPluginHandler, true);
  gBrowser.addEventListener("PluginOutdated",     gPluginHandler, true);
  gBrowser.addEventListener("PluginDisabled",     gPluginHandler, true);
  gBrowser.addEventListener("NewPluginInstalled", gPluginHandler.newPluginInstalled, true);

  Services.obs.addObserver(gPluginHandler.pluginCrashed, "plugin-crashed", false);

  window.addEventListener("AppCommand", HandleAppCommandEvent, true);

  var webNavigation;
  try {
    webNavigation = getWebNavigation();
    if (!webNavigation)
      throw "no XBL binding for browser";
  } catch (e) {
    alert("Error launching browser window:" + e);
    window.close(); // Give up.
    return;
  }

  // initialize observers and listeners
  // and give C++ access to gBrowser
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
    if (window.arguments[1].indexOf("charset=") != -1) {
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
  webNavigation.sessionHistory = Components.classes["@mozilla.org/browser/shistory;1"]
                                           .createInstance(Components.interfaces.nsISHistory);
  Services.obs.addObserver(gBrowser.browsers[0], "browser:purge-session-history", false);

  // remove the disablehistory attribute so the browser cleans up, as
  // though it had done this work itself
  gBrowser.browsers[0].removeAttribute("disablehistory");

  // enable global history
  try {
    gBrowser.docShell.QueryInterface(Components.interfaces.nsIDocShellHistory).useGlobalHistory = true;
  } catch(ex) {
    Components.utils.reportError("Places database may be locked: " + ex);
  }

  // hook up UI through progress listener
  gBrowser.addProgressListener(window.XULBrowserWindow, Components.interfaces.nsIWebProgress.NOTIFY_ALL);
  gBrowser.addTabsProgressListener(window.TabsProgressListener);

  // setup our common DOMLinkAdded listener
  gBrowser.addEventListener("DOMLinkAdded", DOMLinkHandler, false);

  // setup our MozApplicationManifest listener
  gBrowser.addEventListener("MozApplicationManifest",
                            OfflineApps, false);

  // setup simple gestures support
  gGestureSupport.init(true);
}

function delayedStartup(isLoadingBlank, mustLoadSidebar) {
  Services.obs.addObserver(gSessionHistoryObserver, "browser:purge-session-history", false);
  Services.obs.addObserver(gXPInstallObserver, "addon-install-blocked", false);
  Services.obs.addObserver(gXPInstallObserver, "addon-install-failed", false);
  Services.obs.addObserver(gXPInstallObserver, "addon-install-complete", false);
  Services.obs.addObserver(gFormSubmitObserver, "invalidformsubmit", false);

  BrowserOffline.init();
  OfflineApps.init();
  IndexedDBPromptHelper.init();
  gFormSubmitObserver.init();

  gBrowser.addEventListener("pageshow", function(evt) { setTimeout(pageShowEventHandlers, 0, evt); }, true);

  // Ensure login manager is up and running.
  Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager);

  if (mustLoadSidebar) {
    let sidebar = document.getElementById("sidebar");
    let sidebarBox = document.getElementById("sidebar-box");
    sidebar.setAttribute("src", sidebarBox.getAttribute("src"));
  }

  UpdateUrlbarSearchSplitterState();

  PlacesStarButton.init();

  if (isLoadingBlank && gURLBar && isElementVisible(gURLBar))
    gURLBar.focus();
  else
    gBrowser.selectedBrowser.focus();

  gNavToolbox.customizeDone = BrowserToolboxCustomizeDone;
  gNavToolbox.customizeChange = BrowserToolboxCustomizeChange;

  // Set up Sanitize Item
  initializeSanitizer();

  // Enable/Disable auto-hide tabbar
  gBrowser.tabContainer.updateVisibility();

  gPrefService.addObserver(gHomeButton.prefDomain, gHomeButton, false);

  var homeButton = document.getElementById("home-button");
  gHomeButton.updateTooltip(homeButton);
  gHomeButton.updatePersonalToolbarStyle(homeButton);

#ifdef HAVE_SHELL_SERVICE
  // Perform default browser checking (after window opens).
  var shell = getShellService();
  if (shell) {
    var shouldCheck = shell.shouldCheckDefaultBrowser;
    var willRecoverSession = false;
    try {
      var ss = Cc["@mozilla.org/browser/sessionstartup;1"].
               getService(Ci.nsISessionStartup);
      willRecoverSession =
        (ss.sessionType == Ci.nsISessionStartup.RECOVER_SESSION);
    }
    catch (ex) { /* never mind; suppose SessionStore is broken */ }
    if (shouldCheck && !shell.isDefaultBrowser(true) && !willRecoverSession) {
      var brandBundle = document.getElementById("bundle_brand");
      var shellBundle = document.getElementById("bundle_shell");

      var brandShortName = brandBundle.getString("brandShortName");
      var promptTitle = shellBundle.getString("setDefaultBrowserTitle");
      var promptMessage = shellBundle.getFormattedString("setDefaultBrowserMessage",
                                                         [brandShortName]);
      var checkboxLabel = shellBundle.getFormattedString("setDefaultBrowserDontAsk",
                                                         [brandShortName]);
      var checkEveryTime = { value: shouldCheck };
      var ps = Services.prompt;
      var rv = ps.confirmEx(window, promptTitle, promptMessage,
                            ps.STD_YES_NO_BUTTONS,
                            null, null, null, checkboxLabel, checkEveryTime);
      if (rv == 0)
        shell.setDefaultBrowser(true, false);
      shell.shouldCheckDefaultBrowser = checkEveryTime.value;
    }
  }
#endif

  // BiDi UI
  gBidiUI = isBidiEnabled();
  if (gBidiUI) {
    document.getElementById("documentDirection-separator").hidden = false;
    document.getElementById("documentDirection-swap").hidden = false;
    document.getElementById("textfieldDirection-separator").hidden = false;
    document.getElementById("textfieldDirection-swap").hidden = false;
  }

#ifdef XP_MACOSX
  // Setup click-and-hold gestures access to the session history
  // menus if global click-and-hold isn't turned on
  if (!getBoolPref("ui.click_hold_context_menus", false))
    SetClickAndHoldHandlers();
#endif

  // Initialize the full zoom setting.
  // We do this before the session restore service gets initialized so we can
  // apply full zoom settings to tabs restored by the session restore service.
  try {
    FullZoom.init();
  }
  catch(ex) {
    Components.utils.reportError("Failed to init content pref service:\n" + ex);
  }

  let NP = {};
  Cu.import("resource:///modules/NetworkPrioritizer.jsm", NP);
  NP.trackBrowserWindow(window);

  // initialize the session-restore service (in case it's not already running)
  try {
    Cc["@mozilla.org/browser/sessionstore;1"]
      .getService(Ci.nsISessionStore)
      .init(window);
  } catch (ex) {
    dump("nsSessionStore could not be initialized: " + ex + "\n");
  }

  PlacesToolbarHelper.init();

  // bookmark-all-tabs command
  gBookmarkAllTabsHandler.init();

  ctrlTab.readPref();
  gPrefService.addObserver(ctrlTab.prefName, ctrlTab, false);
  gPrefService.addObserver(allTabs.prefName, allTabs, false);

  // Initialize the microsummary service by retrieving it, prompting its factory
  // to create its singleton, whose constructor initializes the service.
  // Started 4 seconds after delayedStartup (before the livemarks service below).
  setTimeout(function() {
    try {
      Cc["@mozilla.org/microsummary/service;1"].getService(Ci.nsIMicrosummaryService);
    } catch (ex) {
      Components.utils.reportError("Failed to init microsummary service:\n" + ex);
    }
  }, 4000);

  // Delayed initialization of the livemarks update timer.
  // Livemark updates don't need to start until after bookmark UI
  // such as the toolbar has initialized. Starting 5 seconds after
  // delayedStartup in order to stagger this after the microsummary
  // service (see above) and before the download manager starts (see below).
  setTimeout(function() PlacesUtils.livemarks.start(), 5000);

  // Initialize the download manager some time after the app starts so that
  // auto-resume downloads begin (such as after crashing or quitting with
  // active downloads) and speeds up the first-load of the download manager UI.
  // If the user manually opens the download manager before the timeout, the
  // downloads will start right away, and getting the service again won't hurt.
  setTimeout(function() {
    gDownloadMgr = Cc["@mozilla.org/download-manager;1"].
                   getService(Ci.nsIDownloadManager);

    if (Win7Features) {
      let tempScope = {};
      Cu.import("resource://gre/modules/DownloadTaskbarProgress.jsm",
                tempScope);
      tempScope.DownloadTaskbarProgress.onBrowserWindowLoad(window);
    }
  }, 10000);

  // Delayed initialization of PlacesDBUtils.
  // This component checks for database coherence once per day, on
  // an idle timer, taking corrective actions where needed.
  setTimeout(function() PlacesUtils.startPlacesDBUtils(), 15000);

#ifndef XP_MACOSX
  updateEditUIVisibility();
  let placesContext = document.getElementById("placesContext");
  placesContext.addEventListener("popupshowing", updateEditUIVisibility, false);
  placesContext.addEventListener("popuphiding", updateEditUIVisibility, false);
#endif

  // initialize the private browsing UI
  gPrivateBrowsingUI.init();

  gBrowser.mPanelContainer.addEventListener("InstallBrowserTheme", LightWeightThemeWebInstaller, false, true);
  gBrowser.mPanelContainer.addEventListener("PreviewBrowserTheme", LightWeightThemeWebInstaller, false, true);
  gBrowser.mPanelContainer.addEventListener("ResetBrowserThemePreview", LightWeightThemeWebInstaller, false, true);

  if (Win7Features)
    Win7Features.onOpenWindow();

  // called when we go into full screen, even if it is
  // initiated by a web page script
  window.addEventListener("fullscreen", onFullScreen, true);
  if (window.fullScreen)
    onFullScreen();

#ifdef MOZ_SERVICES_SYNC
  // initialize the sync UI
  gSyncUI.init();
#endif

  TabView.init();

  // Enable Inspector?
  let enabled = gPrefService.getBoolPref(InspectorUI.prefEnabledName);
  if (enabled) {
    document.getElementById("menu_pageinspect").setAttribute("hidden", false);
    document.getElementById("Tools:Inspect").removeAttribute("disabled");
    let appMenuInspect = document.getElementById("appmenu_pageInspect");
    if (appMenuInspect)
      appMenuInspect.setAttribute("hidden", false);
  }

  // Enable Error Console?
  let consoleEnabled = gPrefService.getBoolPref("devtools.errorconsole.enabled");
  if (consoleEnabled) {
    document.getElementById("javascriptConsole").hidden = false;
    document.getElementById("key_errorConsole").removeAttribute("disabled");
  }

  // If the user (or the locale) hasn't enabled the top-level "Character
  // Encoding" menu via the "browser.menu.showCharacterEncoding" preference,
  // hide it.
  const showCharacterEncodingPref = "browser.menu.showCharacterEncoding";
  let extraCharacterEncodingMenuEnabled = gPrefService.
    getComplexValue(showCharacterEncodingPref, Ci.nsIPrefLocalizedString).data;
  if (extraCharacterEncodingMenuEnabled !== "true") {
    let charsetMenu = document.getElementById("appmenu_charsetMenu");
    if (charsetMenu)
      charsetMenu.setAttribute("hidden", "true");
  }

  Services.obs.notifyObservers(window, "browser-delayed-startup-finished", "");
}

function BrowserShutdown()
{
  if (Win7Features)
    Win7Features.onCloseWindow();

  gPrefService.removeObserver(ctrlTab.prefName, ctrlTab);
  gPrefService.removeObserver(allTabs.prefName, allTabs);
  ctrlTab.uninit();
  allTabs.uninit();

  CombinedStopReload.uninit();

  gGestureSupport.init(false);

  FullScreen.cleanup();

  try {
    FullZoom.destroy();
  }
  catch(ex) {
    Components.utils.reportError(ex);
  }

  Services.obs.removeObserver(gSessionHistoryObserver, "browser:purge-session-history");
  Services.obs.removeObserver(gXPInstallObserver, "addon-install-blocked");
  Services.obs.removeObserver(gXPInstallObserver, "addon-install-failed");
  Services.obs.removeObserver(gXPInstallObserver, "addon-install-complete");
  Services.obs.removeObserver(gPluginHandler.pluginCrashed, "plugin-crashed");
  Services.obs.removeObserver(gFormSubmitObserver, "invalidformsubmit");

  try {
    gBrowser.removeProgressListener(window.XULBrowserWindow);
    gBrowser.removeTabsProgressListener(window.TabsProgressListener);
  } catch (ex) {
  }

  PlacesStarButton.uninit();

  try {
    gPrefService.removeObserver(gHomeButton.prefDomain, gHomeButton);
  } catch (ex) {
    Components.utils.reportError(ex);
  }

  BrowserOffline.uninit();
  OfflineApps.uninit();
  gPrivateBrowsingUI.uninit();
  IndexedDBPromptHelper.uninit();

  var enumerator = Services.wm.getEnumerator(null);
  enumerator.getNext();
  if (!enumerator.hasMoreElements()) {
    document.persist("sidebar-box", "sidebarcommand");
    document.persist("sidebar-box", "width");
    document.persist("sidebar-box", "src");
    document.persist("sidebar-title", "value");
  }

  window.XULBrowserWindow.destroy();
  window.XULBrowserWindow = null;
  window.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
        .getInterface(Components.interfaces.nsIWebNavigation)
        .QueryInterface(Components.interfaces.nsIDocShellTreeItem).treeOwner
        .QueryInterface(Components.interfaces.nsIInterfaceRequestor)
        .getInterface(Components.interfaces.nsIXULWindow)
        .XULBrowserWindow = null;
  window.QueryInterface(Ci.nsIDOMChromeWindow).browserDOMWindow = null;
}

#ifdef XP_MACOSX
// nonBrowserWindowStartup(), nonBrowserWindowDelayedStartup(), and
// nonBrowserWindowShutdown() are used for non-browser windows in
// macBrowserOverlay
function nonBrowserWindowStartup()
{
  // Disable inappropriate commands / submenus
  var disabledItems = ['Browser:SavePage',
                       'Browser:SendLink', 'cmd_pageSetup', 'cmd_print', 'cmd_find', 'cmd_findAgain',
                       'viewToolbarsMenu', 'viewSidebarMenuMenu', 'Browser:Reload',
                       'viewFullZoomMenu', 'pageStyleMenu', 'charsetMenu', 'View:PageSource', 'View:FullScreen',
                       'viewHistorySidebar', 'Browser:AddBookmarkAs', 'View:PageInfo', 'Tasks:InspectPage'];
  var element;

  for (var id in disabledItems)
  {
    element = document.getElementById(disabledItems[id]);
    if (element)
      element.setAttribute("disabled", "true");
  }

  // If no windows are active (i.e. we're the hidden window), disable the close, minimize
  // and zoom menu commands as well
  if (window.location.href == "chrome://browser/content/hiddenWindow.xul")
  {
    var hiddenWindowDisabledItems = ['cmd_close', 'minimizeWindow', 'zoomWindow'];
    for (var id in hiddenWindowDisabledItems)
    {
      element = document.getElementById(hiddenWindowDisabledItems[id]);
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


  setTimeout(nonBrowserWindowDelayedStartup, 0);
}

function nonBrowserWindowDelayedStartup()
{
  // initialise the offline listener
  BrowserOffline.init();

  // Set up Sanitize Item
  initializeSanitizer();

  // initialize the private browsing UI
  gPrivateBrowsingUI.init();

#ifdef MOZ_SERVICES_SYNC
  // initialize the sync UI
  gSyncUI.init();
#endif
}

function nonBrowserWindowShutdown()
{
  BrowserOffline.uninit();

  gPrivateBrowsingUI.uninit();
}
#endif

function initializeSanitizer()
{
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
      itemArray.forEach(function (name) {
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
      });
    }

    gPrefService.setBoolPref("privacy.sanitize.migrateFx3Prefs", true);
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

  duplicateTabIn(gBrowser.selectedTab, where, index);
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
    let currentIndex = getWebNavigation().sessionHistory.index;
    duplicateTabIn(gBrowser.selectedTab, where, currentIndex + 1);
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
    let currentIndex = getWebNavigation().sessionHistory.index;
    duplicateTabIn(gBrowser.selectedTab, where, currentIndex - 1);
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

function BrowserStop()
{
  try {
    const stopFlags = nsIWebNavigation.STOP_ALL;
    getWebNavigation().stop(stopFlags);
  }
  catch(ex) {
  }
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

function BrowserHome()
{
  var homePage = gHomeButton.getHomePage();
  loadOneOrMoreURIs(homePage);
}

function BrowserGoHome(aEvent) {
  if (aEvent && "button" in aEvent &&
      aEvent.button == 2) // right-click: do nothing
    return;

  var homePage = gHomeButton.getHomePage();
  var where = whereToOpenLink(aEvent, false, true);
  var urls;

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
  if (gURLBar && !gURLBar.readOnly) {
    if (window.fullScreen)
      FullScreen.mouseoverToggle(true);
    if (isElementVisible(gURLBar)) {
      gURLBar.focus();
      gURLBar.select();
      return true;
    }
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
                              "chrome,all,dialog=no", "about:blank");
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
  if (!gBrowser) {
    // If there are no open browser windows, open a new one
    window.openDialog("chrome://browser/content/", "_blank",
                      "chrome,all,dialog=no", "about:blank");
    return;
  }
  gBrowser.loadOneTab("about:blank", {inBackground: false});
  focusAndSelectUrlBar();
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
  // the callback function would confuse prepareForStartup() by making
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
    if (!val || !val.exists() || !val.isDirectory())
      return;
    this._lastDir = val.clone();

    // Don't save the last open directory pref inside the Private Browsing mode
    if (!gPrivateBrowsingUI.privateBrowsingEnabled)
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
    const nsIFilePicker = Components.interfaces.nsIFilePicker;
    var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
    fp.init(window, gNavigatorBundle.getString("openFile"), nsIFilePicker.modeOpen);
    fp.appendFilters(nsIFilePicker.filterAll | nsIFilePicker.filterText | nsIFilePicker.filterImages |
                     nsIFilePicker.filterXML | nsIFilePicker.filterHTML);
    fp.displayDirectory = gLastOpenDirectory.path;

    if (fp.show() == nsIFilePicker.returnOK) {
      if (fp.file && fp.file.exists())
        gLastOpenDirectory.path = fp.file.parent.QueryInterface(Ci.nsILocalFile);
      openTopWin(fp.fileURL.spec);
    }
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

function loadURI(uri, referrer, postData, allowThirdPartyFixup)
{
  try {
    if (postData === undefined)
      postData = null;
    var flags = nsIWebNavigation.LOAD_FLAGS_NONE;
    if (allowThirdPartyFixup) {
      flags = nsIWebNavigation.LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;
    }
    gBrowser.loadURIWithFlags(uri, flags, referrer, null, postData);
  } catch (e) {
  }
}

function getShortcutOrURI(aURL, aPostDataRef) {
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

    var encodedParam = "";
    if (charset)
      encodedParam = escape(convertFromUnicode(charset, param));
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
    let uri = aURI || getWebNavigation().currentURI;

    // Replace initial page URIs with an empty string
    // only if there's no opener (bug 370555).
    if (gInitialPages.indexOf(uri.spec) != -1)
      value = content.opener ? uri.spec : "";
    else
      value = losslessDecodeURI(uri);

    valid = (uri.spec != "about:blank");
  }

  gURLBar.value = value;
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
      splitter.className = "chromeclass-toolbar-additional";
    }
    urlbar.parentNode.insertBefore(splitter, ibefore);
  } else if (splitter)
    splitter.parentNode.removeChild(splitter);
}

var LocationBarHelpers = {
  _timeoutID: null,

  _searchBegin: function LocBar_searchBegin() {
    function delayedBegin(self) {
      self._timeoutID = null;
      document.getElementById("urlbar-throbber").setAttribute("busy", "true");
    }

    this._timeoutID = setTimeout(delayedBegin, 500, this);
  },

  _searchComplete: function LocBar_searchComplete() {
    // Did we finish the search before delayedBegin was invoked?
    if (this._timeoutID) {
      clearTimeout(this._timeoutID);
      this._timeoutID = null;
    }
    document.getElementById("urlbar-throbber").removeAttribute("busy");
  }
};

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

    PageProxySetIcon(gBrowser.getIcon());
  } else if (aState == "invalid") {
    gURLBar.removeEventListener("input", UpdatePageProxyState, false);
    PageProxyClearIcon();
  }
}

function PageProxySetIcon (aURL)
{
  if (!gProxyFavIcon)
    return;

  if (!aURL)
    PageProxyClearIcon();
  else if (gProxyFavIcon.getAttribute("src") != aURL)
    gProxyFavIcon.setAttribute("src", aURL);
}

function PageProxyClearIcon ()
{
  gProxyFavIcon.removeAttribute("src");
}

function PageProxyClickHandler(aEvent)
{
  if (aEvent.button == 1 && gPrefService.getBoolPref("middlemouse.paste"))
    middleMousePaste(aEvent);
}

function BrowserImport()
{
#ifdef XP_MACOSX
  var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                     .getService(Components.interfaces.nsIWindowMediator);
  var win = wm.getMostRecentWindow("Browser:MigrationWizard");
  if (win)
    win.focus();
  else {
    window.openDialog("chrome://browser/content/migration/migration.xul",
                      "migration", "centerscreen,chrome,resizable=no");
  }
#else
  window.openDialog("chrome://browser/content/migration/migration.xul",
                    "migration", "modal,centerscreen,chrome,resizable=no");
#endif
}

/**
 * Handle command events bubbling up from error page content
 */
function BrowserOnClick(event) {
    // Don't trust synthetic events
    if (!event.isTrusted || event.target.localName != "button")
      return;

    var ot = event.originalTarget;
    var errorDoc = ot.ownerDocument;

    // If the event came from an ssl error page, it is probably either the "Add
    // Exception…" or "Get me out of here!" button
    if (/^about:certerror/.test(errorDoc.documentURI)) {
      if (ot == errorDoc.getElementById('exceptionDialogButton')) {
        var params = { exceptionAdded : false, handlePrivateBrowsing : true };

        try {
          switch (gPrefService.getIntPref("browser.ssl_override_behavior")) {
            case 2 : // Pre-fetch & pre-populate
              params.prefetchCert = true;
            case 1 : // Pre-populate
              params.location = errorDoc.location.href;
          }
        } catch (e) {
          Components.utils.reportError("Couldn't get ssl_override pref: " + e);
        }

        window.openDialog('chrome://pippki/content/exceptionDialog.xul',
                          '','chrome,centerscreen,modal', params);

        // If the user added the exception cert, attempt to reload the page
        if (params.exceptionAdded)
          errorDoc.location.reload();
      }
      else if (ot == errorDoc.getElementById('getMeOutOfHereButton')) {
        getMeOutOfHere();
      }
    }
    else if (/^about:blocked/.test(errorDoc.documentURI)) {
      // The event came from a button on a malware/phishing block page
      // First check whether it's malware or phishing, so that we can
      // use the right strings/links
      var isMalware = /e=malwareBlocked/.test(errorDoc.documentURI);

      if (ot == errorDoc.getElementById('getMeOutButton')) {
        getMeOutOfHere();
      }
      else if (ot == errorDoc.getElementById('reportButton')) {
        // This is the "Why is this site blocked" button.  For malware,
        // we can fetch a site-specific report, for phishing, we redirect
        // to the generic page describing phishing protection.

        if (isMalware) {
          // Get the stop badware "why is this blocked" report url,
          // append the current url, and go there.
          try {
            let reportURL = formatURL("browser.safebrowsing.malware.reportURL", true);
            reportURL += errorDoc.location.href;
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
      }
      else if (ot == errorDoc.getElementById('ignoreWarningButton')) {
        // Allow users to override and continue through to the site,
        // but add a notify bar as a reminder, so that they don't lose
        // track after, e.g., tab switching.
        gBrowser.loadURIWithFlags(content.location.href,
                                  nsIWebNavigation.LOAD_FLAGS_BYPASS_CLASSIFIER,
                                  null, null, null);
        let buttons = [{
          label: gNavigatorBundle.getString("safebrowsing.getMeOutOfHereButton.label"),
          accessKey: gNavigatorBundle.getString("safebrowsing.getMeOutOfHereButton.accessKey"),
          callback: function() { getMeOutOfHere(); }
        }];

        let title;
        if (isMalware) {
          title = gNavigatorBundle.getString("safebrowsing.reportedAttackSite");
          buttons[1] = {
            label: gNavigatorBundle.getString("safebrowsing.notAnAttackButton.label"),
            accessKey: gNavigatorBundle.getString("safebrowsing.notAnAttackButton.accessKey"),
            callback: function() {
              openUILinkIn(safebrowsing.getReportURL('MalwareError'), 'tab');
            }
          };
        } else {
          title = gNavigatorBundle.getString("safebrowsing.reportedWebForgery");
          buttons[1] = {
            label: gNavigatorBundle.getString("safebrowsing.notAForgeryButton.label"),
            accessKey: gNavigatorBundle.getString("safebrowsing.notAForgeryButton.accessKey"),
            callback: function() {
              openUILinkIn(safebrowsing.getReportURL('Error'), 'tab');
            }
          };
        }

        let notificationBox = gBrowser.getNotificationBox();
        let value = "blocked-badware-page";

        let previousNotification = notificationBox.getNotificationWithValue(value);
        if (previousNotification)
          notificationBox.removeNotification(previousNotification);

        notificationBox.appendNotification(
          title,
          value,
          "chrome://global/skin/icons/blacklist_favicon.png",
          notificationBox.PRIORITY_CRITICAL_HIGH,
          buttons
        );
      }
    }
}

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
  var url = "about:blank";
  try {
    url = prefs.getComplexValue("browser.startup.homepage",
                                Ci.nsIPrefLocalizedString).data;
    // If url is a pipe-delimited set of pages, just take the first one.
    if (url.indexOf("|") != -1)
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

function getWebNavigation()
{
  try {
    return gBrowser.webNavigation;
  } catch (e) {
    return null;
  }
}

function BrowserReloadWithFlags(reloadFlags) {
  /* First, we'll try to use the session history object to reload so
   * that framesets are handled properly. If we're in a special
   * window (such as view-source) that has no session history, fall
   * back on using the web navigation's reload method.
   */

  var webNav = getWebNavigation();
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
    gBrowser.removeTab(this._printPreviewTab);
    this._printPreviewTab = null;
    gInPrintPreviewMode = false;
    this._toggleAffectedChrome();
  },
  _toggleAffectedChrome: function () {
#ifdef MENUBAR_CAN_AUTOHIDE
    updateAppButtonDisplay();
#endif

    gNavToolbox.hidden = gInPrintPreviewMode;

    if (gInPrintPreviewMode)
      this._hideChrome();
    else
      this._showChrome();

    if (this._chromeState.sidebarOpen)
      toggleSidebar(this._sidebarCommand);
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

    if (this._chromeState.addonBarOpen)
      document.getElementById("addon-bar").collapsed = false;

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
  // Don't show the tooltip if the tooltip node is a XUL element or a document.
  if (tipElement.namespaceURI == "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul" ||
      !tipElement.ownerDocument)
    return retVal;

  const XLinkNS = "http://www.w3.org/1999/xlink";


  var titleText = null;
  var XLinkTitleText = null;
  var SVGTitleText = null;
#ifdef MOZ_SVG
  var lookingForSVGTitle = true;
#else
  var lookingForSVGTitle = false;
#endif // MOZ_SVG
  var direction = tipElement.ownerDocument.dir;

  // If the element is invalid per HTML5 Forms specifications,
  // show the constraint validation error message instead of @tooltip.
  if (tipElement instanceof HTMLInputElement ||
      tipElement instanceof HTMLTextAreaElement ||
      tipElement instanceof HTMLSelectElement ||
      tipElement instanceof HTMLButtonElement) {
    // If the element is barred from constraint validation or valid,
    // the validation message will be the empty string.
    titleText = tipElement.validationMessage;
  }

  while (!titleText && !XLinkTitleText && !SVGTitleText && tipElement) {
    if (tipElement.nodeType == Node.ELEMENT_NODE) {
      titleText = tipElement.getAttribute("title");
      if ((tipElement instanceof HTMLAnchorElement && tipElement.href) ||
          (tipElement instanceof HTMLAreaElement && tipElement.href) ||
          (tipElement instanceof HTMLLinkElement && tipElement.href)
#ifdef MOZ_SVG
          || (tipElement instanceof SVGAElement && tipElement.hasAttributeNS(XLinkNS, "href"))
#endif // MOZ_SVG
          ) {
        XLinkTitleText = tipElement.getAttributeNS(XLinkNS, "title");
      }
      if (lookingForSVGTitle &&
          !(tipElement instanceof SVGElement &&
            tipElement.parentNode instanceof SVGElement &&
            !(tipElement.parentNode instanceof SVGForeignObjectElement))) {
        lookingForSVGTitle = false;
      }
      if (lookingForSVGTitle) {
        let length = tipElement.childNodes.length;
        for (let i = 0; i < length; i++) {
          let childNode = tipElement.childNodes[i];
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

      // Per HTML 4.01 6.2 (CDATA section), literal CRs and tabs should be
      // replaced with spaces, and LFs should be removed entirely.
      // XXX Bug 322270: We don't preserve the result of entities like &#13;,
      // which should result in a line break in the tooltip, because we can't
      // distinguish that from a literal character in the source by this point.
      t = t.replace(/[\r\t]/g, ' ');
      t = t.replace(/\n/g, '');

      tipNode.setAttribute("label", t);
      retVal = true;
    }
  });

  return retVal;
}

var browserDragAndDrop = {
  canDropLink: function (aEvent) Services.droppedLinkHandler.canDropLink(aEvent, true),

  dragOver: function (aEvent, statusString)
  {
    if (this.canDropLink(aEvent)) {
      aEvent.preventDefault();

      if (statusString) {
        XULBrowserWindow.setStatusText(gNavigatorBundle.getString(statusString));
      }
    }
  },

  drop: function (aEvent, aName) Services.droppedLinkHandler.dropLink(aEvent, aName)
};

var homeButtonObserver = {
  onDrop: function (aEvent)
    {
      setTimeout(openHomeDialog, 0, browserDragAndDrop.drop(aEvent, { }));
    },

  onDragOver: function (aEvent)
    {
      browserDragAndDrop.dragOver(aEvent, "droponhomebutton");
      aEvent.dropEffect = "link";
    },
  onDragLeave: function (aEvent)
    {
      XULWindowBrowser.setStatusText("");
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
      PlacesUIUtils.showMinimalAddBookmarkUI(makeURI(url), name);
    } catch(ex) { }
  },

  onDragOver: function (aEvent)
  {
    browserDragAndDrop.dragOver(aEvent, "droponbookmarksbutton");
    aEvent.dropEffect = "link";
  },

  onDragLeave: function (aEvent)
  {
    XULWindowBrowser.setStatusText("");
  }
}

var newTabButtonObserver = {
  onDragOver: function (aEvent)
  {
    browserDragAndDrop.dragOver(aEvent, "droponnewtabbutton");
  },

  onDragLeave: function (aEvent)
  {
    XULWindowBrowser.setStatusText("");
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
    browserDragAndDrop.dragOver(aEvent, "droponnewwindowbutton");
  },
  onDragLeave: function (aEvent)
  {
    XULWindowBrowser.setStatusText("");
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

var DownloadsButtonDNDObserver = {
  onDragOver: function (aEvent)
  {
    XULWindowBrowser.setStatusText(gNavigatorBundle.getString("dropondownloadsbutton"));
    var types = aEvent.dataTransfer.types;
    if (types.contains("text/x-moz-url") ||
        types.contains("text/uri-list") ||
        types.contains("text/plain"))
      aEvent.preventDefault();
  },

  onDragLeave: function (aEvent)
  {
    XULWindowBrowser.setStatusText("");
  },

  onDrop: function (aEvent)
  {
    let name = { };
    let url = browserDragAndDrop.drop(aEvent, name);
    if (url)
      saveURL(url, name, null, true, true);
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
  onLinkAdded: function (event) {
    var link = event.originalTarget;
    var rel = link.rel && link.rel.toLowerCase();
    if (!link || !link.ownerDocument || !rel || !link.href)
      return;

    var feedAdded = false;
    var iconAdded = false;
    var searchAdded = false;
    var relStrings = rel.split(/\s+/);
    var rels = {};
    for (let i = 0; i < relStrings.length; i++)
      rels[relStrings[i]] = true;

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

            var targetDoc = link.ownerDocument;
            var uri = makeURI(link.href, targetDoc.characterSet);

            if (gBrowser.isFailedIcon(uri))
              break;

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
                break;
              }
            }

            try {
              var contentPolicy = Cc["@mozilla.org/layout/content-policy;1"].
                                  getService(Ci.nsIContentPolicy);
            } catch(e) {
              break; // Refuse to load if we can't do a security check.
            }

            // Security says okay, now ask content policy
            if (contentPolicy.shouldLoad(Ci.nsIContentPolicy.TYPE_IMAGE,
                                         uri, targetDoc.documentURIObject,
                                         link, link.type, null)
                                         != Ci.nsIContentPolicy.ACCEPT)
              break;

            var browserIndex = gBrowser.getBrowserIndexForDocument(targetDoc);
            // no browser? no favicon.
            if (browserIndex == -1)
              break;

            let tab = gBrowser.tabs[browserIndex];
            gBrowser.setIcon(tab, link.href);
            iconAdded = true;
          }
          break;
        case "search":
          if (!searchAdded) {
            var type = link.type && link.type.toLowerCase();
            type = type.replace(/^\s+|\s*(?:;.*)?$/g, "");

            if (type == "application/opensearchdescription+xml" && link.title &&
                /^(?:https?|ftp):/i.test(link.href) &&
                !gPrivateBrowsingUI.privateBrowsingEnabled) {
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
        win.focus()
        win.BrowserSearch.webSearch();
      } else {
        // If there are no open browser windows, open a new one

        // This needs to be in a timeout so that we don't end up refocused
        // in the url bar
        function webSearchCallback() {
          setTimeout(BrowserSearch.webSearch, 0);
        }

        win = window.openDialog("chrome://browser/content/", "_blank",
                                "chrome,all,dialog=no", "about:blank");
        win.addEventListener("load", webSearchCallback, false);
      }
      return;
    }
#endif
    var searchBar = this.searchBar;
    if (searchBar && window.fullScreen)
      FullScreen.mouseoverToggle(true);

    if (isElementVisible(searchBar)) {
      searchBar.select();
      searchBar.focus();
    } else {
      openUILinkIn(Services.search.defaultEngine.searchForm, "current");
    }
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
   */
  loadSearch: function BrowserSearch_search(searchText, useNewTab) {
    var engine;

    // If the search bar is visible, use the current engine, otherwise, fall
    // back to the default engine.
    if (isElementVisible(this.searchBar))
      engine = Services.search.currentEngine;
    else
      engine = Services.search.defaultEngine;

    var submission = engine.getSubmission(searchText); // HTML response

    // getSubmission can return null if the engine doesn't have a URL
    // with a text/html response type.  This is unlikely (since
    // SearchService._addEngineToStore() should fail for such an engine),
    // but let's be on the safe side.
    if (!submission)
      return;

    if (useNewTab) {
      gBrowser.loadOneTab(submission.uri.spec, {
                          postData: submission.postData,
                          relatedToCurrent: true});
    } else
      loadURI(submission.uri.spec, null, submission.postData, false);
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
    var regionBundle = document.getElementById("bundle_browser_region");
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

  var webNav = getWebNavigation();
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
      try {
        let iconURL = Cc["@mozilla.org/browser/favicon-service;1"]
                         .getService(Ci.nsIFaviconService)
                         .getFaviconForPage(entry.URI).spec;
        item.style.listStyleImage = "url(" + iconURL + ")";
      } catch (ex) {}
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
  if (aUrlToAdd &&
      aUrlToAdd.indexOf(" ") == -1 &&
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

function OpenBrowserWindow()
{
  var charsetArg = new String();
  var handler = Components.classes["@mozilla.org/browser/clh;1"]
                          .getService(Components.interfaces.nsIBrowserHandler);
  var defaultArgs = handler.defaultArgs;
  var wintype = document.documentElement.getAttribute('windowtype');

  // if and only if the current window is a browser window and it has a document with a character
  // set, then extract the current charset menu setting from the current document and use it to
  // initialize the new browser window...
  var win;
  if (window && (wintype == "navigator:browser") && window.content && window.content.document)
  {
    var DocCharset = window.content.document.characterSet;
    charsetArg = "charset="+DocCharset;

    //we should "inherit" the charset menu setting in a new window
    win = window.openDialog("chrome://browser/content/", "_blank", "chrome,all,dialog=no", defaultArgs, charsetArg);
  }
  else // forget about the charset information.
  {
    win = window.openDialog("chrome://browser/content/", "_blank", "chrome,all,dialog=no", defaultArgs);
  }

  return win;
}

var gCustomizeSheet = false;
// Returns a reference to the window in which the toolbar
// customization document is loaded.
function BrowserCustomizeToolbar()
{
  // Disable the toolbar context menu items
  var menubar = document.getElementById("main-menubar");
  for (var i = 0; i < menubar.childNodes.length; ++i)
    menubar.childNodes[i].setAttribute("disabled", true);

  var cmd = document.getElementById("cmd_CustomizeToolbars");
  cmd.setAttribute("disabled", "true");

  var splitter = document.getElementById("urlbar-search-splitter");
  if (splitter)
    splitter.parentNode.removeChild(splitter);

  CombinedStopReload.uninit();

  PlacesToolbarHelper.customizeStart();
  BookmarksMenuButton.customizeStart();

  var customizeURL = "chrome://global/content/customizeToolbar.xul";
  gCustomizeSheet = getBoolPref("toolbar.customization.usesheet", false);

  if (gCustomizeSheet) {
    var sheetFrame = document.getElementById("customizeToolbarSheetIFrame");
    var panel = document.getElementById("customizeToolbarSheetPopup");
    sheetFrame.hidden = false;
    sheetFrame.toolbox = gNavToolbox;
    sheetFrame.panel = panel;

    // The document might not have been loaded yet, if this is the first time.
    // If it is already loaded, reload it so that the onload initialization code
    // re-runs.
    if (sheetFrame.getAttribute("src") == customizeURL)
      sheetFrame.contentWindow.location.reload()
    else
      sheetFrame.setAttribute("src", customizeURL);

    // Open the panel, but make it invisible until the iframe has loaded so
    // that the user doesn't see a white flash.
    panel.style.visibility = "hidden";
    gNavToolbox.addEventListener("beforecustomization", function () {
      gNavToolbox.removeEventListener("beforecustomization", arguments.callee, false);
      panel.style.removeProperty("visibility");
    }, false);
    panel.openPopup(gNavToolbox, "after_start", 0, 0);
    return sheetFrame.contentWindow;
  } else {
    return window.openDialog(customizeURL,
                             "CustomizeToolbar",
                             "chrome,titlebar,toolbar,location,resizable,dependent",
                             gNavToolbox);
  }
}

function BrowserToolboxCustomizeDone(aToolboxChanged) {
  if (gCustomizeSheet) {
    document.getElementById("customizeToolbarSheetIFrame").hidden = true;
    document.getElementById("customizeToolbarSheetPopup").hidePopup();
  }

  // Update global UI elements that may have been added or removed
  if (aToolboxChanged) {
    gURLBar = document.getElementById("urlbar");

    gProxyFavIcon = document.getElementById("page-proxy-favicon");
    gHomeButton.updateTooltip();
    gIdentityHandler._cacheElements();
    window.XULBrowserWindow.init();

    var backForwardDropmarker = document.getElementById("back-forward-dropmarker");
    if (backForwardDropmarker)
      backForwardDropmarker.disabled =
        document.getElementById('Browser:Back').hasAttribute('disabled') &&
        document.getElementById('Browser:Forward').hasAttribute('disabled');

#ifndef XP_MACOSX
    updateEditUIVisibility();
#endif
  }

  PlacesToolbarHelper.customizeDone();
  BookmarksMenuButton.customizeDone();

  UpdateUrlbarSearchSplitterState();

  CombinedStopReload.init();

  // Update the urlbar
  if (gURLBar) {
    URLBarSetURI();
    XULBrowserWindow.asyncUpdateUI();
    PlacesStarButton.updateState();
  }

  // Re-enable parts of the UI we disabled during the dialog
  var menubar = document.getElementById("main-menubar");
  for (var i = 0; i < menubar.childNodes.length; ++i)
    menubar.childNodes[i].setAttribute("disabled", false);
  var cmd = document.getElementById("cmd_CustomizeToolbars");
  cmd.removeAttribute("disabled");

#ifdef XP_MACOSX
  // make sure to re-enable click-and-hold
  if (!getBoolPref("ui.click_hold_context_menus", false))
    SetClickAndHoldHandlers();
#endif

  // XXX Shouldn't have to do this, but I do
  if (!gCustomizeSheet)
    window.focus();
}

function BrowserToolboxCustomizeChange() {
  gHomeButton.updatePersonalToolbarStyle();
  BookmarksMenuButton.customizeChange();
  allTabs.readPref();
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

var FullScreen = {
  _XULNS: "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
  toggle: function (event) {
    var enterFS = window.fullScreen;

    // We get the fullscreen event _before_ the window transitions into or out of FS mode.
    if (event && event.type == "fullscreen")
      enterFS = !enterFS;

    // show/hide all menubars, toolbars (except the full screen toolbar)
    this.showXULChrome("toolbar", !enterFS);
    document.getElementById("View:FullScreen").setAttribute("checked", enterFS);

    if (enterFS) {
      // Add a tiny toolbar to receive mouseover and dragenter events, and provide affordance.
      // This will help simulate the "collapse" metaphor while also requiring less code and
      // events than raw listening of mouse coords.
      let fullScrToggler = document.getElementById("fullscr-toggler");
      if (!fullScrToggler) {
        fullScrToggler = document.createElement("hbox");
        fullScrToggler.id = "fullscr-toggler";
        fullScrToggler.collapsed = true;
        gNavToolbox.parentNode.insertBefore(fullScrToggler, gNavToolbox.nextSibling);
      }
      fullScrToggler.addEventListener("mouseover", this._expandCallback, false);
      fullScrToggler.addEventListener("dragenter", this._expandCallback, false);

      if (gPrefService.getBoolPref("browser.fullscreen.autohide"))
        gBrowser.mPanelContainer.addEventListener("mousemove",
                                                  this._collapseCallback, false);

      document.addEventListener("keypress", this._keyToggleCallback, false);
      document.addEventListener("popupshown", this._setPopupOpen, false);
      document.addEventListener("popuphidden", this._setPopupOpen, false);
      this._shouldAnimate = true;
      this.mouseoverToggle(false);

      // Autohide prefs
      gPrefService.addObserver("browser.fullscreen", this, false);
    }
    else {
      // The user may quit fullscreen during an animation
      clearInterval(this._animationInterval);
      clearTimeout(this._animationTimeout);
      gNavToolbox.style.marginTop = "0px";
      if (this._isChromeCollapsed)
        this.mouseoverToggle(true);
      this._isAnimating = false;
      // This is needed if they use the context menu to quit fullscreen
      this._isPopupOpen = false;

      this.cleanup();
    }
  },

  cleanup: function () {
    if (window.fullScreen) {
      gBrowser.mPanelContainer.removeEventListener("mousemove",
                                                   this._collapseCallback, false);
      document.removeEventListener("keypress", this._keyToggleCallback, false);
      document.removeEventListener("popupshown", this._setPopupOpen, false);
      document.removeEventListener("popuphidden", this._setPopupOpen, false);
      gPrefService.removeObserver("browser.fullscreen", this);

      let fullScrToggler = document.getElementById("fullscr-toggler");
      if (fullScrToggler) {
        fullScrToggler.removeEventListener("mouseover", this._expandCallback, false);
        fullScrToggler.removeEventListener("dragenter", this._expandCallback, false);
      }
    }
  },

  observe: function(aSubject, aTopic, aData)
  {
    if (aData == "browser.fullscreen.autohide") {
      if (gPrefService.getBoolPref("browser.fullscreen.autohide")) {
        gBrowser.mPanelContainer.addEventListener("mousemove",
                                                  this._collapseCallback, false);
      }
      else {
        gBrowser.mPanelContainer.removeEventListener("mousemove",
                                                     this._collapseCallback, false);
      }
    }
  },

  // Event callbacks
  _expandCallback: function()
  {
    FullScreen.mouseoverToggle(true);
  },
  _collapseCallback: function()
  {
    FullScreen.mouseoverToggle(false);
  },
  _keyToggleCallback: function(aEvent)
  {
    // if we can use the keyboard (eg Ctrl+L or Ctrl+E) to open the toolbars, we
    // should provide a way to collapse them too.
    if (aEvent.keyCode == aEvent.DOM_VK_ESCAPE) {
      FullScreen._shouldAnimate = false;
      FullScreen.mouseoverToggle(false, true);
    }
    // F6 is another shortcut to the address bar, but its not covered in OpenLocation()
    else if (aEvent.keyCode == aEvent.DOM_VK_F6)
      FullScreen.mouseoverToggle(true);
  },

  // Checks whether we are allowed to collapse the chrome
  _isPopupOpen: false,
  _isChromeCollapsed: false,
  _safeToCollapse: function(forceHide)
  {
    if (!gPrefService.getBoolPref("browser.fullscreen.autohide"))
      return false;

    // a popup menu is open in chrome: don't collapse chrome
    if (!forceHide && this._isPopupOpen)
      return false;

    // a textbox in chrome is focused (location bar anyone?): don't collapse chrome
    if (document.commandDispatcher.focusedElement &&
        document.commandDispatcher.focusedElement.ownerDocument == document &&
        document.commandDispatcher.focusedElement.localName == "input") {
      if (forceHide)
        // hidden textboxes that still have focus are bad bad bad
        document.commandDispatcher.focusedElement.blur();
      else
        return false;
    }
    return true;
  },

  _setPopupOpen: function(aEvent)
  {
    // Popups should only veto chrome collapsing if they were opened when the chrome was not collapsed.
    // Otherwise, they would not affect chrome and the user would expect the chrome to go away.
    // e.g. we wouldn't want the autoscroll icon firing this event, so when the user
    // toggles chrome when moving mouse to the top, it doesn't go away again.
    if (aEvent.type == "popupshown" && !FullScreen._isChromeCollapsed &&
        aEvent.target.localName != "tooltip" && aEvent.target.localName != "window")
      FullScreen._isPopupOpen = true;
    else if (aEvent.type == "popuphidden" && aEvent.target.localName != "tooltip" &&
             aEvent.target.localName != "window")
      FullScreen._isPopupOpen = false;
  },

  // Autohide helpers for the context menu item
  getAutohide: function(aItem)
  {
    aItem.setAttribute("checked", gPrefService.getBoolPref("browser.fullscreen.autohide"));
  },
  setAutohide: function()
  {
    gPrefService.setBoolPref("browser.fullscreen.autohide", !gPrefService.getBoolPref("browser.fullscreen.autohide"));
  },

  // Animate the toolbars disappearing
  _shouldAnimate: true,
  _isAnimating: false,
  _animationTimeout: null,
  _animationInterval: null,
  _animateUp: function()
  {
    // check again, the user may have done something before the animation was due to start
    if (!window.fullScreen || !FullScreen._safeToCollapse(false)) {
      FullScreen._isAnimating = false;
      FullScreen._shouldAnimate = true;
      return;
    }

    var animateFrameAmount = 2;
    function animateUpFrame() {
      animateFrameAmount *= 2;
      if (animateFrameAmount >= gNavToolbox.boxObject.height) {
        // We've animated enough
        clearInterval(FullScreen._animationInterval);
        gNavToolbox.style.marginTop = "0px";
        FullScreen._isAnimating = false;
        FullScreen._shouldAnimate = false; // Just to make sure
        FullScreen.mouseoverToggle(false);
        return;
      }
      gNavToolbox.style.marginTop = (animateFrameAmount * -1) + "px";
    }

    FullScreen._animationInterval = setInterval(animateUpFrame, 70);
  },

  mouseoverToggle: function(aShow, forceHide)
  {
    // Don't do anything if:
    // a) we're already in the state we want,
    // b) we're animating and will become collapsed soon, or
    // c) we can't collapse because it would be undesirable right now
    if (aShow != this._isChromeCollapsed || (!aShow && this._isAnimating) ||
        (!aShow && !this._safeToCollapse(forceHide)))
      return;

    // browser.fullscreen.animateUp
    // 0 - never animate up
    // 1 - animate only for first collapse after entering fullscreen (default for perf's sake)
    // 2 - animate every time it collapses
    if (gPrefService.getIntPref("browser.fullscreen.animateUp") == 0)
      this._shouldAnimate = false;

    if (!aShow && this._shouldAnimate) {
      this._isAnimating = true;
      this._shouldAnimate = false;
      this._animationTimeout = setTimeout(this._animateUp, 800);
      return;
    }

    // The chrome is collapsed so don't spam needless mousemove events
    if (aShow) {
      gBrowser.mPanelContainer.addEventListener("mousemove",
                                                this._collapseCallback, false);
    }
    else {
      gBrowser.mPanelContainer.removeEventListener("mousemove",
                                                   this._collapseCallback, false);
    }

    // Hiding/collapsing the toolbox interferes with the tab bar's scrollbox,
    // so we just move it off-screen instead. See bug 430687.
    gNavToolbox.style.marginTop =
      aShow ? "" : -gNavToolbox.getBoundingClientRect().height + "px";

    document.getElementById("fullscr-toggler").collapsed = aShow;
    this._isChromeCollapsed = !aShow;
    if (gPrefService.getIntPref("browser.fullscreen.animateUp") == 2)
      this._shouldAnimate = true;
  },

  showXULChrome: function(aTag, aShow)
  {
    var els = document.getElementsByTagNameNS(this._XULNS, aTag);

    for (var i = 0; i < els.length; ++i) {
      // XXX don't interfere with previously collapsed toolbars
      if (els[i].getAttribute("fullscreentoolbar") == "true") {
        if (!aShow) {

          var toolbarMode = els[i].getAttribute("mode");
          if (toolbarMode != "text") {
            els[i].setAttribute("saved-mode", toolbarMode);
            els[i].setAttribute("saved-iconsize",
                                els[i].getAttribute("iconsize"));
            els[i].setAttribute("mode", "icons");
            els[i].setAttribute("iconsize", "small");
          }

          // Give the main nav bar the fullscreen context menu, otherwise remove it
          // to prevent breakage
          els[i].setAttribute("saved-context",
                              els[i].getAttribute("context"));
          if (els[i].id == "nav-bar")
            els[i].setAttribute("context", "autohide-context");
          else
            els[i].removeAttribute("context");

          // Set the inFullscreen attribute to allow specific styling
          // in fullscreen mode
          els[i].setAttribute("inFullscreen", true);
        }
        else {
          function restoreAttr(attrName) {
            var savedAttr = "saved-" + attrName;
            if (els[i].hasAttribute(savedAttr)) {
              els[i].setAttribute(attrName, els[i].getAttribute(savedAttr));
              els[i].removeAttribute(savedAttr);
            }
          }

          restoreAttr("mode");
          restoreAttr("iconsize");
          restoreAttr("context");

          els[i].removeAttribute("inFullscreen");
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

    if (aShow) {
      gNavToolbox.removeAttribute("inFullscreen");
      document.documentElement.removeAttribute("inFullscreen");
    } else {
      gNavToolbox.setAttribute("inFullscreen", true);
      document.documentElement.setAttribute("inFullscreen", true);
    }

    var controls = document.getElementsByAttribute("fullscreencontrol", "true");
    for (var i = 0; i < controls.length; ++i)
      controls[i].hidden = aShow;
  }
};

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
  startTime: 0,
  statusText: "",
  isBusy: false,

  _progressCollapseTimer: 0,

  QueryInterface: function (aIID) {
    if (aIID.equals(Ci.nsIWebProgressListener) ||
        aIID.equals(Ci.nsIWebProgressListener2) ||
        aIID.equals(Ci.nsISupportsWeakReference) ||
        aIID.equals(Ci.nsIXULBrowserWindow) ||
        aIID.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_NOINTERFACE;
  },

  get statusMeter () {
    delete this.statusMeter;
    return this.statusMeter = document.getElementById("urlbar-progress");
  },
  get stopCommand () {
    delete this.stopCommand;
    return this.stopCommand = document.getElementById("Browser:Stop");
  },
  get reloadCommand () {
    delete this.reloadCommand;
    return this.reloadCommand = document.getElementById("Browser:Reload");
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

    // Initialize the security button's state and tooltip text.  Remember to reset
    // _hostChanged, otherwise onSecurityChange will short circuit.
    var securityUI = gBrowser.securityUI;
    this._hostChanged = true;
    this.onSecurityChange(null, null, securityUI.state);
  },

  destroy: function () {
    // XXXjag to avoid leaks :-/, see bug 60729
    delete this.throbberElement;
    delete this.statusMeter;
    delete this.stopCommand;
    delete this.reloadCommand;
    delete this.statusText;
  },

  setJSStatus: function (status) {
    this.jsStatus = status;
  },

  setJSDefaultStatus: function (status) {
    this.jsDefaultStatus = status;
  },

  setDefaultStatus: function (status) {
    this.defaultStatus = status;
  },

  setOverLink: function (link) {
    // Encode bidirectional formatting characters.
    // (RFC 3987 sections 3.2 and 4.1 paragraph 6)
    link = link.replace(/[\u200e\u200f\u202a\u202b\u202c\u202d\u202e]/g,
                        encodeURIComponent);
    gURLBar.setOverLink(link);
  },

  onLinkIconAvailable: function (aIconURL) {
    if (gProxyFavIcon && gBrowser.userTypedValue === null)
      PageProxySetIcon(aIconURL); // update the favicon in the URL bar
  },

  onProgressChange: function (aWebProgress, aRequest,
                              aCurSelfProgress, aMaxSelfProgress,
                              aCurTotalProgress, aMaxTotalProgress) {
    // Check this._busyUI to be safe, because we don't want to update
    // the progress meter when restoring a page from bfcache.
    if (aMaxTotalProgress > 0 && this._busyUI) {
      // This is highly optimized.  Don't touch this code unless
      // you are intimately familiar with the cost of setting
      // attrs on XUL elements. -- hyatt
      let percentage = (aCurTotalProgress * 100) / aMaxTotalProgress;
      this.statusMeter.value = percentage;
    }
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

        // Turn the status meter on.
        this.statusMeter.value = 0;  // be sure to clear the progress bar
        if (this._progressCollapseTimer) {
          clearTimeout(this._progressCollapseTimer);
          this._progressCollapseTimer = 0;
        }
        else
          this.statusMeter.collapsed = false;

        // XXX: This needs to be based on window activity...
        this.stopCommand.removeAttribute("disabled");
        CombinedStopReload.switchToStop();
      }
    }
    else if (aStateFlags & nsIWebProgressListener.STATE_STOP) {
      if (aStateFlags & nsIWebProgressListener.STATE_IS_NETWORK) {
        if (aWebProgress.DOMWindow == content) {
          if (aRequest)
            this.endDocumentLoad(aRequest, aStatus);
          if (!gBrowser.mTabbedMode && !gBrowser.getIcon())
            gBrowser.useDefaultIcon(gBrowser.selectedTab);
        }
      }

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
              case Components.results.NS_BINDING_ABORTED:
                msg = gNavigatorBundle.getString("nv_stopped");
                break;
              case Components.results.NS_ERROR_NET_TIMEOUT:
                msg = gNavigatorBundle.getString("nv_timeout");
                break;
            }
          }
        }
        // If msg is false then we did not have an error (channel may have
        // been null, in the case of a stray image load).
        if (!msg && (!location || location.spec != "about:blank"))
          msg = gNavigatorBundle.getString("nv_done");

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

        // Turn the progress meter and throbber off.
        this._progressCollapseTimer = setTimeout(function (self) {
          self.statusMeter.collapsed = true;
          self._progressCollapseTimer = 0;
        }, 100, this);

        if (this.throbberElement)
          this.throbberElement.removeAttribute("busy");

        this.stopCommand.setAttribute("disabled", "true");
        CombinedStopReload.switchToReload(aRequest instanceof Ci.nsIRequest);
      }
    }
  },

  onLocationChange: function (aWebProgress, aRequest, aLocationURI) {
    var location = aLocationURI ? aLocationURI.spec : "";
    this._hostChanged = true;

    // Hide the form invalid popup.
    if (gFormSubmitObserver.panelIsOpen()) {
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

    this.setOverLink("", null);

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

      if (!gBrowser.mTabbedMode && aWebProgress.isLoadingDocument)
        gBrowser.setIcon(gBrowser.selectedTab, null);

      if (gURLBar) {
        // Strip off "wyciwyg://" and passwords for the location bar
        let uri = aLocationURI;
        try {
          uri = this._uriFixup.createExposableURI(uri);
        } catch (e) {}
        URLBarSetURI(uri);

        // Update starring UI
        PlacesStarButton.updateState();
      }
    }
    UpdateBackForwardCommands(gBrowser.webNavigation);

    if (gFindBarInitialized) {
      if (gFindBar.findMode != gFindBar.FIND_NORMAL) {
        // Close the Find toolbar if we're in old-style TAF mode
        gFindBar.close();
      }

      // XXXmano new-findbar, do something useful once it lands.
      // Of course, this is especially wrong with bfcache on...

      // fix bug 253793 - turn off highlight when page changes
      gFindBar.getElement("highlight").checked = false;      
    }

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

  onStatusChange: function (aWebProgress, aRequest, aStatus, aMessage) {
    this.status = aMessage;
  },

  // Properties used to cache security state used to update the UI
  _state: null,
  _tooltipText: null,
  _hostChanged: false, // onLocationChange will flip this bit

  onSecurityChange: function (aWebProgress, aRequest, aState) {
    // Don't need to do anything if the data we use to update the UI hasn't
    // changed
    if (this._state == aState &&
        this._tooltipText == gBrowser.securityUI.tooltipText &&
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
    this._tooltipText = gBrowser.securityUI.tooltipText

    // aState is defined as a bitmask that may be extended in the future.
    // We filter out any unknown bits before testing for known values.
    const wpl = Components.interfaces.nsIWebProgressListener;
    const wpl_security_bits = wpl.STATE_IS_SECURE |
                              wpl.STATE_IS_BROKEN |
                              wpl.STATE_IS_INSECURE |
                              wpl.STATE_SECURE_HIGH |
                              wpl.STATE_SECURE_MED |
                              wpl.STATE_SECURE_LOW;
    var level;

    switch (this._state & wpl_security_bits) {
      case wpl.STATE_IS_SECURE | wpl.STATE_SECURE_HIGH:
        level = "high";
        break;
      case wpl.STATE_IS_SECURE | wpl.STATE_SECURE_MED:
      case wpl.STATE_IS_SECURE | wpl.STATE_SECURE_LOW:
        level = "low";
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
    this.onProgressChange(gBrowser.webProgress, 0, 0, aTotalProgress, 1);
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

    if (!aDelay || this._stopClicked) {
      this._stopClicked = false;
      this._cancelTransition();
      this.reload.removeAttribute("displaystop");
      return;
    }

    if (this._timer)
      return;

    this._timer = setTimeout(function (self) {
      self._timer = 0;
      self.reload.removeAttribute("displaystop");
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

    // Attach a listener to watch for "click" events bubbling up from error
    // pages and other similar page. This lets us fix bugs like 401575 which
    // require error page UI to do privileged things, without letting error
    // pages have any privilege themselves.
    // We can't look for this during onLocationChange since at that point the
    // document URI is not yet the about:-uri of the error page.

    if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
        /^about:/.test(aBrowser.contentWindow.document.documentURI)) {
      aBrowser.addEventListener("click", BrowserOnClick, false);
      aBrowser.addEventListener("pagehide", function () {
        aBrowser.removeEventListener("click", BrowserOnClick, false);
        aBrowser.removeEventListener("pagehide", arguments.callee, true);
      }, true);
    }
  },

  onLocationChange: function (aBrowser, aWebProgress, aRequest, aLocationURI) {
    // Filter out any sub-frame loads
    if (aBrowser.contentWindow == aWebProgress.DOMWindow)
      FullZoom.onLocationChange(aLocationURI, false, aBrowser);
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

    if (aWhere == Ci.nsIBrowserDOMWindow.OPEN_DEFAULTWINDOW)
      aWhere = gPrefService.getIntPref("browser.link.open_newwindow");
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
        if (!window.document.documentElement.getAttribute("chromehidden"))
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
          content.focus();
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

  var i;

  // Empty the menu
  for (i = popup.childNodes.length-1; i >= 0; --i) {
    var deadItem = popup.childNodes[i];
    if (deadItem.hasAttribute("toolbarId"))
      popup.removeChild(deadItem);
  }

  var firstMenuItem = aInsertPoint || popup.firstChild;

  let toolbarNodes = [document.getElementById("addon-bar")];
  for (i = 0; i < gNavToolbox.childNodes.length; ++i)
    toolbarNodes.push(gNavToolbox.childNodes[i]);
  toolbarNodes.forEach(function(toolbar) {
    var toolbarName = toolbar.getAttribute("toolbarname");
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
      popup.insertBefore(menuItem, firstMenuItem);

      menuItem.addEventListener("command", onViewToolbarCommand, false);
    }
  }, this);
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

#ifdef MENUBAR_CAN_AUTOHIDE
  updateAppButtonDisplay();
#endif
}

var TabsOnTop = {
  toggle: function () {
    this.enabled = !this.enabled;
  },
  syncCommand: function () {
    let enabled = this.enabled;
    document.getElementById("cmd_ToggleTabsOnTop")
            .setAttribute("checked", enabled);
    document.documentElement.setAttribute("tabsontop", enabled);
    document.getElementById("TabsToolbar").setAttribute("tabsontop", enabled);
    gBrowser.tabContainer.setAttribute("tabsontop", enabled);
  },
  get enabled () {
    return gNavToolbox.getAttribute("tabsontop") == "true";
  },
  set enabled (val) {
    gNavToolbox.setAttribute("tabsontop", !!val);
    this.syncCommand();

    return val;
  }
}

#ifdef MENUBAR_CAN_AUTOHIDE
function updateAppButtonDisplay() {
  var displayAppButton =
    !gInPrintPreviewMode &&
    window.menubar.visible &&
    document.getElementById("toolbar-menubar").getAttribute("autohide") == "true";

  document.getElementById("titlebar").hidden = !displayAppButton;

  if (displayAppButton)
    document.documentElement.setAttribute("chromemargin", "0,-1,-1,-1");
  else
    document.documentElement.removeAttribute("chromemargin");
}

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
      sidebarBroadcaster.removeAttribute("checked");
      sidebarBox.setAttribute("sidebarcommand", "");
      sidebarTitle.value = "";
      sidebar.setAttribute("src", "about:blank");
      sidebarBox.hidden = true;
      sidebarSplitter.hidden = true;
      content.focus();
    } else {
      fireSidebarFocusedEvent();
    }
    return;
  }

  // now we need to show the specified sidebar

  // ..but first update the 'checked' state of all sidebar broadcasters
  var broadcasters = document.getElementsByAttribute("group", "sidebar");
  for (var i = 0; i < broadcasters.length; ++i) {
    // skip elements that observe sidebar broadcasters and random
    // other elements
    if (broadcasters[i].localName != "broadcaster")
      continue;

    if (broadcasters[i] != sidebarBroadcaster)
      broadcasters[i].removeAttribute("checked");
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
  // element itself, because the code in BrowserShutdown persists this
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

  var focusedWindow = document.commandDispatcher.focusedWindow;
  var selection = focusedWindow.getSelection().toString();

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

 // Called whenever the user clicks in the content area,
 // except when left-clicking on links (special case)
 // should always return true for click to go through
 function contentAreaClick(event, fieldNormalClicks)
 {
   if (!event.isTrusted || event.getPreventDefault()) {
     return true;
   }

   var target = event.target;
   var linkNode;

   if (target instanceof HTMLAnchorElement ||
       target instanceof HTMLAreaElement ||
       target instanceof HTMLLinkElement) {
     if (target.hasAttribute("href"))
       linkNode = target;

     // xxxmpc: this is kind of a hack to work around a Gecko bug (see bug 266932)
     // we're going to walk up the DOM looking for a parent link node,
     // this shouldn't be necessary, but we're matching the existing behaviour for left click
     var parent = target.parentNode;
     while (parent) {
       if (parent instanceof HTMLAnchorElement ||
           parent instanceof HTMLAreaElement ||
           parent instanceof HTMLLinkElement) {
           if (parent.hasAttribute("href"))
             linkNode = parent;
       }
       parent = parent.parentNode;
     }
   }
   else {
     linkNode = event.originalTarget;
     while (linkNode && !(linkNode instanceof HTMLAnchorElement))
       linkNode = linkNode.parentNode;
     // <a> cannot be nested.  So if we find an anchor without an
     // href, there is no useful <a> around the target
     if (linkNode && !linkNode.hasAttribute("href"))
       linkNode = null;
   }
   var wrapper = null;
   if (linkNode) {
     wrapper = linkNode;
     if (event.button == 0 && !event.ctrlKey && !event.shiftKey &&
         !event.altKey && !event.metaKey) {
       // A Web panel's links should target the main content area.  Do this
       // if no modifier keys are down and if there's no target or the target equals
       // _main (the IE convention) or _content (the Mozilla convention).
       // XXX Now that markLinkVisited is gone, we may not need to field _main and
       // _content here.
       target = wrapper.getAttribute("target");
       if (fieldNormalClicks &&
           (!target || target == "_content" || target  == "_main"))
         // IE uses _main, SeaMonkey uses _content, we support both
       {
         if (!wrapper.href)
           return true;
         if (wrapper.getAttribute("onclick"))
           return true;
         // javascript links should be executed in the current browser
         if (wrapper.href.substr(0, 11) === "javascript:")
           return true;
         // data links should be executed in the current browser
         if (wrapper.href.substr(0, 5) === "data:")
           return true;

         try {
           urlSecurityCheck(wrapper.href, wrapper.ownerDocument.nodePrincipal);
         }
         catch(ex) {
           return false;
         }

         var postData = { };
         var url = getShortcutOrURI(wrapper.href, postData);
         if (!url)
           return true;
         loadURI(url, null, postData.value, false);
         event.preventDefault();
         return false;
       }
       else if (linkNode.getAttribute("rel") == "sidebar") {
         // This is the Opera convention for a special link that - when clicked - allows
         // you to add a sidebar panel.  We support the Opera convention here.  The link's
         // title attribute contains the title that should be used for the sidebar panel.
         PlacesUIUtils.showMinimalAddBookmarkUI(makeURI(wrapper.href),
                                                wrapper.getAttribute("title"),
                                                null, null, true, true);
         event.preventDefault();
         return false;
       }
     }
     else {
       handleLinkClick(event, wrapper.href, linkNode);
     }

     return true;
   } else {
     // Try simple XLink
     var href, realHref, baseURI;
     linkNode = target;
     while (linkNode) {
       if (linkNode.nodeType == Node.ELEMENT_NODE) {
         wrapper = linkNode;

         realHref = wrapper.getAttributeNS("http://www.w3.org/1999/xlink", "href");
         if (realHref) {
           href = realHref;
           baseURI = wrapper.baseURI
         }
       }
       linkNode = linkNode.parentNode;
     }
     if (href) {
       href = makeURLAbsolute(baseURI, href);
       handleLinkClick(event, href, null);
       return true;
     }
   }
   if (event.button == 1 &&
       gPrefService.getBoolPref("middlemouse.contentLoadURL") &&
       !gPrefService.getBoolPref("general.autoScroll")) {
     middleMousePaste(event);
   }
   return true;
 }

function handleLinkClick(event, href, linkNode)
{
  var doc = event.target.ownerDocument;

  switch (event.button) {
    case 0:    // if left button clicked
#ifdef XP_MACOSX
      if (event.metaKey) { // Cmd
#else
      if (event.ctrlKey) {
#endif
        openNewTabWith(href, doc, null, event, false);
        event.stopPropagation();
        return true;
      }

      if (event.shiftKey && event.altKey) {
        var feedService =
            Cc["@mozilla.org/browser/feeds/result-service;1"].
            getService(Ci.nsIFeedResultService);
        feedService.forcePreviewPage = true;
        loadURI(href, null, null, false);
        return false;
      }

      if (event.shiftKey) {
        openNewWindowWith(href, doc, null, false);
        event.stopPropagation();
        return true;
      }

      if (event.altKey) {
        saveURL(href, linkNode ? gatherTextUnder(linkNode) : "", null, true,
                true, doc.documentURIObject);
        return true;
      }

      return false;
    case 1:    // if middle button clicked
      var tab = gPrefService.getBoolPref("browser.tabs.opentabfor.middleclick");
      if (tab)
        openNewTabWith(href, doc, null, event, false);
      else
        openNewWindowWith(href, doc, null, false);
      event.stopPropagation();
      return true;
  }
  return false;
}

function middleMousePaste(event) {
  var url = getShortcutOrURI(readFromClipboard());
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

  openUILink(url,
             event,
             true /* ignore the fact this is a middle click */);

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
        SetForcedDetector(true);
        SelectDetector(event, false);
    } else if (name == 'charsetGroup') {
        var charset = node.getAttribute('id');
        charset = charset.substring('charset.'.length, charset.length)
        SetForcedCharset(charset);
    } else if (name == 'charsetCustomize') {
        //do nothing - please remove this else statement, once the charset prefs moves to the pref window
    } else {
        SetForcedCharset(node.getAttribute('id'));
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

function SetForcedDetector(doReload)
{
    BrowserSetForcedDetector(doReload);
}

function SetForcedCharset(charset)
{
    BrowserSetForcedCharacterSet(charset);
}

function BrowserSetForcedCharacterSet(aCharset)
{
  var docCharset = gBrowser.docShell.QueryInterface(Ci.nsIDocCharset);
  docCharset.charset = aCharset;
  // Save the forced character-set
  PlacesUtils.history.setCharsetForURI(getWebNavigation().currentURI, aCharset);
  BrowserReloadWithFlags(nsIWebNavigation.LOAD_FLAGS_CHARSET_CHANGE);
}

function BrowserSetForcedDetector(doReload)
{
  gBrowser.documentCharsetInfo.forcedDetector = true;
  if (doReload)
    BrowserReloadWithFlags(nsIWebNavigation.LOAD_FLAGS_CHARSET_CHANGE);
}

function UpdateCurrentCharset()
{
    // extract the charset from DOM
    var wnd = document.commandDispatcher.focusedWindow;
    if ((window == wnd) || (wnd == null)) wnd = window.content;

    // Uncheck previous item
    if (gPrevCharset) {
        var pref_item = document.getElementById('charset.' + gPrevCharset);
        if (pref_item)
          pref_item.setAttribute('checked', 'false');
    }

    var menuitem = document.getElementById('charset.' + wnd.document.characterSet);
    if (menuitem) {
        menuitem.setAttribute('checked', 'true');
    }
}

function UpdateCharsetDetector() {
  var prefvalue = "off";

  try {
    prefvalue = gPrefService.getComplexValue("intl.charset.detector", Ci.nsIPrefLocalizedString).data;
  }
  catch (ex) {}

  prefvalue = "chardet." + prefvalue;

  var menuitem = document.getElementById(prefvalue);
  if (menuitem)
    menuitem.setAttribute("checked", "true");
}

function UpdateMenus(event) {
  // use setTimeout workaround to delay checkmark the menu
  // when onmenucomplete is ready then use it instead of oncreate
  // see bug 78290 for the detail
  UpdateCurrentCharset();
  setTimeout(UpdateCurrentCharset, 0);
  UpdateCharsetDetector();
  setTimeout(UpdateCharsetDetector, 0);
}

function CreateMenu(node) {
  Services.obs.notifyObservers(null, "charsetmenu-selected", node);
}

function charsetLoadListener(event) {
  var charset = window.content.document.characterSet;

  if (charset.length > 0 && (charset != gLastBrowserCharset)) {
    if (!gCharsetMenu)
      gCharsetMenu = Cc['@mozilla.org/rdf/datasource;1?name=charset-menu'].getService(Ci.nsICurrentCharsetListener);
    gCharsetMenu.SetCurrentCharset(charset);
    gPrevCharset = gLastBrowserCharset;
    gLastBrowserCharset = charset;
  }
}

/* Begin Page Style Functions */
function getAllStyleSheets(frameset) {
  var styleSheetsArray = Array.slice(frameset.document.styleSheets);
  for (let i = 0; i < frameset.frames.length; i++) {
    let frameSheets = getAllStyleSheets(frameset.frames[i]);
    styleSheetsArray = styleSheetsArray.concat(frameSheets);
  }
  return styleSheetsArray;
}

function stylesheetFillPopup(menuPopup) {
  var noStyle = menuPopup.firstChild;
  var persistentOnly = noStyle.nextSibling;
  var sep = persistentOnly.nextSibling;
  while (sep.nextSibling)
    menuPopup.removeChild(sep.nextSibling);

  var styleSheets = getAllStyleSheets(window.content);
  var currentStyleSheets = {};
  var styleDisabled = getMarkupDocumentViewer().authorStyleDisabled;
  var haveAltSheets = false;
  var altStyleSelected = false;

  for (let i = 0; i < styleSheets.length; ++i) {
    let currentStyleSheet = styleSheets[i];

    if (!currentStyleSheet.title)
      continue;

    // Skip any stylesheets that don't match the screen media type.
    if (currentStyleSheet.media.length > 0) {
      let media = currentStyleSheet.media.mediaText.split(", ");
      if (media.indexOf("screen") == -1 &&
          media.indexOf("all") == -1)
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
  return true;
}

function stylesheetInFrame(frame, title) {
  return Array.some(frame.document.styleSheets,
                    function (stylesheet) stylesheet.title == title);
}

function stylesheetSwitchFrame(frame, title) {
  var docStyleSheets = frame.document.styleSheets;

  for (let i = 0; i < docStyleSheets.length; ++i) {
    let docStyleSheet = docStyleSheets[i];

    if (title == "_nostyle")
      docStyleSheet.disabled = true;
    else if (docStyleSheet.title)
      docStyleSheet.disabled = (docStyleSheet.title != title);
    else if (docStyleSheet.disabled)
      docStyleSheet.disabled = false;
  }
}

function stylesheetSwitchAll(frameset, title) {
  if (!title || title == "_nostyle" || stylesheetInFrame(frameset, title))
    stylesheetSwitchFrame(frameset, title);

  for (let i = 0; i < frameset.frames.length; i++)
    stylesheetSwitchAll(frameset.frames[i], title);
}

function setStyleDisabled(disabled) {
  getMarkupDocumentViewer().authorStyleDisabled = disabled;
}
/* End of the Page Style functions */

var BrowserOffline = {
  /////////////////////////////////////////////////////////////////////////////
  // BrowserOffline Public Methods
  init: function ()
  {
    if (!this._uiElement)
      this._uiElement = document.getElementById("workOfflineMenuitemState");

    Services.obs.addObserver(this, "network:offline-status-changed", false);

    this._updateOfflineUI(Services.io.offline);
  },

  uninit: function ()
  {
    try {
      Services.obs.removeObserver(this, "network:offline-status-changed");
    } catch (ex) {
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

    // Save the current state for later use as the initial state
    // (if there is no netLinkService)
    gPrefService.setBoolPref("browser.offline", ioService.offline);
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
    Services.obs.addObserver(this, "dom-storage-warn-quota-exceeded", false);
    Services.obs.addObserver(this, "offline-cache-update-completed", false);
  },

  uninit: function ()
  {
    Services.obs.removeObserver(this, "dom-storage-warn-quota-exceeded");
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
    for (var i = 0; i < browsers.length; ++i) {
      if (browsers[i].contentWindow == aContentWindow)
        return browsers[i];
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
    for (var i = 0; i < browsers.length; ++i) {
      uri = this._getManifestURI(browsers[i].contentWindow);
      if (uri && uri.equals(aCacheUpdate.manifestURI)) {
        return browsers[i];
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
    for (var i = 0; i < groups.length; i++) {
      var uri = Services.io.newURI(groups[i], null, null);
      if (uri.asciiHost == host) {
        var cache = cacheService.getActiveCache(groups[i]);
        usage += cache.usage;
      }
    }

    var storageManager = Cc["@mozilla.org/dom/storagemanager;1"].
                         getService(Ci.nsIDOMStorageManager);
    usage += storageManager.getUsage(host);

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
          for (var i = 0; i < notification.documents.length; i++) {
            OfflineApps.allowSite(notification.documents[i]);
          }
        }
      },{
        label: gNavigatorBundle.getString("offlineApps.never"),
        accessKey: gNavigatorBundle.getString("offlineApps.neverAccessKey"),
        callback: function() {
          for (var i = 0; i < notification.documents.length; i++) {
            OfflineApps.disallowSite(notification.documents[i]);
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
    updateService.scheduleUpdate(manifestURI, aDocument.documentURIObject);
  },

  /////////////////////////////////////////////////////////////////////////////
  // nsIObserver
  observe: function (aSubject, aTopic, aState)
  {
    if (aTopic == "dom-storage-warn-quota-exceeded") {
      if (aSubject) {
        var uri = makeURI(aSubject.location.href);

        if (OfflineApps._checkUsage(uri)) {
          var browserWindow =
            this._getBrowserWindowForContentWindow(aSubject);
          var browser = this._getBrowserForContentWindow(browserWindow,
                                                         aSubject);
          OfflineApps._warnUsage(browser, uri);
        }
      }
    } else if (aTopic == "offline-cache-update-completed") {
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

  _notificationIcon: "indexedDB-notification-icon",

  init:
  function IndexedDBPromptHelper_init() {
    Services.obs.addObserver(this, this._permissionsPrompt, false);
    Services.obs.addObserver(this, this._quotaPrompt, false);
  },

  uninit:
  function IndexedDBPromptHelper_uninit() {
    Services.obs.removeObserver(this, this._permissionsPrompt, false);
    Services.obs.removeObserver(this, this._quotaPrompt, false);
  },

  observe:
  function IndexedDBPromptHelper_observe(subject, topic, data) {
    if (topic != this._permissionsPrompt &&
        topic != this._quotaPrompt) {
      throw new Error("Unexpected topic!");
    }

    var requestor = subject.QueryInterface(Ci.nsIInterfaceRequestor);

    var contentWindow = requestor.getInterface(Ci.nsIDOMWindow);
    var contentDocument = contentWindow.document;
    var browserWindow =
      OfflineApps._getBrowserWindowForContentWindow(contentWindow);
    var browser =
      OfflineApps._getBrowserForContentWindow(browserWindow, contentWindow);

    if (!browser) {
      // Must belong to some other window.
      return;
    }

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

    var self = this;
    var observer = requestor.getInterface(Ci.nsIObserver);

    var mainAction = {
      label: gNavigatorBundle.getString("offlineApps.allow"),
      accessKey: gNavigatorBundle.getString("offlineApps.allowAccessKey"),
      callback: function() {
        observer.observe(null, responseTopic,
                         Ci.nsIPermissionManager.ALLOW_ACTION);
      }
    };

    var secondaryActions = [
      {
        label: gNavigatorBundle.getString("offlineApps.never"),
        accessKey: gNavigatorBundle.getString("offlineApps.neverAccessKey"),
        callback: function() {
          observer.observe(null, responseTopic,
                           Ci.nsIPermissionManager.DENY_ACTION);
        }
      }
    ];

    PopupNotifications.show(browser, topic, message, this._notificationIcon,
                            mainAction, secondaryActions);

  }
};

function WindowIsClosing()
{
  var reallyClose = closeWindow(false, warnAboutClosingWindow);
  if (!reallyClose)
    return false;

  var numBrowsers = gBrowser.browsers.length;
  for (let i = 0; reallyClose && i < numBrowsers; ++i) {
    let ds = gBrowser.browsers[i].docShell;

    if (ds.contentViewer && !ds.contentViewer.permitUnload())
      reallyClose = false;
  }

  return reallyClose;
}

/**
 * Checks if this is the last full *browser* window around. If it is, this will
 * be communicated like quitting. Otherwise, we warn about closing multiple tabs.
 * @returns true if closing can proceed, false if it got cancelled.
 */
function warnAboutClosingWindow() {
  // Popups aren't considered full browser windows.
  if (!toolbar.visible)
    return gBrowser.warnAboutClosingTabs(true);

  // Figure out if there's at least one other browser window around.
  let e = Services.wm.getEnumerator("navigator:browser");
  while (e.hasMoreElements()) {
    let win = e.getNext();
    if (win != window && win.toolbar.visible)
      return gBrowser.warnAboutClosingTabs(true);
  }

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
  switchToTabHavingURI("about:addons", true, function(browser) {
    if (aView)
      browser.contentWindow.wrappedJSObject.loadView(aView);
  });
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

  var el, type;
  var formData = [];

  function escapeNameValuePair(aName, aValue, aIsFormUrlEncoded) {
    if (aIsFormUrlEncoded)
      return escape(aName + "=" + aValue);
    else
      return escape(aName) + "=" + escape(aValue);
  }

  for (var i=0; i < node.form.elements.length; i++) {
    el = node.form.elements[i];

    if (!el.type) // happens with fieldsets
      continue;

    if (el == node) {
      formData.push((isURLEncoded) ? escapeNameValuePair(el.name, "%s", true) :
                                     // Don't escape "%s", just append
                                     escapeNameValuePair(el.name, "", false) + "%s");
      continue;
    }

    type = el.type.toLowerCase();

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

  PlacesUIUtils.showMinimalAddBookmarkUI(makeURI(spec), title, description, null,
                                         null, null, "", postData, charset);
}

function SwitchDocumentDirection(aWindow) {
  aWindow.document.dir = (aWindow.document.dir == "ltr" ? "rtl" : "ltr");
  for (var run = 0; run < aWindow.frames.length; run++)
    SwitchDocumentDirection(aWindow.frames[run]);
}

function getPluginInfo(pluginElement)
{
  var tagMimetype;
  var pluginsPage;
  if (pluginElement instanceof HTMLAppletElement) {
    tagMimetype = "application/x-java-vm";
  } else {
    if (pluginElement instanceof HTMLObjectElement) {
      pluginsPage = pluginElement.getAttribute("codebase");
    } else {
      pluginsPage = pluginElement.getAttribute("pluginspage");
    }

    // only attempt if a pluginsPage is defined.
    if (pluginsPage) {
      var doc = pluginElement.ownerDocument;
      var docShell = findChildShell(doc, gBrowser.docShell, null);
      try {
        pluginsPage = makeURI(pluginsPage, doc.characterSet, docShell.currentURI).spec;
      } catch (ex) {
        pluginsPage = "";
      }
    }

    tagMimetype = pluginElement.QueryInterface(Components.interfaces.nsIObjectLoadingContent)
                               .actualType;

    if (tagMimetype == "") {
      tagMimetype = pluginElement.type;
    }
  }

  return {mimetype: tagMimetype, pluginsPage: pluginsPage};
}

var gPluginHandler = {

  get CrashSubmit() {
    delete this.CrashSubmit;
    Cu.import("resource://gre/modules/CrashSubmit.jsm", this);
    return this.CrashSubmit;
  },

  get crashReportHelpURL() {
    delete this.crashReportHelpURL;
    let url = formatURL("app.support.baseURL", true);
    url += "plugin-crashed";
    this.crashReportHelpURL = url;
    return this.crashReportHelpURL;
  },

  // Map the plugin's name to a filtered version more suitable for user UI.
  makeNicePluginName : function (aName, aFilename) {
    if (aName == "Shockwave Flash")
      return "Adobe Flash";

    // Clean up the plugin name by stripping off any trailing version numbers
    // or "plugin". EG, "Foo Bar Plugin 1.23_02" --> "Foo Bar"
    let newName = aName.replace(/\bplug-?in\b/i, "").replace(/[\s\d\.\-\_\(\)]+$/, "");
    return newName;
  },

  isTooSmall : function (plugin, overlay) {
    // Is the <object>'s size too small to hold what we want to show?
    let pluginRect = plugin.getBoundingClientRect();
    // XXX bug 446693. The text-shadow on the submitted-report text at
    //     the bottom causes scrollHeight to be larger than it should be.
    let overflows = (overlay.scrollWidth > pluginRect.width) ||
                    (overlay.scrollHeight - 5 > pluginRect.height);
    return overflows;
  },

  addLinkClickCallback: function (linkNode, callbackName /*callbackArgs...*/) {
    // XXX just doing (callback)(arg) was giving a same-origin error. bug?
    let self = this;
    let callbackArgs = Array.prototype.slice.call(arguments).slice(2);
    linkNode.addEventListener("click",
                              function(evt) {
                                if (!evt.isTrusted)
                                  return;
                                evt.preventDefault();
                                if (callbackArgs.length == 0)
                                  callbackArgs = [ evt ];
                                (self[callbackName]).apply(self, callbackArgs);
                              },
                              true);

    linkNode.addEventListener("keydown",
                              function(evt) {
                                if (!evt.isTrusted)
                                  return;
                                if (evt.keyCode == evt.DOM_VK_RETURN) {
                                  evt.preventDefault();
                                  if (callbackArgs.length == 0)
                                    callbackArgs = [ evt ];
                                  evt.preventDefault();
                                  (self[callbackName]).apply(self, callbackArgs);
                                }
                              },
                              true);
  },

  handleEvent : function(event) {
    let self = gPluginHandler;
    let plugin = event.target;

    // We're expecting the target to be a plugin.
    if (!(plugin instanceof Ci.nsIObjectLoadingContent))
      return;

    switch (event.type) {
      case "PluginCrashed":
        self.pluginInstanceCrashed(plugin, event);
        break;

      case "PluginNotFound":
        // For non-object plugin tags, register a click handler to install the
        // plugin. Object tags can, and often do, deal with that themselves,
        // so don't stomp on the page developers toes.
        if (!(plugin instanceof HTMLObjectElement))
          self.addLinkClickCallback(plugin, "installSinglePlugin");
        /* FALLTHRU */
      case "PluginBlocklisted":
      case "PluginOutdated":
        let hideBarPrefName = event.type == "PluginOutdated" ?
                                "plugins.hide_infobar_for_outdated_plugin" :
                                "plugins.hide_infobar_for_missing_plugin";
        if (gPrefService.getBoolPref(hideBarPrefName))
          return;

        self.pluginUnavailable(plugin, event.type);
        break;

      case "PluginDisabled":
        self.addLinkClickCallback(plugin, "managePlugins");
        break;
    }
  },

  newPluginInstalled : function(event) {
    // browser elements are anonymous so we can't just use target.
    var browser = event.originalTarget;
    // clear the plugin list, now that at least one plugin has been installed
    browser.missingPlugins = null;

    var notificationBox = gBrowser.getNotificationBox(browser);
    var notification = notificationBox.getNotificationWithValue("missing-plugins");
    if (notification)
      notificationBox.removeNotification(notification);

    // reload the browser to make the new plugin show.
    browser.reload();
  },

  // Callback for user clicking on a missing (unsupported) plugin.
  installSinglePlugin: function (aEvent) {
    var missingPluginsArray = {};

    var pluginInfo = getPluginInfo(aEvent.target);
    missingPluginsArray[pluginInfo.mimetype] = pluginInfo;

    openDialog("chrome://mozapps/content/plugins/pluginInstallerWizard.xul",
               "PFSWindow", "chrome,centerscreen,resizable=yes",
               {plugins: missingPluginsArray, browser: gBrowser.selectedBrowser});
  },

  // Callback for user clicking on a disabled plugin
  managePlugins: function (aEvent) {
    BrowserOpenAddonsMgr("addons://list/plugin");
  },

  // Callback for user clicking "submit a report" link
  submitReport : function(pluginDumpID, browserDumpID) {
    // The crash reporter wants a DOM element it can append an IFRAME to,
    // which it uses to submit a form. Let's just give it gBrowser.
    this.CrashSubmit.submit(pluginDumpID, gBrowser, null, null);
    if (browserDumpID)
      this.CrashSubmit.submit(browserDumpID, gBrowser, null, null);
  },

  // Callback for user clicking a "reload page" link
  reloadPage: function (browser) {
    browser.reload();
  },

  // Callback for user clicking the help icon
  openHelpPage: function () {
    openHelpLink("plugin-crashed", false);
  },


  // event listener for missing/blocklisted/outdated plugins.
  pluginUnavailable: function (plugin, eventType) {
    let browser = gBrowser.getBrowserForDocument(plugin.ownerDocument
                                                       .defaultView.top.document);
    if (!browser.missingPlugins)
      browser.missingPlugins = {};

    var pluginInfo = getPluginInfo(plugin);
    browser.missingPlugins[pluginInfo.mimetype] = pluginInfo;

    var notificationBox = gBrowser.getNotificationBox(browser);

    // Should only display one of these warnings per page.
    // In order of priority, they are: outdated > missing > blocklisted
    let outdatedNotification = notificationBox.getNotificationWithValue("outdated-plugins");
    let blockedNotification  = notificationBox.getNotificationWithValue("blocked-plugins");
    let missingNotification  = notificationBox.getNotificationWithValue("missing-plugins");

    // If there is already an outdated plugin notification then do nothing
    if (outdatedNotification)
      return;

    function showBlocklistInfo() {
      var url = formatURL("extensions.blocklist.detailsURL", true);
      gBrowser.loadOneTab(url, {inBackground: false});
      return true;
    }

    function showOutdatedPluginsInfo() {
      gPrefService.setBoolPref("plugins.update.notifyUser", false);
      var url = formatURL("plugins.update.url", true);
      gBrowser.loadOneTab(url, {inBackground: false});
      return true;
    }

    function showPluginsMissing() {
      // get the urls of missing plugins
      var missingPluginsArray = gBrowser.selectedBrowser.missingPlugins;
      if (missingPluginsArray) {
        openDialog("chrome://mozapps/content/plugins/pluginInstallerWizard.xul",
                   "PFSWindow", "chrome,centerscreen,resizable=yes",
                   {plugins: missingPluginsArray, browser: gBrowser.selectedBrowser});
      }
    }

    let notifications = {
      PluginBlocklisted : {
                            barID   : "blocked-plugins",
                            iconURL : "chrome://mozapps/skin/plugins/notifyPluginBlocked.png",
                            message : gNavigatorBundle.getString("blockedpluginsMessage.title"),
                            buttons : [{
                                         label     : gNavigatorBundle.getString("blockedpluginsMessage.infoButton.label"),
                                         accessKey : gNavigatorBundle.getString("blockedpluginsMessage.infoButton.accesskey"),
                                         popup     : null,
                                         callback  : showBlocklistInfo
                                       },
                                       {
                                         label     : gNavigatorBundle.getString("blockedpluginsMessage.searchButton.label"),
                                         accessKey : gNavigatorBundle.getString("blockedpluginsMessage.searchButton.accesskey"),
                                         popup     : null,
                                         callback  : showOutdatedPluginsInfo
                                      }],
                          },
      PluginOutdated    : {
                            barID   : "outdated-plugins",
                            iconURL : "chrome://mozapps/skin/plugins/notifyPluginOutdated.png",
                            message : gNavigatorBundle.getString("outdatedpluginsMessage.title"),
                            buttons : [{
                                         label     : gNavigatorBundle.getString("outdatedpluginsMessage.updateButton.label"),
                                         accessKey : gNavigatorBundle.getString("outdatedpluginsMessage.updateButton.accesskey"),
                                         popup     : null,
                                         callback  : showOutdatedPluginsInfo
                                      }],
                          },
      PluginNotFound    : {
                            barID   : "missing-plugins",
                            iconURL : "chrome://mozapps/skin/plugins/notifyPluginGeneric.png",
                            message : gNavigatorBundle.getString("missingpluginsMessage.title"),
                            buttons : [{
                                         label     : gNavigatorBundle.getString("missingpluginsMessage.button.label"),
                                         accessKey : gNavigatorBundle.getString("missingpluginsMessage.button.accesskey"),
                                         popup     : null,
                                         callback  : showPluginsMissing
                                      }],
                          }
    };

    if (eventType == "PluginBlocklisted") {
      if (blockedNotification || missingNotification)
        return;
    }
    else if (eventType == "PluginOutdated") {
      // Cancel any notification about blocklisting/missing plugins
      if (blockedNotification)
        blockedNotification.close();
      if (missingNotification)
        missingNotification.close();
    }
    else if (eventType == "PluginNotFound") {
      if (missingNotification)
        return;

      // Cancel any notification about blocklisting plugins
      if (blockedNotification)
        blockedNotification.close();
    }

    let notify = notifications[eventType];
    notificationBox.appendNotification(notify.message, notify.barID, notify.iconURL,
                                       notificationBox.PRIORITY_WARNING_MEDIUM,
                                       notify.buttons);
  },

  // Crashed-plugin observer. Notified once per plugin crash, before events
  // are dispatched to individual plugin instances.
  pluginCrashed : function(subject, topic, data) {
    let propertyBag = subject;
    if (!(propertyBag instanceof Ci.nsIPropertyBag2) ||
        !(propertyBag instanceof Ci.nsIWritablePropertyBag2))
     return;

#ifdef MOZ_CRASHREPORTER
    let pluginDumpID = propertyBag.getPropertyAsAString("pluginDumpID");
    let browserDumpID= propertyBag.getPropertyAsAString("browserDumpID");
    let shouldSubmit = gCrashReporter.submitReports;
    let doPrompt     = true; // XXX followup to get via gCrashReporter

    // Submit automatically when appropriate.
    if (pluginDumpID && shouldSubmit && !doPrompt) {
      this.submitReport(pluginDumpID, browserDumpID);
      // Submission is async, so we can't easily show failure UI.
      propertyBag.setPropertyAsBool("submittedCrashReport", true);
    }
#endif
  },

  // Crashed-plugin event listener. Called for every instance of a
  // plugin in content.
  pluginInstanceCrashed: function (plugin, aEvent) {
    // Ensure the plugin and event are of the right type.
    if (!(aEvent instanceof Ci.nsIDOMDataContainerEvent))
      return;

    let submittedReport = aEvent.getData("submittedCrashReport");
    let doPrompt        = true; // XXX followup for .getData("doPrompt");
    let submitReports   = true; // XXX followup for .getData("submitReports");
    let pluginName      = aEvent.getData("pluginName");
    let pluginFilename  = aEvent.getData("pluginFilename");
    let pluginDumpID    = aEvent.getData("pluginDumpID");
    let browserDumpID   = aEvent.getData("browserDumpID");

    // Remap the plugin name to a more user-presentable form.
    pluginName = this.makeNicePluginName(pluginName, pluginFilename);

    // Force a style flush, so that we ensure our binding is attached.
    plugin.clientTop;

    let messageString = gNavigatorBundle.getFormattedString("crashedpluginsMessage.title", [pluginName]);

    //
    // Configure the crashed-plugin placeholder.
    //
    let doc = plugin.ownerDocument;
    let overlay = doc.getAnonymousElementByAttribute(plugin, "class", "mainBox");

    // The binding has role="link" here, since missing/disabled/blocked
    // plugin UI has a onclick handler on the whole thing. This isn't needed
    // for the plugin-crashed UI, because we use actual HTML links in the text.
    overlay.removeAttribute("role");

    let statusDiv = doc.getAnonymousElementByAttribute(plugin, "class", "submitStatus");
#ifdef MOZ_CRASHREPORTER
    let status;

    // Determine which message to show regarding crash reports.
    if (submittedReport) { // submitReports && !doPrompt, handled in observer
      status = "submitted";
    }
    else if (!submitReports && !doPrompt) {
      status = "noSubmit";
    }
    else { // doPrompt
      status = "please";
      // XXX can we make the link target actually be blank?
      let pleaseLink = doc.getAnonymousElementByAttribute(
                            plugin, "class", "pleaseSubmitLink");
      this.addLinkClickCallback(pleaseLink, "submitReport",
                                pluginDumpID, browserDumpID);
    }

    // If we don't have a minidumpID, we can't (or didn't) submit anything.
    // This can happen if the plugin is killed from the task manager.
    if (!pluginDumpID) {
        status = "noReport";
    }

    statusDiv.setAttribute("status", status);

    let bottomLinks = doc.getAnonymousElementByAttribute(plugin, "class", "msg msgBottomLinks");
    bottomLinks.style.display = "block";
    let helpIcon = doc.getAnonymousElementByAttribute(plugin, "class", "helpIcon");
    this.addLinkClickCallback(helpIcon, "openHelpPage");

    // If we're showing the link to manually trigger report submission, we'll
    // want to be able to update all the instances of the UI for this crash to
    // show an updated message when a report is submitted.
    if (doPrompt) {
      let observer = {
        QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                               Ci.nsISupportsWeakReference]),
        observe : function(subject, topic, data) {
          let propertyBag = subject;
          if (!(propertyBag instanceof Ci.nsIPropertyBag2))
            return;
          // Ignore notifications for other crashes.
          if (propertyBag.get("minidumpID") != pluginDumpID)
            return;
          statusDiv.setAttribute("status", data);
        },

        handleEvent : function(event) {
            // Not expected to be called, just here for the closure.
        }
      }

      // Use a weak reference, so we don't have to remove it...
      Services.obs.addObserver(observer, "crash-report-status", true);
      // ...alas, now we need something to hold a strong reference to prevent
      // it from being GC. But I don't want to manually manage the reference's
      // lifetime (which should be no greater than the page).
      // Clever solution? Use a closue with an event listener on the document.
      // When the doc goes away, so do the listener references and the closure.
      doc.addEventListener("mozCleverClosureHack", observer, false);
    }
#endif

    let crashText = doc.getAnonymousElementByAttribute(plugin, "class", "msg msgCrashed");
    crashText.textContent = messageString;

    let browser = gBrowser.getBrowserForDocument(doc.defaultView.top.document);

    let link = doc.getAnonymousElementByAttribute(plugin, "class", "reloadLink");
    this.addLinkClickCallback(link, "reloadPage", browser);

    let notificationBox = gBrowser.getNotificationBox(browser);

    // Is the <object>'s size too small to hold what we want to show?
    if (this.isTooSmall(plugin, overlay)) {
        // Hide the overlay's contents. Use visibility style, so that it
        // doesn't collapse down to 0x0.
        overlay.style.visibility = "hidden";
        // If another plugin on the page was large enough to show our UI, we
        // don't want to show a notification bar.
        if (!doc.mozNoPluginCrashedNotification)
          showNotificationBar(pluginDumpID, browserDumpID);
    } else {
        // If a previous plugin on the page was too small and resulted in
        // adding a notification bar, then remove it because this plugin
        // instance it big enough to serve as in-content notification.
        hideNotificationBar();
        doc.mozNoPluginCrashedNotification = true;
    }

    function hideNotificationBar() {
      let notification = notificationBox.getNotificationWithValue("plugin-crashed");
      if (notification)
        notificationBox.removeNotification(notification, true);
    }

    function showNotificationBar(pluginDumpID, browserDumpID) {
      // If there's already an existing notification bar, don't do anything.
      let notification = notificationBox.getNotificationWithValue("plugin-crashed");
      if (notification)
        return;

      // Configure the notification bar
      let priority = notificationBox.PRIORITY_WARNING_MEDIUM;
      let iconURL = "chrome://mozapps/skin/plugins/notifyPluginCrashed.png";
      let reloadLabel = gNavigatorBundle.getString("crashedpluginsMessage.reloadButton.label");
      let reloadKey   = gNavigatorBundle.getString("crashedpluginsMessage.reloadButton.accesskey");
      let submitLabel = gNavigatorBundle.getString("crashedpluginsMessage.submitButton.label");
      let submitKey   = gNavigatorBundle.getString("crashedpluginsMessage.submitButton.accesskey");

      let buttons = [{
        label: reloadLabel,
        accessKey: reloadKey,
        popup: null,
        callback: function() { browser.reload(); },
      }];
#ifdef MOZ_CRASHREPORTER
      let submitButton = {
        label: submitLabel,
        accessKey: submitKey,
        popup: null,
          callback: function() { gPluginHandler.submitReport(pluginDumpID, browserDumpID); },
      };
      if (pluginDumpID)
        buttons.push(submitButton);
#endif

      let notification = notificationBox.appendNotification(messageString, "plugin-crashed",
                                                            iconURL, priority, buttons);

      // Add the "learn more" link.
      let XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
      let link = notification.ownerDocument.createElementNS(XULNS, "label");
      link.className = "text-link";
      link.setAttribute("value", gNavigatorBundle.getString("crashedpluginsMessage.learnMore"));
      link.href = gPluginHandler.crashReportHelpURL;
      let description = notification.ownerDocument.getAnonymousElementByAttribute(notification, "anonid", "messageText");
      description.appendChild(link);

      // Remove the notfication when the page is reloaded.
      doc.defaultView.top.addEventListener("unload", function() {
        notificationBox.removeNotification(notification);
      }, false);
    }

  }
};

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
 * The Feed Handler object manages discovery of RSS/ATOM feeds in web pages
 * and shows UI when they are discovered.
 */
var FeedHandler = {
  /**
   * Called when the user clicks on the Subscribe to This Page... menu item.
   * Builds a menu of unique feeds associated with the page, and if there
   * is only one, shows the feed inline in the browser window.
   * @param   menuPopup
   *          The feed list menupopup to be populated.
   * @returns true if the menu should be shown, false if there was only
   *          one feed and the feed should be shown inline in the browser
   *          window (do not show the menupopup).
   */
  buildFeedList: function(menuPopup) {
    var feeds = gBrowser.selectedBrowser.feeds;
    if (feeds == null) {
      // XXX hack -- menu opening depends on setting of an "open"
      // attribute, and the menu refuses to open if that attribute is
      // set (because it thinks it's already open).  onpopupshowing gets
      // called after the attribute is unset, and it doesn't get unset
      // if we return false.  so we unset it here; otherwise, the menu
      // refuses to work past this point.
      menuPopup.parentNode.removeAttribute("open");
      return false;
    }

    while (menuPopup.firstChild)
      menuPopup.removeChild(menuPopup.firstChild);

    // Build the menu showing the available feed choices for viewing.
    for (var i = 0; i < feeds.length; ++i) {
      var feedInfo = feeds[i];
      var menuItem = document.createElement("menuitem");
      var baseTitle = feedInfo.title || feedInfo.href;
      var labelStr = gNavigatorBundle.getFormattedString("feedShowFeedNew", [baseTitle]);
      menuItem.setAttribute("class", "feed-menuitem");
      menuItem.setAttribute("label", labelStr);
      menuItem.setAttribute("feed", feedInfo.href);
      menuItem.setAttribute("tooltiptext", feedInfo.href);
      menuItem.setAttribute("crop", "center");
      menuPopup.appendChild(menuItem);
    }
    return true;
  },

  /**
   * Subscribe to a given feed.  Called when
   *   1. Page has a single feed and user clicks feed icon in location bar
   *   2. Page has a single feed and user selects Subscribe menu item
   *   3. Page has multiple feeds and user selects from feed icon popup
   *   4. Page has multiple feeds and user selects from Subscribe submenu
   * @param   href
   *          The feed to subscribe to. May be null, in which case the
   *          event target's feed attribute is examined.
   * @param   event
   *          The event this method is handling. Used to decide where
   *          to open the preview UI. (Optional, unless href is null)
   */
  subscribeToFeed: function(href, event) {
    // Just load the feed in the content area to either subscribe or show the
    // preview UI
    if (!href)
      href = event.target.getAttribute("feed");
    urlSecurityCheck(href, gBrowser.contentPrincipal,
                     Ci.nsIScriptSecurityManager.DISALLOW_INHERIT_PRINCIPAL);
    var feedURI = makeURI(href, document.characterSet);
    // Use the feed scheme so X-Moz-Is-Feed will be set
    // The value doesn't matter
    if (/^https?/.test(feedURI.scheme))
      href = "feed:" + href;
    this.loadFeed(href, event);
  },

  loadFeed: function(href, event) {
    var feeds = gBrowser.selectedBrowser.feeds;
    try {
      openUILink(href, event, false, true, false, null);
    }
    finally {
      // We might default to a livebookmarks modal dialog,
      // so reset that if the user happens to click it again
      gBrowser.selectedBrowser.feeds = feeds;
    }
  },

  get _feedMenuitem() {
    delete this._feedMenuitem;
    return this._feedMenuitem = document.getElementById("singleFeedMenuitemState");
  },

  get _feedMenupopup() {
    delete this._feedMenupopup;
    return this._feedMenupopup = document.getElementById("multipleFeedsMenuState");
  },

  /**
   * Update the browser UI to show whether or not feeds are available when
   * a page is loaded or the user switches tabs to a page that has feeds.
   */
  updateFeeds: function() {
    var feeds = gBrowser.selectedBrowser.feeds;
    if (!feeds || feeds.length == 0) {
      this._feedMenuitem.setAttribute("disabled", "true");
      this._feedMenupopup.setAttribute("hidden", "true");
      this._feedMenuitem.removeAttribute("hidden");
    } else {
      if (feeds.length > 1) {
        this._feedMenuitem.setAttribute("hidden", "true");
        this._feedMenupopup.removeAttribute("hidden");
      } else {
        this._feedMenuitem.setAttribute("feed", feeds[0].href);
        this._feedMenuitem.removeAttribute("disabled");
        this._feedMenuitem.removeAttribute("hidden");
        this._feedMenupopup.setAttribute("hidden", "true");
      }
    }
  },

  addFeed: function(link, targetDoc) {
    // find which tab this is for, and set the attribute on the browser
    var browserForLink = gBrowser.getBrowserForDocument(targetDoc);
    if (!browserForLink) {
      // ignore feeds loaded in subframes (see bug 305472)
      return;
    }

    if (!browserForLink.feeds)
      browserForLink.feeds = [];

    browserForLink.feeds.push({ href: link.href, title: link.title });
  }
};

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
    tab = ss.undoCloseTab(window, aIndex || 0);

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
  let browser = aTab.linkedBrowser;
  return browser.sessionHistory.count < 2 &&
         browser.currentURI.spec == "about:blank" &&
         !browser.contentDocument.body.hasChildNodes() &&
         !aTab.hasAttribute("busy");
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
 * This also takes care of updating the command enabled-state when tabs are
 * created or removed.
 */
var gBookmarkAllTabsHandler = {
  init: function () {
    this._command = document.getElementById("Browser:BookmarkAllTabs");
    gBrowser.tabContainer.addEventListener("TabOpen", this, true);
    gBrowser.tabContainer.addEventListener("TabClose", this, true);
    gBrowser.tabContainer.addEventListener("TabShow", this, true);
    gBrowser.tabContainer.addEventListener("TabHide", this, true);
    this._updateCommandState();
  },

  _updateCommandState: function BATH__updateCommandState() {
    let remainingTabs = gBrowser.visibleTabs.filter(function(tab) {
      return gBrowser._removingTabs.indexOf(tab) == -1;
    });

    if (remainingTabs.length > 1)
      this._command.removeAttribute("disabled");
    else
      this._command.setAttribute("disabled", "true");
  },

  doCommand: function BATH_doCommand() {
    PlacesCommandHook.bookmarkCurrentPages();
  },

  // nsIDOMEventListener
  handleEvent: function(aEvent) {
    this._updateCommandState();
  }
};

/**
 * Utility object to handle manipulations of the identity indicators in the UI
 */
var gIdentityHandler = {
  // Mode strings used to control CSS display
  IDENTITY_MODE_IDENTIFIED       : "verifiedIdentity", // High-quality identity information
  IDENTITY_MODE_DOMAIN_VERIFIED  : "verifiedDomain",   // Minimal SSL CA-signed domain verification
  IDENTITY_MODE_UNKNOWN          : "unknownIdentity",  // No trusted identity information
  IDENTITY_MODE_MIXED_CONTENT    : "unknownIdentity mixedContent",  // SSL with unauthenticated content

  // Cache the most recent SSLStatus and Location seen in checkIdentity
  _lastStatus : null,
  _lastLocation : null,

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

  /**
   * Rebuild cache of the elements that may or may not exist depending
   * on whether there's a location bar.
   */
  _cacheElements : function() {
    delete this._identityBox;
    delete this._identityIconLabel;
    delete this._identityIconCountryLabel;
    this._identityBox = document.getElementById("identity-box");
    this._identityIconLabel = document.getElementById("identity-icon-label");
    this._identityIconCountryLabel = document.getElementById("identity-icon-country-label");
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
    if (state & nsIWebProgressListener.STATE_IDENTITY_EV_TOPLEVEL)
      this.setMode(this.IDENTITY_MODE_IDENTIFIED);
    else if (state & nsIWebProgressListener.STATE_SECURE_HIGH)
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
  },

  /**
   * Set up the messages for the primary identity UI based on the specified mode,
   * and the details of the SSL cert, where applicable
   *
   * @param newMode The newly set identity mode.  Should be one of the IDENTITY_MODE_* constants.
   */
  setIdentityMessages : function(newMode) {
    if (newMode == this.IDENTITY_MODE_DOMAIN_VERIFIED) {
      var iData = this.getIdentityData();

      // It would be sort of nice to use the CN= field in the cert, since that's
      // typically what we want here, but thanks to x509 certs being extensible,
      // it's not the only place you have to check, there can be more than one domain,
      // et cetera, ad nauseum.  We know the cert is valid for location.host, so
      // let's just use that. Check the pref to determine how much of the verified
      // hostname to show
      var icon_label = "";
      var icon_country_label = "";
      var icon_labels_dir = "ltr";
      switch (gPrefService.getIntPref("browser.identity.ssl_domain_display")) {
        case 2 : // Show full domain
          icon_label = this._lastLocation.hostname;
          break;
        case 1 : // Show eTLD.
          icon_label = this.getEffectiveHost();
      }

      // We need a port number for all lookups.  If one hasn't been specified, use
      // the https default
      var lookupHost = this._lastLocation.host;
      if (lookupHost.indexOf(':') < 0)
        lookupHost += ":443";

      // Verifier is either the CA Org, for a normal cert, or a special string
      // for certs that are trusted because of a security exception.
      var tooltip = gNavigatorBundle.getFormattedString("identity.identified.verifier",
                                                        [iData.caOrg]);

      // Check whether this site is a security exception. XPConnect does the right
      // thing here in terms of converting _lastLocation.port from string to int, but
      // the overrideService doesn't like undefined ports, so make sure we have
      // something in the default case (bug 432241).
      if (this._overrideService.hasMatchingOverride(this._lastLocation.hostname,
                                                    (this._lastLocation.port || 443),
                                                    iData.cert, {}, {}))
        tooltip = gNavigatorBundle.getString("identity.identified.verified_by_you");
    }
    else if (newMode == this.IDENTITY_MODE_IDENTIFIED) {
      // If it's identified, then we can populate the dialog with credentials
      iData = this.getIdentityData();
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
    }
    else {
      tooltip = gNavigatorBundle.getString("identity.unknown.tooltip");
      icon_label = "";
      icon_country_label = "";
      icon_labels_dir = "ltr";
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
    var supplemental = "";
    var verifier = "";

    if (newMode == this.IDENTITY_MODE_DOMAIN_VERIFIED) {
      var iData = this.getIdentityData();
      var host = this.getEffectiveHost();
      var owner = gNavigatorBundle.getString("identity.ownerUnknown2");
      verifier = this._identityBox.tooltipText;
      supplemental = "";
    }
    else if (newMode == this.IDENTITY_MODE_IDENTIFIED) {
      // If it's identified, then we can populate the dialog with credentials
      iData = this.getIdentityData();
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
    }
    else {
      // These strings will be hidden in CSS anyhow
      host = "";
      owner = "";
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

  /**
   * Click handler for the identity-box element in primary chrome.
   */
  handleIdentityButtonEvent : function(event) {

    event.stopPropagation();

    if ((event.type == "click" && event.button != 0) ||
        (event.type == "keypress" && event.charCode != KeyEvent.DOM_VK_SPACE &&
         event.keyCode != KeyEvent.DOM_VK_RETURN))
      return; // Left click, space or enter only

    // Revert the contents of the location bar, see bug 406779
    gURLBar.handleRevert();

    // Make sure that the display:none style we set in xul is removed now that
    // the popup is actually needed
    this._identityPopup.hidden = false;

    // Tell the popup to consume dismiss clicks, to avoid bug 395314
    this._identityPopup.popupBoxObject
        .setConsumeRollupEvent(Ci.nsIPopupBoxObject.ROLLUP_CONSUME);

    // Update the popup strings
    this.setPopupMessages(this._identityBox.className);

    // Make sure the identity popup hangs toward the middle of the location bar
    // in RTL builds
    var position = (getComputedStyle(gNavToolbox, "").direction == "rtl") ? 'after_end' : 'after_start';

    // Add the "open" attribute to the identity box for styling
    this._identityBox.setAttribute("open", "true");
    var self = this;
    this._identityPopup.addEventListener("popuphidden", function (e) {
      e.currentTarget.removeEventListener("popuphidden", arguments.callee, false);
      self._identityBox.removeAttribute("open");
    }, false);

    // Now open the popup, anchored off the primary chrome element
    this._identityPopup.openPopup(this._identityBox, position);
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
    dt.setDragImage(event.currentTarget, 0, 0);
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
      let dl = dls.getNext().QueryInterface(Ci.nsIDownload);
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

/* DEPRECATED */
function getBrowser() gBrowser;
function getNavToolbox() gNavToolbox;

let gPrivateBrowsingUI = {
  _privateBrowsingService: null,
  _searchBarValue: null,
  _findBarValue: null,

  init: function PBUI_init() {
    Services.obs.addObserver(this, "private-browsing", false);
    Services.obs.addObserver(this, "private-browsing-transition-complete", false);

    this._privateBrowsingService = Cc["@mozilla.org/privatebrowsing;1"].
                                   getService(Ci.nsIPrivateBrowsingService);

    if (this.privateBrowsingEnabled)
      this.onEnterPrivateBrowsing(true);
  },

  uninit: function PBUI_unint() {
    Services.obs.removeObserver(this, "private-browsing");
    Services.obs.removeObserver(this, "private-browsing-transition-complete");
  },

  get _disableUIOnToggle() {
    if (this._privateBrowsingService.autoStarted)
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
        // use setTimeout here in order to make the code testable
        setTimeout(function() {
          document.getElementById("Tools:PrivateBrowsing")
                  .removeAttribute("disabled");
        }, 0);
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

    document.getElementById("menu_import").setAttribute("disabled", "true");

    // Disable the Clear Recent History... menu item when in PB mode
    // temporary fix until bug 463607 is fixed
    document.getElementById("Tools:Sanitize").setAttribute("disabled", "true");

    if (this._privateBrowsingService.autoStarted) {
      // Disable the menu item in auto-start mode
      document.getElementById("privateBrowsingItem")
              .setAttribute("disabled", "true");
#ifdef MENUBAR_CAN_AUTOHIDE
      document.getElementById("appmenu_privateBrowsing")
              .setAttribute("disabled", "true");
#endif
      document.getElementById("Tools:PrivateBrowsing")
              .setAttribute("disabled", "true");
    }
    else if (window.location.href == getBrowserURL()) {
      // Adjust the window's title
      let docElement = document.documentElement;
      docElement.setAttribute("title",
        docElement.getAttribute("title_privatebrowsing"));
      docElement.setAttribute("titlemodifier",
        docElement.getAttribute("titlemodifier_privatebrowsing"));
      docElement.setAttribute("browsingmode", "private");
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

    document.getElementById("menu_import").removeAttribute("disabled");

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
      docElement.setAttribute("browsingmode", "normal");
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

var LightWeightThemeWebInstaller = {
  handleEvent: function (event) {
    switch (event.type) {
      case "InstallBrowserTheme":
      case "PreviewBrowserTheme":
      case "ResetBrowserThemePreview":
        // ignore requests from background tabs
        if (event.target.ownerDocument.defaultView.top != content)
          return;
    }
    switch (event.type) {
      case "InstallBrowserTheme":
        this._installRequest(event);
        break;
      case "PreviewBrowserTheme":
        this._preview(event);
        break;
      case "ResetBrowserThemePreview":
        this._resetPreview(event);
        break;
      case "pagehide":
      case "TabSelect":
        this._resetPreview();
        break;
    }
  },

  get _manager () {
    var temp = {};
    Cu.import("resource://gre/modules/LightweightThemeManager.jsm", temp);
    delete this._manager;
    return this._manager = temp.LightweightThemeManager;
  },

  _installRequest: function (event) {
    var node = event.target;
    var data = this._getThemeFromNode(node);
    if (!data)
      return;

    if (this._isAllowed(node)) {
      this._install(data);
      return;
    }

    var allowButtonText =
      gNavigatorBundle.getString("lwthemeInstallRequest.allowButton");
    var allowButtonAccesskey =
      gNavigatorBundle.getString("lwthemeInstallRequest.allowButton.accesskey");
    var message =
      gNavigatorBundle.getFormattedString("lwthemeInstallRequest.message",
                                          [node.ownerDocument.location.host]);
    var buttons = [{
      label: allowButtonText,
      accessKey: allowButtonAccesskey,
      callback: function () {
        LightWeightThemeWebInstaller._install(data);
      }
    }];

    this._removePreviousNotifications();

    var notificationBox = gBrowser.getNotificationBox();
    var notificationBar =
      notificationBox.appendNotification(message, "lwtheme-install-request", "",
                                         notificationBox.PRIORITY_INFO_MEDIUM,
                                         buttons);
    notificationBar.persistence = 1;
  },

  _install: function (newLWTheme) {
    var previousLWTheme = this._manager.currentTheme;

    var listener = {
      onEnabling: function(aAddon, aRequiresRestart) {
        if (!aRequiresRestart)
          return;

        let messageString = gNavigatorBundle.getFormattedString("lwthemeNeedsRestart.message",
          [aAddon.name], 1);

        let action = {
          label: gNavigatorBundle.getString("lwthemeNeedsRestart.button"),
          accessKey: gNavigatorBundle.getString("lwthemeNeedsRestart.accesskey"),
          callback: function () {
            Application.restart();
          }
        };

        let options = {
          timeout: Date.now() + 30000
        };

        PopupNotifications.show(gBrowser.selectedBrowser, "addon-theme-change",
                                messageString, "addons-notification-icon",
                                action, null, options);
      },

      onEnabled: function(aAddon) {
        LightWeightThemeWebInstaller._postInstallNotification(newLWTheme, previousLWTheme);
      }
    };

    AddonManager.addAddonListener(listener);
    this._manager.currentTheme = newLWTheme;
    AddonManager.removeAddonListener(listener);
  },

  _postInstallNotification: function (newTheme, previousTheme) {
    function text(id) {
      return gNavigatorBundle.getString("lwthemePostInstallNotification." + id);
    }

    var buttons = [{
      label: text("undoButton"),
      accessKey: text("undoButton.accesskey"),
      callback: function () {
        LightWeightThemeWebInstaller._manager.forgetUsedTheme(newTheme.id);
        LightWeightThemeWebInstaller._manager.currentTheme = previousTheme;
      }
    }, {
      label: text("manageButton"),
      accessKey: text("manageButton.accesskey"),
      callback: function () {
        BrowserOpenAddonsMgr("addons://list/theme");
      }
    }];

    this._removePreviousNotifications();

    var notificationBox = gBrowser.getNotificationBox();
    var notificationBar =
      notificationBox.appendNotification(text("message"),
                                         "lwtheme-install-notification", "",
                                         notificationBox.PRIORITY_INFO_MEDIUM,
                                         buttons);
    notificationBar.persistence = 1;
    notificationBar.timeout = Date.now() + 20000; // 20 seconds
  },

  _removePreviousNotifications: function () {
    var box = gBrowser.getNotificationBox();

    ["lwtheme-install-request",
     "lwtheme-install-notification"].forEach(function (value) {
        var notification = box.getNotificationWithValue(value);
        if (notification)
          box.removeNotification(notification);
      });
  },

  _previewWindow: null,
  _preview: function (event) {
    if (!this._isAllowed(event.target))
      return;

    var data = this._getThemeFromNode(event.target);
    if (!data)
      return;

    this._resetPreview();

    this._previewWindow = event.target.ownerDocument.defaultView;
    this._previewWindow.addEventListener("pagehide", this, true);
    gBrowser.tabContainer.addEventListener("TabSelect", this, false);

    this._manager.previewTheme(data);
  },

  _resetPreview: function (event) {
    if (!this._previewWindow ||
        event && !this._isAllowed(event.target))
      return;

    this._previewWindow.removeEventListener("pagehide", this, true);
    this._previewWindow = null;
    gBrowser.tabContainer.removeEventListener("TabSelect", this, false);

    this._manager.resetPreview();
  },

  _isAllowed: function (node) {
    var pm = Services.perms;

    var uri = node.ownerDocument.documentURIObject;
    return pm.testPermission(uri, "install") == pm.ALLOW_ACTION;
  },

  _getThemeFromNode: function (node) {
    return this._manager.parseTheme(node.getAttribute("data-browsertheme"),
                                    node.baseURI);
  }
}

/**
 * Switch to a tab that has a given URI, and focusses its browser window.
 * If a matching tab is in this window, it will be switched to. Otherwise, other
 * windows will be searched.
 *
 * @param aURI
 *        URI to search for
 * @param aOpenNew
 *        True to open a new tab and switch to it, if no existing tab is found
 * @param A callback to call when the tab is open, the tab's browser will be
 *        passed as an argument
 * @return True if a tab was switched to (or opened), false otherwise
 */
function switchToTabHavingURI(aURI, aOpenNew, aCallback) {
  function switchIfURIInWindow(aWindow) {
    if (!("gBrowser" in aWindow))
      return false;
    let browsers = aWindow.gBrowser.browsers;
    for (let i = 0; i < browsers.length; i++) {
      let browser = browsers[i];
      if (browser.currentURI.equals(aURI)) {
        gURLBar.handleRevert();
        // We need the current tab so we can check if we should close it
        let prevTab = gBrowser.selectedTab;
        // Focus the matching window & tab
        aWindow.focus();
        aWindow.gBrowser.tabContainer.selectedIndex = i;
        if (aCallback)
          aCallback(browser);
        // Close the previously selected tab if it was empty
        if (isTabEmpty(prevTab))
          gBrowser.removeTab(prevTab);
        return true;
      }
    }
    return false;
  }

  // This can be passed either nsIURI or a string.
  if (!(aURI instanceof Ci.nsIURI))
    aURI = makeURI(aURI);

  // Prioritise this window.
  if (switchIfURIInWindow(window))
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
    if (isTabEmpty(gBrowser.selectedTab))
      gBrowser.selectedBrowser.loadURI(aURI.spec);
    else
      gBrowser.selectedTab = gBrowser.addTab(aURI.spec);
    if (aCallback) {
      let browser = gBrowser.selectedBrowser;
      browser.addEventListener("pageshow", function(event) {
        if (event.target.location.href != aURI.spec)
          return;
        browser.removeEventListener("pageshow", arguments.callee, true);
        aCallback(browser);
      }, true);
    }
    return true;
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
    this.contextTab = document.popupNode.localName == "tab" ?
                      document.popupNode : gBrowser.selectedTab;
    let disabled = gBrowser.visibleTabs.length == 1;

    // Enable the "Close Tab" menuitem when the window doesn't close with the last tab.
    document.getElementById("context_closeTab").disabled =
      disabled && gBrowser.tabContainer._closeWindowWithLastTab;

    var menuItems = aPopupMenu.getElementsByAttribute("tbattr", "tabbrowser-multiple");
    for (var i = 0; i < menuItems.length; i++)
      menuItems[i].disabled = disabled;

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
  }
};

XPCOMUtils.defineLazyGetter(this, "HUDConsoleUI", function () {
  Cu.import("resource://gre/modules/HUDService.jsm");
  try {
    return HUDService.consoleUI;
  }
  catch (ex) {
    Components.utils.reportError(ex);
  }
});

// Prompt user to restart the browser in safe mode 
function safeModeRestart()
{
  // prompt the user to confirm 
  let promptTitle = gNavigatorBundle.getString("safeModeRestartPromptTitle");
  let promptMessage = 
    gNavigatorBundle.getString("safeModeRestartPromptMessage");
  let rv = Services.prompt.confirm(window, promptTitle, promptMessage);
  if (rv) {
    let environment = Components.classes["@mozilla.org/process/environment;1"].
      getService(Components.interfaces.nsIEnvironment);
    environment.set("MOZ_SAFE_MODE_RESTART", "1");
    Application.restart();
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
 * historyIndex is an index the page can navigate to after the new tab is
 * created and loaded, it can for example be used to go back one page after the
 * tab is duplicated.
 */
function duplicateTabIn(aTab, where, historyIndex) {
  let newTab = gBrowser.duplicateTab(aTab);

  // Go to index if it's provided, fallback to loadURI if there's no history.
  if (historyIndex != null) {
    try {
      gBrowser.getBrowserForTab(newTab).gotoIndex(historyIndex);
    }
    catch (ex) {
      let sessionHistory = aTab.linkedBrowser.sessionHistory;
      let entry = sessionHistory.getEntryAtIndex(historyIndex, false);
      let fallbackUrl = entry.URI.spec;
      gBrowser.getBrowserForTab(newTab).loadURI(fallbackUrl);
    }
  }

  var loadInBackground =
    getBoolPref("browser.tabs.loadBookmarksInBackground", false);

  switch (where) {
    case "window":
      gBrowser.hideTab(newTab);
      gBrowser.replaceTabWithWindow(newTab);
      break;
    case "tabshifted":
      loadInBackground = !loadInBackground;
      // fall through
    case "tab":
      if (!loadInBackground)
        gBrowser.selectedTab = newTab;
      break;
  }
}
