/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsISyncMessageSender");

function debug(aMsg) {
  //dump("-- ActivityRequestHandler.js " + Date.now() + " : " + aMsg + "\n");
}

/**
  * nsIDOMMozActivityRequestHandler implementation.
  */

function ActivityRequestHandler() {
  debug("ActivityRequestHandler");

  // When a system message of type 'activity' is emitted, it forces the
  // creation of an ActivityWrapper which in turns replace the default
  // system message callback. The newly created wrapper then create an
  // ActivityRequestHandler object.
}

ActivityRequestHandler.prototype = {
  init: function arh_init(aWindow) {
    this._window = aWindow;
  },

  __init: function arh___init(aId, aOptions, aReturnValue) {
    this._id = aId;
    this._options = aOptions;
    this._returnValue = aReturnValue;
  },

  get source() {
    // We need to clone this object because the this._options.data has
    // the type any in WebIDL which will cause the binding layer to pass
    // the value which is a COW unmodified to content.
    return Cu.cloneInto(this._options, this._window);
  },

  postResult: function arh_postResult(aResult) {
    if (this._returnValue) {
      cpmm.sendAsyncMessage("Activity:PostResult", {
        "id": this._id,
        "result": aResult
      });
      Services.obs.notifyObservers(null, "activity-success", this._id);
    } else {
      Cu.reportError("postResult() can't be called when 'returnValue': 'true' isn't declared in manifest.webapp");
      throw new Error("postResult() can't be called when 'returnValue': 'true' isn't declared in manifest.webapp");
    }
  },

  postError: function arh_postError(aError) {
    cpmm.sendAsyncMessage("Activity:PostError", {
      "id": this._id,
      "error": aError
    });
    Services.obs.notifyObservers(null, "activity-error", this._id);
  },

  classID: Components.ID("{9326952a-dbe3-4d81-a51f-d9c160d96d6b}"),

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIDOMGlobalPropertyInitializer
  ])
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ActivityRequestHandler]);
