/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const kCountBeforeWeRemember = 5;

const kEntities = {
  "geolocation": "geolocation2",
  "desktop-notification": "desktopNotification",
  "indexedDB": "offlineApps",
  "indexedDBQuota": "indexedDBQuota"
};

const kIcons = {
  geolocation: "chrome://browser/skin/images/infobar-geolocation.png"
};

function ContentPermissionPrompt() {}

ContentPermissionPrompt.prototype = {
  classID: Components.ID("{C6E8C44D-9F39-4AF7-BCC0-76E38A8310F5}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentPermissionPrompt]),

  getChromeWindow: function getChromeWindow(aWindow) {
     let chromeWin = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIWebNavigation)
                            .QueryInterface(Ci.nsIDocShellTreeItem)
                            .rootTreeItem
                            .QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIDOMWindow)
                            .QueryInterface(Ci.nsIDOMChromeWindow);
     return chromeWin;
  },

  getNotificationBoxForRequest: function getNotificationBoxForRequest(request) {
    let notificationBox = null;
    if (request.window) {
      let requestingWindow = request.window.top;
      let chromeWin = this.getChromeWindow(requestingWindow).wrappedJSObject;
      let windowID = request.window.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID;
      let browser = chromeWin.Browser.getBrowserForWindowId(windowID);
      return chromeWin.getNotificationBox(browser);
    }

    let chromeWin = request.element.ownerDocument.defaultView;
    return chromeWin.Browser.getNotificationBox(request.element);
  },

  handleExistingPermission: function handleExistingPermission(request) {
    let result = Services.perms.testExactPermissionFromPrincipal(request.principal, request.type);
    if (result == Ci.nsIPermissionManager.ALLOW_ACTION) {
      request.allow();
      return true;
    }
    if (result == Ci.nsIPermissionManager.DENY_ACTION) {
      request.cancel();
      return true;
    }
    return false;
  },

  prompt: function(request) {
    // returns true if the request was handled
    if (this.handleExistingPermission(request))
       return;

    let pm = Services.perms;
    let notificationBox = this.getNotificationBoxForRequest(request);
    let browserBundle = Services.strings.createBundle("chrome://browser/locale/browser.properties");
    
    let notification = notificationBox.getNotificationWithValue(request.type);
    if (notification)
      return;

    let entityName = kEntities[request.type];
    let icon = kIcons[request.type] || "";

    let buttons = [{
      label: browserBundle.GetStringFromName(entityName + ".allow"),
      accessKey: "",
      callback: function(notification) {
        request.allow();
      }
    },
    {
      label: browserBundle.GetStringFromName("contentPermissions.alwaysForSite"),
      accessKey: "",
      callback: function(notification) {
        Services.perms.addFromPrincipal(request.principal, request.type, Ci.nsIPermissionManager.ALLOW_ACTION);
        request.allow();
      }
    },
    {
      label: browserBundle.GetStringFromName("contentPermissions.neverForSite"),
      accessKey: "",
      callback: function(notification) {
        Services.perms.addFromPrincipal(request.principal, request.type, Ci.nsIPermissionManager.DENY_ACTION);
        request.cancel();
      }
    }];

    let message = browserBundle.formatStringFromName(entityName + ".wantsTo",
                                                     [request.principal.URI.host], 1);
    let newBar = notificationBox.appendNotification(message,
                                                    request.type,
                                                    icon,
                                                    notificationBox.PRIORITY_WARNING_MEDIUM,
                                                    buttons);
  }
};


//module initialization
this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ContentPermissionPrompt]);
