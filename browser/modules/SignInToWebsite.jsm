/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["SignInToWebsiteUX"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "IdentityService",
                                  "resource://gre/modules/identity/Identity.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Logger",
                                  "resource://gre/modules/identity/LogUtils.jsm");

function log(...aMessageArgs) {
  Logger.log.apply(Logger, ["SignInToWebsiteUX"].concat(aMessageArgs));
}

this.SignInToWebsiteUX = {

  init: function SignInToWebsiteUX_init() {

    Services.obs.addObserver(this, "identity-request", false);
    Services.obs.addObserver(this, "identity-auth", false);
    Services.obs.addObserver(this, "identity-auth-complete", false);
    Services.obs.addObserver(this, "identity-login-state-changed", false);
  },

  uninit: function SignInToWebsiteUX_uninit() {
    Services.obs.removeObserver(this, "identity-request");
    Services.obs.removeObserver(this, "identity-auth");
    Services.obs.removeObserver(this, "identity-auth-complete");
    Services.obs.removeObserver(this, "identity-login-state-changed");
  },

  observe: function SignInToWebsiteUX_observe(aSubject, aTopic, aData) {
    log("observe: received", aTopic, "with", aData, "for", aSubject);
    let options = null;
    if (aSubject) {
      options = aSubject.wrappedJSObject;
    }
    switch(aTopic) {
      case "identity-request":
        this.requestLogin(options);
        break;
      case "identity-auth":
        this._openAuthenticationUI(aData, options);
        break;
      case "identity-auth-complete":
        this._closeAuthenticationUI(aData);
        break;
      case "identity-login-state-changed":
        let emailAddress = aData;
        if (emailAddress) {
          this._removeRequestUI(options);
          this._showLoggedInUI(emailAddress, options);
        } else {
          this._removeLoggedInUI(options);
        }
        break;
      default:
        Logger.reportError("SignInToWebsiteUX", "Unknown observer notification:", aTopic);
        break;
    }
  },

  /**
   * The website is requesting login so the user must choose an identity to use.
   */
  requestLogin: function SignInToWebsiteUX_requestLogin(aOptions) {
    let windowID = aOptions.rpId;
    log("requestLogin", aOptions);
    let [chromeWin, browserEl] = this._getUIForWindowID(windowID);

    // message is not shown in the UI but is required
    let message = aOptions.origin;
    let mainAction = {
      label: chromeWin.gNavigatorBundle.getString("identity.next.label"),
      accessKey: chromeWin.gNavigatorBundle.getString("identity.next.accessKey"),
      callback: function() {}, // required
    };
    let options = {
      identity: {
        origin: aOptions.origin,
      },
    };
    let secondaryActions = [];

    // add some extra properties to the notification to store some identity-related state
    for (let opt in aOptions) {
      options.identity[opt] = aOptions[opt];
    }
    log("requestLogin: rpId: ", options.identity.rpId);

    chromeWin.PopupNotifications.show(browserEl, "identity-request", message,
                                      "identity-notification-icon", mainAction,
                                      [], options);
  },

  /**
   * Get the list of possible identities to login to the given origin.
   */
  getIdentitiesForSite: function SignInToWebsiteUX_getIdentitiesForSite(aOrigin) {
    return IdentityService.RP.getIdentitiesForSite(aOrigin);
  },

  /**
   * User chose a new or existing identity from the doorhanger after a request() call
   */
  selectIdentity: function SignInToWebsiteUX_selectIdentity(aRpId, aIdentity) {
    log("selectIdentity: rpId: ", aRpId, " identity: ", aIdentity);
    IdentityService.selectIdentity(aRpId, aIdentity);
  },

  // Private

  /**
   * Return the chrome window and <browser> for the given outer window ID.
   */
  _getUIForWindowID: function(aWindowID) {
    let content = Services.wm.getOuterWindowWithId(aWindowID);
    if (content) {
      let browser = content.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIWebNavigation)
                           .QueryInterface(Ci.nsIDocShell).chromeEventHandler;
      let chromeWin = browser.ownerDocument.defaultView;
      return [chromeWin, browser];
    }

    Logger.reportError("SignInToWebsiteUX", "no content");
    return [null, null];
  },

  /**
   * Open UI with a content frame displaying aAuthURI so that the user can authenticate with their
   * IDP.  Then tell Identity.jsm the identifier for the window so that it knows that the DOM API
   * calls are for this authentication flow.
   */
  _openAuthenticationUI: function _openAuthenticationUI(aAuthURI, aContext) {
    // Open a tab/window with aAuthURI with an identifier (aID) attached so that the DOM APIs know this is an auth. window.
    let chromeWin = Services.wm.getMostRecentWindow('navigator:browser');
    let features = "chrome=false,width=640,height=480,centerscreen,location=yes,resizable=yes,scrollbars=yes,status=yes";
    log("aAuthURI: ", aAuthURI);
    let authWin = Services.ww.openWindow(chromeWin, "about:blank", "", features, null);
    let windowID = authWin.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).outerWindowID;
    log("authWin outer id: ", windowID);

    let provId = aContext.provId;
    // Tell the ID service about the id before loading the url
    IdentityService.IDP.setAuthenticationFlow(windowID, provId);

    authWin.location = aAuthURI;
  },

  _closeAuthenticationUI: function _closeAuthenticationUI(aAuthId) {
    log("_closeAuthenticationUI:", aAuthId);
    let [chromeWin, browserEl] = this._getUIForWindowID(aAuthId);
    if (chromeWin)
      chromeWin.close();
    else
      Logger.reportError("SignInToWebsite", "Could not close window with ID", aAuthId);
  },

  /**
   * Show a doorhanger indicating the currently logged-in user.
   */
  _showLoggedInUI: function _showLoggedInUI(aIdentity, aContext) {
    let windowID = aContext.rpId;
    log("_showLoggedInUI for ", windowID);
    let [chromeWin, browserEl] = this._getUIForWindowID(windowID);

    let message = chromeWin.gNavigatorBundle.getFormattedString("identity.loggedIn.description",
                                                          [aIdentity]);
    let mainAction = {
      label: chromeWin.gNavigatorBundle.getString("identity.loggedIn.signOut.label"),
      accessKey: chromeWin.gNavigatorBundle.getString("identity.loggedIn.signOut.accessKey"),
      callback: function() {
        log("sign out callback fired");
        IdentityService.RP.logout(windowID);
      },
    };
    let secondaryActions = [];
    let options = {
      dismissed: true,
    };
    let loggedInNot = chromeWin.PopupNotifications.show(browserEl, "identity-logged-in", message,
                                                  "identity-notification-icon", mainAction,
                                                  secondaryActions, options);
    loggedInNot.rpId = windowID;
  },

  /**
   * Remove the doorhanger indicating the currently logged-in user.
   */
  _removeLoggedInUI: function _removeLoggedInUI(aContext) {
    let windowID = aContext.rpId;
    log("_removeLoggedInUI for ", windowID);
    if (!windowID)
      throw "_removeLoggedInUI: Invalid RP ID";
    let [chromeWin, browserEl] = this._getUIForWindowID(windowID);

    let loggedInNot = chromeWin.PopupNotifications.getNotification("identity-logged-in", browserEl);
    if (loggedInNot)
      chromeWin.PopupNotifications.remove(loggedInNot);
  },

  /**
   * Remove the doorhanger indicating the currently logged-in user.
   */
  _removeRequestUI: function _removeRequestUI(aContext) {
    let windowID = aContext.rpId;
    log("_removeRequestUI for ", windowID);
    let [chromeWin, browserEl] = this._getUIForWindowID(windowID);

    let requestNot = chromeWin.PopupNotifications.getNotification("identity-request", browserEl);
    if (requestNot)
      chromeWin.PopupNotifications.remove(requestNot);
  },

};
