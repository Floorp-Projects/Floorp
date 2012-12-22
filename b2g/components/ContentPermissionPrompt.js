/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

let DEBUG = 0;
let debug;
if (DEBUG) {
  debug = function (s) { dump("-*- ContentPermissionPrompt: " + s + "\n"); };
}
else {
  debug = function (s) {};
}

const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;
const Cc = Components.classes;

const PROMPT_FOR_UNKNOWN = ['geolocation'];

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

function rememberPermission(aPermission, aPrincipal)
{
  function convertPermToAllow(aPerm, aPrincipal)
  {
    let type =
      permissionManager.testExactPermissionFromPrincipal(aPrincipal, aPerm);
    if (type == Ci.nsIPermissionManager.PROMPT_ACTION ||
        (type == Ci.nsIPermissionManager.UNKNOWN_ACTION &&
        PROMPT_FOR_UNKNOWN.indexOf(aPermission) >= 0)) {
      permissionManager.addFromPrincipal(aPrincipal,
                                         aPerm,
                                         Ci.nsIPermissionManager.ALLOW_ACTION);
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

  _id: 0,
  prompt: function(request) {
    // returns true if the request was handled
    if (this.handleExistingPermission(request))
       return;

    let browser = Services.wm.getMostRecentWindow("navigator:browser");
    let content = browser.getContentWindow();
    if (!content)
      return;

    let access = (request.access && request.access !== "unused") ? request.type + "-" + request.access :
                                                                   request.type;

    let requestId = this._id++;
    content.addEventListener("mozContentEvent", function contentEvent(evt) {
      if (evt.detail.id != requestId)
        return;
      evt.target.removeEventListener(evt.type, contentEvent);

      if (evt.detail.type == "permission-allow") {
        if (evt.detail.remember) {
          rememberPermission(request.type, request.principal);
        }

        request.allow();
        return;
      }

      if (evt.detail.remember) {
        Services.perms.addFromPrincipal(request.principal, access,
                                        Ci.nsIPermissionManager.DENY_ACTION);
      }

      request.cancel();
    });

    let principal = request.principal;
    let isApp = principal.appStatus != Ci.nsIPrincipal.APP_STATUS_NOT_INSTALLED;
    let remember = principal.appStatus == Ci.nsIPrincipal.APP_STATUS_PRIVILEGED
                   ? true
                   : request.remember;

    let details = {
      type: "permission-prompt",
      permission: request.type,
      id: requestId,
      origin: principal.origin,
      isApp: isApp,
      remember: remember
    };

    this._permission = access;
    this._uri = request.principal.URI.spec;
    this._origin = request.principal.origin;

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
