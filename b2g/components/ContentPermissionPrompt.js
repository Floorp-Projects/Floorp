/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

function debug(str) {
  //dump("-*- ContentPermissionPrompt: " + s + "\n");
}

const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;
const Cc = Components.classes;

const PROMPT_FOR_UNKNOWN = ["geolocation", "desktop-notification"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Webapps.jsm");
Cu.import("resource://gre/modules/AppsUtils.jsm");
Cu.import("resource://gre/modules/PermissionsInstaller.jsm");
Cu.import("resource://gre/modules/PermissionsTable.jsm");

var permissionManager = Cc["@mozilla.org/permissionmanager;1"].getService(Ci.nsIPermissionManager);
var secMan = Cc["@mozilla.org/scriptsecuritymanager;1"].getService(Ci.nsIScriptSecurityManager);

XPCOMUtils.defineLazyServiceGetter(this,
                                   "PermSettings",
                                   "@mozilla.org/permissionSettings;1",
                                   "nsIDOMPermissionSettings");

function rememberPermission(aPermission, aPrincipal, aSession)
{
  function convertPermToAllow(aPerm, aPrincipal)
  {
    let type =
      permissionManager.testExactPermissionFromPrincipal(aPrincipal, aPerm);
    if (type == Ci.nsIPermissionManager.PROMPT_ACTION ||
        (type == Ci.nsIPermissionManager.UNKNOWN_ACTION &&
        PROMPT_FOR_UNKNOWN.indexOf(aPermission) >= 0)) {
      if (!aSession) {
        permissionManager.addFromPrincipal(aPrincipal,
                                           aPerm,
                                           Ci.nsIPermissionManager.ALLOW_ACTION);
      } else {
        permissionManager.addFromPrincipal(aPrincipal,
                                           aPerm,
                                           Ci.nsIPermissionManager.ALLOW_ACTION,
                                           Ci.nsIPermissionManager.EXPIRE_SESSION, 0);
      }
    }
  }

  // Expand the permission to see if we have multiple access properties to convert
  let access = PermissionsTable[aPermission].access;
  if (access) {
    for (let idx in access) {
      convertPermToAllow(aPermission + "-" + access[idx], aPrincipal);
    }
  } else {
    convertPermToAllow(aPermission, aPrincipal);
  }
}

function ContentPermissionPrompt() {}

ContentPermissionPrompt.prototype = {

  handleExistingPermission: function handleExistingPermission(request) {
    let access = (request.access && request.access !== "unused") ? request.type + "-" + request.access :
                                                                   request.type;
    let result = Services.perms.testExactPermissionFromPrincipal(request.principal, access);
    if (result == Ci.nsIPermissionManager.ALLOW_ACTION) {
      request.allow();
      return true;
    }
    if (result == Ci.nsIPermissionManager.DENY_ACTION ||
        result == Ci.nsIPermissionManager.UNKNOWN_ACTION && PROMPT_FOR_UNKNOWN.indexOf(access) < 0) {
      request.cancel();
      return true;
    }
    return false;
  },

  handledByApp: function handledByApp(request) {
    if (request.principal.appId == Ci.nsIScriptSecurityManager.NO_APP_ID ||
        request.principal.appId == Ci.nsIScriptSecurityManager.UNKNOWN_APP_ID) {
      // This should not really happen
      request.cancel();
      return true;
    }

    let appsService = Cc["@mozilla.org/AppsService;1"]
                        .getService(Ci.nsIAppsService);
    let app = appsService.getAppByLocalId(request.principal.appId);

    let url = Services.io.newURI(app.origin, null, null);
    let principal = secMan.getAppCodebasePrincipal(url, request.principal.appId,
                                                   /*mozbrowser*/false);
    let access = (request.access && request.access !== "unused") ? request.type + "-" + request.access :
                                                                   request.type;
    let result = Services.perms.testExactPermissionFromPrincipal(principal, access);

    if (result == Ci.nsIPermissionManager.ALLOW_ACTION ||
        result == Ci.nsIPermissionManager.PROMPT_ACTION) {
      return false;
    }

    request.cancel();
    return true;
  },

  _id: 0,
  prompt: function(request) {
    if (secMan.isSystemPrincipal(request.principal)) {
      request.allow();
      return true;
    }

    if (this.handledByApp(request))
        return;

    // returns true if the request was handled
    if (this.handleExistingPermission(request))
       return;

    let frame = request.element;
    let requestId = this._id++;

    if (!frame) {
      this.delegatePrompt(request, requestId);
      return;
    }

    frame = frame.wrappedJSObject;
    var cancelRequest = function() {
      frame.removeEventListener("mozbrowservisibilitychange", onVisibilityChange);
      request.cancel();
    }

    var self = this;
    var onVisibilityChange = function(evt) {
      if (evt.detail.visible === true)
        return;

      self.cancelPrompt(request, requestId);
      cancelRequest();
    }

    // If the request was initiated from a hidden iframe
    // we don't forward it to content and cancel it right away
    let domRequest = frame.getVisible();
    domRequest.onsuccess = function gv_success(evt) {
      if (!evt.target.result) {
        cancelRequest();
        return;
      }

      // Monitor the frame visibility and cancel the request if the frame goes
      // away but the request is still here.
      frame.addEventListener("mozbrowservisibilitychange", onVisibilityChange);

      self.delegatePrompt(request, requestId, function onCallback() {
        frame.removeEventListener("mozbrowservisibilitychange", onVisibilityChange);
      });
    };

    // Something went wrong. Let's cancel the request just in case.
    domRequest.onerror = function gv_error() {
      cancelRequest();
    }
  },

  cancelPrompt: function(request, requestId) {
    this.sendToBrowserWindow("cancel-permission-prompt", request, requestId);
  },

  delegatePrompt: function(request, requestId, callback) {
    let access = (request.access && request.access !== "unused") ? request.type + "-" + request.access :
                                                                   request.type;
    let principal = request.principal;

    this._permission = access;
    this._uri = principal.URI.spec;
    this._origin = principal.origin;

    this.sendToBrowserWindow("permission-prompt", request, requestId, function(type, remember) {
      if (type == "permission-allow") {
        rememberPermission(request.type, principal, !remember);
        if (callback) {
          callback();
        }
        request.allow();
        return;
      }

      if (remember) {
        Services.perms.addFromPrincipal(principal, access,
                                        Ci.nsIPermissionManager.DENY_ACTION);
      } else {
        Services.perms.addFromPrincipal(principal, access,
                                        Ci.nsIPermissionManager.DENY_ACTION,
                                        Ci.nsIPermissionManager.EXPIRE_SESSION, 0);
      }

      if (callback) {
        callback();
      }
      request.cancel();
    });
  },

  sendToBrowserWindow: function(type, request, requestId, callback) {
    let browser = Services.wm.getMostRecentWindow("navigator:browser");
    let content = browser.getContentWindow();
    if (!content)
      return;

    if (callback) {
      content.addEventListener("mozContentEvent", function contentEvent(evt) {
        let detail = evt.detail;
        if (detail.id != requestId)
          return;
        evt.target.removeEventListener(evt.type, contentEvent);

        callback(detail.type, detail.remember);
      })
    }

    let principal = request.principal;
    let isApp = principal.appStatus != Ci.nsIPrincipal.APP_STATUS_NOT_INSTALLED;
    let remember = (principal.appStatus == Ci.nsIPrincipal.APP_STATUS_PRIVILEGED ||
                    principal.appStatus == Ci.nsIPrincipal.APP_STATUS_CERTIFIED)
                    ? true
                    : request.remember;

    let details = {
      type: type,
      permission: request.type,
      id: requestId,
      origin: principal.origin,
      isApp: isApp,
      remember: remember
    };

    if (!isApp) {
      browser.shell.sendChromeEvent(details);
      return;
    }

    // When it's an app, get the manifest to add the l10n application name.
    let app = DOMApplicationRegistry.getAppByLocalId(principal.appId);
    DOMApplicationRegistry.getManifestFor(app.origin, function getManifest(aManifest) {
      let helper = new ManifestHelper(aManifest, app.origin);
      details.appName = helper.name;
      browser.shell.sendChromeEvent(details);
    });
  },

  classID: Components.ID("{8c719f03-afe0-4aac-91ff-6c215895d467}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentPermissionPrompt])
};


//module initialization
this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ContentPermissionPrompt]);
