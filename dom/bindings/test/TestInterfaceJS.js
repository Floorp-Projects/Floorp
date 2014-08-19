/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cu = Components.utils;
const Ci = Components.interfaces;

"use strict";
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

var gGlobal = this;
function checkGlobal(obj) {
  if (Object(obj) === obj && Cu.getGlobalForObject(obj) != gGlobal) {
    // This message may not make it to the caller in a useful form, so dump
    // as well.
    var msg = "TestInterfaceJS received an object from a different scope!";
    dump(msg + "\n");
    throw new Error(msg);
  }
}

function TestInterfaceJS(anyArg, objectArg) {}

TestInterfaceJS.prototype = {
  classID: Components.ID("{2ac4e026-cf25-47d5-b067-78d553c3cad8}"),
  contractID: "@mozilla.org/dom/test-interface-js;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports]),

  __init: function (anyArg, objectArg) {
    this._anyAttr = undefined;
    this._objectAttr = null;
    this._anyArg = anyArg;
    this._objectArg = objectArg;
    checkGlobal(anyArg);
    checkGlobal(objectArg);
  },

  get anyArg() { return this._anyArg; },
  get objectArg() { return this._objectArg; },
  get anyAttr() { return this._anyAttr; },
  set anyAttr(val) { checkGlobal(val); this._anyAttr = val; },
  get objectAttr() { return this._objectAttr; },
  set objectAttr(val) { checkGlobal(val); this._objectAttr = val; },
  pingPongAny: function(any) { checkGlobal(any); return any; },
  pingPongObject: function(obj) { checkGlobal(obj); return obj; },

  getCallerPrincipal: function() { return Cu.getWebIDLCallerPrincipal().origin; },

  convertSVS: function(svs) { return svs; },

  pingPongUnion: function(x) { return x; },
  pingPongUnionContainingNull: function(x) { return x; },
  pingPongNullableUnion: function(x) { return x; },
  returnBadUnion: function(x) { return 3; }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([TestInterfaceJS])
