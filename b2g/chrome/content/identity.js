/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This JS shim contains the callbacks to fire DOMRequest events for
// navigator.pay API within the payment processor's scope.

"use strict";

var { classes: Cc, interfaces: Ci, utils: Cu }  = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

XPCOMUtils.defineLazyServiceGetter(this, "uuidgen",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

XPCOMUtils.defineLazyModuleGetter(this, "Logger",
                                  "resource://gre/modules/identity/LogUtils.jsm");

function log(...aMessageArgs) {
  Logger.log.apply(Logger, ["injected identity.js"].concat(aMessageArgs));
}

log("\n\n======================= identity.js =======================\n\n");

// This script may be injected more than once into an iframe.
// It's hard to do this with |const| like we should, so use var instead.
if (typeof kIdentityJSLoaded === 'undefined') {
  var kIdentityDelegateWatch = "identity-delegate-watch";
  var kIdentityDelegateRequest = "identity-delegate-request";
  var kIdentityDelegateLogout = "identity-delegate-logout";
  var kIdentityDelegateReady = "identity-delegate-ready";
  var kIdentityDelegateFinished = "identity-delegate-finished";
  var kIdentityControllerDoMethod = "identity-controller-doMethod";
  var kIdentktyJSLoaded = true;
}

var showUI = false;
var options = {};
var isLoaded = false;
var func = null;

/*
 * Message back to the SignInToWebsite pipe.  Message should be an
 * object with the following keys:
 *
 *   method:             one of 'login', 'logout', 'ready'
 *   assertion:          optional assertion
 */
function identityCall(message) {
  if (options._internal) {
    message._internal = options._internal;
  }
  sendAsyncMessage(kIdentityControllerDoMethod, message);
}

/*
 * To close the dialog, we first tell the gecko SignInToWebsite manager that it
 * can clean up.  Then we tell the gaia component that we are finished.  It is
 * necessary to notify gecko first, so that the message can be sent before gaia
 * destroys our context.
 */
function closeIdentityDialog() {
  // tell gecko we're done.
  func = null; options = null;
  sendAsyncMessage(kIdentityDelegateFinished);
}

/*
 * doInternalWatch - call the internal.watch api and relay the results
 * up to the controller.
 */
function doInternalWatch() {
  log("doInternalWatch:", options, isLoaded);
  if (options && isLoaded) {
    let BrowserID = content.wrappedJSObject.BrowserID;
    BrowserID.internal.watch(function(aParams, aInternalParams) {
        identityCall(aParams);
        if (aParams.method === "ready") {
          closeIdentityDialog();
        }
      },
      JSON.stringify(options),
      function(...things) {
        // internal watch log callback
        log("(watch) internal: ", things);
      }
    );
  }
}

function doInternalRequest() {
  log("doInternalRequest:", options && isLoaded);
  if (options && isLoaded) {
    var stringifiedOptions = JSON.stringify(options);
    content.wrappedJSObject.BrowserID.internal.get(
      options.origin,
      function(assertion, internalParams) {
        internalParams = internalParams || {};
        if (assertion) {
          identityCall({
            method: 'login',
            assertion: assertion,
            _internalParams: internalParams});
        } else {
          identityCall({
            method: 'cancel'
          });
        }
        closeIdentityDialog();
      },
      stringifiedOptions);
  }
}
function doInternalLogout(aOptions) {
  log("doInternalLogout:", (options && isLoaded));
  if (options && isLoaded) {
    let BrowserID = content.wrappedJSObject.BrowserID;
    BrowserID.internal.logout(options.origin, function() {
      identityCall({method:'logout'});
      closeIdentityDialog();
    });
  }
}

addEventListener("DOMContentLoaded", function(e) {
  content.addEventListener("load", function(e) {
    isLoaded = true;
     // bring da func
     if (func) func();
  });
});

// listen for request
addMessageListener(kIdentityDelegateRequest, function(aMessage) {
  log("injected identity.js received", kIdentityDelegateRequest);
  options = aMessage.json;
  showUI = true;
  func = doInternalRequest;
  func();
});

// listen for watch
addMessageListener(kIdentityDelegateWatch, function(aMessage) {
  log("injected identity.js received", kIdentityDelegateWatch);
  options = aMessage.json;
  showUI = false;
  func = doInternalWatch;
  func();
});

// listen for logout
addMessageListener(kIdentityDelegateLogout, function(aMessage) {
  log("injected identity.js received", kIdentityDelegateLogout);
  options = aMessage.json;
  showUI = false;
  func = doInternalLogout;
  func();
});
