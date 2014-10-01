/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cu = Components.utils;
const Ci = Components.interfaces;

"use strict";
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

function TestInterfaceJS(anyArg, objectArg) {}

TestInterfaceJS.prototype = {
  classID: Components.ID("{2ac4e026-cf25-47d5-b067-78d553c3cad8}"),
  contractID: "@mozilla.org/dom/test-interface-js;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports]),

  __init: function (anyArg, objectArg, dictionaryArg) {
    this._anyAttr = undefined;
    this._objectAttr = null;
    this._anyArg = anyArg;
    this._objectArg = objectArg;
    this._dictionaryArg = dictionaryArg;
    this._cachedAttr = 15;
  },

  get anyArg() { return this._anyArg; },
  get objectArg() { return this._objectArg; },
  get dictionaryArg() { return this._dictionaryArg; },
  get anyAttr() { return this._anyAttr; },
  set anyAttr(val) { this._anyAttr = val; },
  get objectAttr() { return this._objectAttr; },
  set objectAttr(val) { this._objectAttr = val; },
  get dictionaryAttr() { return this._dictionaryAttr; },
  set dictionaryAttr(val) { this._dictionaryAttr = val; },
  pingPongAny: function(any) { return any; },
  pingPongObject: function(obj) { return obj; },
  pingPongObjectOrString: function(objectOrString) { return objectOrString; },
  pingPongDictionary: function(dict) { return dict; },
  pingPongDictionaryOrLong: function(dictOrLong) { return dictOrLong.anyMember || dictOrLong; },
  pingPongMap: function(map) { return JSON.stringify(map); },
  objectSequenceLength: function(seq) { return seq.length; },
  anySequenceLength: function(seq) { return seq.length; },


  getCallerPrincipal: function() { return Cu.getWebIDLCallerPrincipal().origin; },

  convertSVS: function(svs) { return svs; },

  pingPongUnion: function(x) { return x; },
  pingPongUnionContainingNull: function(x) { return x; },
  pingPongNullableUnion: function(x) { return x; },
  returnBadUnion: function(x) { return 3; },

  get cachedAttr() { return this._cachedAttr; },
  setCachedAttr: function(n) { this._cachedAttr = n; },
  clearCachedAttrCache: function () { this.__DOM_IMPL__._clearCachedCachedAttrValue(); },

  testSequenceOverload: function(arg) {},
  testSequenceUnion: function(arg) {},
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([TestInterfaceJS])
