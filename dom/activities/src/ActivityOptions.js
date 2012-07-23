/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
 
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
 
function debug(aMsg) {
  //dump("-- ActivityOptions.js " + Date.now() + " : " + aMsg + "\n");
}

/**
  * nsIDOMMozActivityOptions implementation.
  */

function ActivityOptions() {
  debug("ActivityOptions");
  this.wrappedJSObject = this;

  // When a system message of type 'activity' is emitted, it forces the
  // creation of an ActivityWrapper which in turns replace the default
  // system message callback. The newly created wrapper then create a
  // nsIDOMActivityRequestHandler object and fills up the properties of 
  // this object as well as the properties of the nsIDOMActivityOptions
  // object contains by the request handler.
  this._name = null;
  this._data = null;
}

ActivityOptions.prototype = {
  get name() {
    return this._name;
  },

  get data() {
    return this._data;
  },

  classID: Components.ID("{ee983dbb-d5ea-4c5b-be98-10a13cac9f9d}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMMozActivityOptions]),

  classInfo: XPCOMUtils.generateCI({
    classID: Components.ID("{ee983dbb-d5ea-4c5b-be98-10a13cac9f9d}"),
    contractID: "@mozilla.org/dom/activities/options;1",
    interfaces: [Ci.nsIDOMMozActivityOptions],
    flags: Ci.nsIClassInfo.DOM_OBJECT,
    classDescription: "Activity Options"
  })
}

const NSGetFactory = XPCOMUtils.generateNSGetFactory([ActivityOptions]);
