/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// -----------------------------------------------------------------------
// Alerts Service
// -----------------------------------------------------------------------

function AlertsService() { }

AlertsService.prototype = {
  classID: Components.ID("{fe33c107-82a4-41d6-8c64-5353267e04c9}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAlertsService]),

  showAlertNotification: function(aImageUrl, aTitle, aText, aTextClickable,
                                  aCookie, aAlertListener, aName, aDir, aLang) {
    let browser = Services.wm.getMostRecentWindow("navigator:browser");
    try {
      browser.AlertsHelper.showAlertNotification(aImageUrl, aTitle, aText, aTextClickable, aCookie, aAlertListener);
    } catch (ex) {
      let chromeWin = this._getChromeWindow(browser).wrappedJSObject;
      let notificationBox = chromeWin.Browser.getNotificationBox();
      notificationBox.appendNotification(aTitle,
                                         aText,
                                         aImageUrl,
                                         notificationBox.PRIORITY_WARNING_MEDIUM,
                                         null);
    }
  },

  closeAlert: function(aName) {
    let browser = Services.wm.getMostRecentWindow("navigator:browser");
    browser.AlertsHelper.closeAlert();
  },

  _getChromeWindow: function (aWindow) {
      let chromeWin = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIWebNavigation)
                            .QueryInterface(Ci.nsIDocShellTreeItem)
                            .rootTreeItem
                            .QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIDOMWindow)
                            .QueryInterface(Ci.nsIDOMChromeWindow);
     return chromeWin;
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([AlertsService]);
