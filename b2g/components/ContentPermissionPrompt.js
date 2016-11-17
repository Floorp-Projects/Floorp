/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

function debug(str) {
  //dump("-*- ContentPermissionPrompt: " + str + "\n");
}

const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;
const Cc = Components.classes;

const PROMPT_FOR_UNKNOWN = ["audio-capture",
                            "desktop-notification",
                            "geolocation",
                            "video-capture"];
// Due to privary issue, permission requests like GetUserMedia should prompt
// every time instead of providing session persistence.
const PERMISSION_NO_SESSION = ["audio-capture", "video-capture"];
const ALLOW_MULTIPLE_REQUESTS = ["audio-capture", "video-capture"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AppsUtils.jsm");
Cu.import("resource://gre/modules/PermissionsTable.jsm");

var permissionManager = Cc["@mozilla.org/permissionmanager;1"].getService(Ci.nsIPermissionManager);
var secMan = Cc["@mozilla.org/scriptsecuritymanager;1"].getService(Ci.nsIScriptSecurityManager);

var permissionSpecificChecker = {};

XPCOMUtils.defineLazyModuleGetter(this, "SystemAppProxy",
                                  "resource://gre/modules/SystemAppProxy.jsm");

/**
 * Determine if a permission should be prompt to user or not.
 *
 * @param aPerm requested permission
 * @param aAction the action according to principal
 * @return true if prompt is required
 */
function shouldPrompt(aPerm, aAction) {
  return ((aAction == Ci.nsIPermissionManager.PROMPT_ACTION) ||
          (aAction == Ci.nsIPermissionManager.UNKNOWN_ACTION &&
           PROMPT_FOR_UNKNOWN.indexOf(aPerm) >= 0));
}

/**
 * Create the default choices for the requested permissions
 *
 * @param aTypesInfo requested permissions
 * @return the default choices for permissions with options, return
 *         undefined if no option in all requested permissions.
 */
function buildDefaultChoices(aTypesInfo) {
  let choices;
  for (let type of aTypesInfo) {
    if (type.options.length > 0) {
      if (!choices) {
        choices = {};
      }
      choices[type.access] = type.options[0];
    }
  }
  return choices;
}

/**
 * aTypesInfo is an array of {permission, access, action, deny} which keeps
 * the information of each permission. This arrary is initialized in
 * ContentPermissionPrompt.prompt and used among functions.
 *
 * aTypesInfo[].permission : permission name
 * aTypesInfo[].access     : permission name + request.access
 * aTypesInfo[].action     : the default action of this permission
 * aTypesInfo[].deny       : true if security manager denied this app's origin
 *                           principal.
 * Note:
 *   aTypesInfo[].permission will be sent to prompt only when
 *   aTypesInfo[].action is PROMPT_ACTION and aTypesInfo[].deny is false.
 */
function rememberPermission(aTypesInfo, aPrincipal, aSession)
{
  function convertPermToAllow(aPerm, aPrincipal)
  {
    let type =
      permissionManager.testExactPermissionFromPrincipal(aPrincipal, aPerm);
    if (shouldPrompt(aPerm, type)) {
      debug("add " + aPerm + " to permission manager with ALLOW_ACTION");
      if (!aSession) {
        permissionManager.addFromPrincipal(aPrincipal,
                                           aPerm,
                                           Ci.nsIPermissionManager.ALLOW_ACTION);
      } else if (PERMISSION_NO_SESSION.indexOf(aPerm) < 0) {
        permissionManager.addFromPrincipal(aPrincipal,
                                           aPerm,
                                           Ci.nsIPermissionManager.ALLOW_ACTION,
                                           Ci.nsIPermissionManager.EXPIRE_SESSION, 0);
      }
    }
  }

  for (let i in aTypesInfo) {
    // Expand the permission to see if we have multiple access properties
    // to convert
    let perm = aTypesInfo[i].permission;
    let access = PermissionsTable[perm].access;
    if (access) {
      for (let idx in access) {
        convertPermToAllow(perm + "-" + access[idx], aPrincipal);
      }
    } else {
      convertPermToAllow(perm, aPrincipal);
    }
  }
}

function ContentPermissionPrompt() {}

ContentPermissionPrompt.prototype = {

  handleExistingPermission: function handleExistingPermission(request,
                                                              typesInfo) {
    typesInfo.forEach(function(type) {
      type.action =
        Services.perms.testExactPermissionFromPrincipal(request.principal,
                                                        type.access);
      if (shouldPrompt(type.access, type.action)) {
        type.action = Ci.nsIPermissionManager.PROMPT_ACTION;
      }
    });

    // If all permissions are allowed already and no more than one option,
    // call allow() without prompting.
    let checkAllowPermission = function(type) {
      if (type.action == Ci.nsIPermissionManager.ALLOW_ACTION &&
          type.options.length <= 1) {
        return true;
      }
      return false;
    }
    if (typesInfo.every(checkAllowPermission)) {
      debug("all permission requests are allowed");
      request.allow(buildDefaultChoices(typesInfo));
      return true;
    }

    // If all permissions are DENY_ACTION or UNKNOWN_ACTION, call cancel()
    // without prompting.
    let checkDenyPermission = function(type) {
      if (type.action == Ci.nsIPermissionManager.DENY_ACTION ||
          type.action == Ci.nsIPermissionManager.UNKNOWN_ACTION) {
        return true;
      }
      return false;
    }
    if (typesInfo.every(checkDenyPermission)) {
      debug("all permission requests are denied");
      request.cancel();
      return true;
    }
    return false;
  },

  // multiple requests should be audio and video
  checkMultipleRequest: function checkMultipleRequest(typesInfo) {
    if (typesInfo.length == 1) {
      return true;
    } else if (typesInfo.length > 1) {
      let checkIfAllowMultiRequest = function(type) {
        return (ALLOW_MULTIPLE_REQUESTS.indexOf(type.access) !== -1);
      }
      if (typesInfo.every(checkIfAllowMultiRequest)) {
        debug("legal multiple requests");
        return true;
      }
    }

    return false;
  },

  handledByPermissionType: function handledByPermissionType(request, typesInfo) {
    for (let i in typesInfo) {
      if (permissionSpecificChecker.hasOwnProperty(typesInfo[i].permission) &&
          permissionSpecificChecker[typesInfo[i].permission](request)) {
        return true;
      }
    }

    return false;
  },

  prompt: function(request) {
    // Initialize the typesInfo and set the default value.
    let typesInfo = [];
    let perms = request.types.QueryInterface(Ci.nsIArray);
    for (let idx = 0; idx < perms.length; idx++) {
      let perm = perms.queryElementAt(idx, Ci.nsIContentPermissionType);
      let tmp = {
        permission: perm.type,
        access: (perm.access && perm.access !== "unused") ?
                  perm.type + "-" + perm.access : perm.type,
        options: [],
        deny: true,
        action: Ci.nsIPermissionManager.UNKNOWN_ACTION
      };

      // Append available options, if any.
      let options = perm.options.QueryInterface(Ci.nsIArray);
      for (let i = 0; i < options.length; i++) {
        let option = options.queryElementAt(i, Ci.nsISupportsString).data;
        tmp.options.push(option);
      }
      typesInfo.push(tmp);
    }

    if (secMan.isSystemPrincipal(request.principal)) {
      request.allow(buildDefaultChoices(typesInfo));
      return;
    }


    if (typesInfo.length == 0) {
      request.cancel();
      return;
    }

    if(!this.checkMultipleRequest(typesInfo)) {
      request.cancel();
      return;
    }

    if (this.handledByPermissionType(request, typesInfo)) {
      return;
    }

    // returns true if the request was handled
    if (this.handleExistingPermission(request, typesInfo)) {
       return;
    }

    // prompt PROMPT_ACTION request or request with options.
    typesInfo = typesInfo.filter(function(type) {
      return !type.deny && (type.action == Ci.nsIPermissionManager.PROMPT_ACTION || type.options.length > 0) ;
    });

    if (!request.element) {
      this.delegatePrompt(request, typesInfo);
      return;
    }

    var cancelRequest = function() {
      request.requester.onVisibilityChange = null;
      request.cancel();
    }

    var self = this;

    // If the request was initiated from a hidden iframe
    // we don't forward it to content and cancel it right away
    request.requester.getVisibility( {
      notifyVisibility: function(isVisible) {
        if (!isVisible) {
          cancelRequest();
          return;
        }

        // Monitor the frame visibility and cancel the request if the frame goes
        // away but the request is still here.
        request.requester.onVisibilityChange = {
          notifyVisibility: function(isVisible) {
            if (isVisible)
              return;

            self.cancelPrompt(request, typesInfo);
            cancelRequest();
          }
        }

        self.delegatePrompt(request, typesInfo, function onCallback() {
          request.requester.onVisibilityChange = null;
        });
      }
    });

  },

  cancelPrompt: function(request, typesInfo) {
    this.sendToBrowserWindow("cancel-permission-prompt", request,
                             typesInfo);
  },

  delegatePrompt: function(request, typesInfo, callback) {
    this.sendToBrowserWindow("permission-prompt", request, typesInfo,
                             function(type, remember, choices) {
      if (type == "permission-allow") {
        rememberPermission(typesInfo, request.principal, !remember);
        if (callback) {
          callback();
        }
        request.allow(choices);
        return;
      }

      let addDenyPermission = function(type) {
        debug("add " + type.permission +
              " to permission manager with DENY_ACTION");
        if (remember) {
          Services.perms.addFromPrincipal(request.principal, type.access,
                                          Ci.nsIPermissionManager.DENY_ACTION);
        } else if (PERMISSION_NO_SESSION.indexOf(type.access) < 0) {
          Services.perms.addFromPrincipal(request.principal, type.access,
                                          Ci.nsIPermissionManager.DENY_ACTION,
                                          Ci.nsIPermissionManager.EXPIRE_SESSION,
                                          0);
        }
      }
      try {
        // This will trow if we are canceling because the remote process died.
        // Just eat the exception and call the callback that will cleanup the
        // visibility event listener.
        typesInfo.forEach(addDenyPermission);
      } catch(e) { }

      if (callback) {
        callback();
      }

      try {
        request.cancel();
      } catch(e) { }
    });
  },

  sendToBrowserWindow: function(type, request, typesInfo, callback) {
    let requestId = Cc["@mozilla.org/uuid-generator;1"]
                  .getService(Ci.nsIUUIDGenerator).generateUUID().toString();
    if (callback) {
      SystemAppProxy.addEventListener("mozContentEvent", function contentEvent(evt) {
        let detail = evt.detail;
        if (detail.id != requestId)
          return;
        SystemAppProxy.removeEventListener("mozContentEvent", contentEvent);

        callback(detail.type, detail.remember, detail.choices);
      })
    }

    let principal = request.principal;
    let isApp = principal.appStatus != Ci.nsIPrincipal.APP_STATUS_NOT_INSTALLED;
    let remember = (principal.appStatus == Ci.nsIPrincipal.APP_STATUS_PRIVILEGED ||
                    principal.appStatus == Ci.nsIPrincipal.APP_STATUS_CERTIFIED)
                    ? true
                    : request.remember;
    let isGranted = typesInfo.every(function(type) {
      return type.action == Ci.nsIPermissionManager.ALLOW_ACTION;
    });
    let permissions = {};
    for (let i in typesInfo) {
      debug("prompt " + typesInfo[i].permission);
      permissions[typesInfo[i].permission] = typesInfo[i].options;
    }

    let details = {
      type: type,
      permissions: permissions,
      id: requestId,
      // This system app uses the origin from permission events to
      // compare against the mozApp.origin of app windows, so we
      // are not concerned with origin suffixes here (appId, etc).
      origin: principal.originNoSuffix,
      isApp: isApp,
      remember: remember,
      isGranted: isGranted,
    };

    if (isApp) {
      details.manifestURL = DOMApplicationRegistry.getManifestURLByLocalId(principal.appId);
    }

    // request.element is defined for OOP content, while request.window
    // is defined for In-Process content.
    // In both cases the message needs to be dispatched to the top-level
    // <iframe mozbrowser> container in the system app.
    // So the above code iterates over window.realFrameElement in order
    // to crosss mozbrowser iframes boundaries and find the top-level
    // one in the system app.
    // window.realFrameElement will be |null| if the code try to cross
    // content -> chrome boundaries.
    let targetElement = request.element;
    let targetWindow = request.window || targetElement.ownerDocument.defaultView;
    while (targetWindow.realFrameElement) {
      targetElement = targetWindow.realFrameElement;
      targetWindow = targetElement.ownerDocument.defaultView;
    }

    SystemAppProxy.dispatchEvent(details, targetElement);
  },

  classID: Components.ID("{8c719f03-afe0-4aac-91ff-6c215895d467}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentPermissionPrompt])
};

(function() {
  // Do not allow GetUserMedia while in call.
  permissionSpecificChecker["audio-capture"] = function(request) {
    let forbid = false;

    if (forbid) {
      request.cancel();
    }

    return forbid;
  };
})();

//module initialization
this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ContentPermissionPrompt]);
