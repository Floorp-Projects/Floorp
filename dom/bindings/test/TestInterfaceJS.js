/* -*- Mode: JavaScript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const Cu = Components.utils;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

function TestInterfaceJS(anyArg, objectArg) {}

TestInterfaceJS.prototype = {
  classID: Components.ID("{2ac4e026-cf25-47d5-b067-78d553c3cad8}"),
  contractID: "@mozilla.org/dom/test-interface-js;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIDOMGlobalPropertyInitializer,
                                         Ci.mozITestInterfaceJS]),

  init: function(win) { this._win = win; },

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

  testThrowError: function() {
    throw new this._win.Error("We are an Error");
  },

  testThrowDOMException: function() {
    throw new this._win.DOMException("We are a DOMException",
                                     "NotSupportedError");
  },

  testThrowTypeError: function() {
    throw new this._win.TypeError("We are a TypeError");
  },

  testThrowNsresult: function() {
      throw Components.results.NS_BINDING_ABORTED;
  },

  testThrowNsresultFromNative: function(x) {
    // We want to throw an exception that we generate from an nsresult thrown
    // by a C++ component.  Ideally we'd have one explicitly for testing, but
    // for now just piggyback on nsStandardURL.
    var url = Components.classes["@mozilla.org/network/standard-url;1"]
                        .createInstance(Ci.nsIURI);
    url.QueryInterface(Ci.nsIMutable);
    url.mutable = false;
    url.spec = "http://example.com";
  },

  testThrowCallbackError: function(callback) {
    callback();
  },

  testThrowXraySelfHosted: function() {
    this._win.Array.indexOf();
  },

  testThrowSelfHosted: function() {
    Array.indexOf();
  },

  testPromiseWithThrowingChromePromiseInit: function() {
    return new this._win.Promise(function() {
      noSuchMethodExistsYo1();
    })
  },

  testPromiseWithThrowingContentPromiseInit: function(func) {
      return new this._win.Promise(func);
  },

  testPromiseWithDOMExceptionThrowingPromiseInit: function() {
    return new this._win.Promise(() => {
      throw new this._win.DOMException("We are a second DOMException",
                                       "NotFoundError");
    })
  },

  testPromiseWithThrowingChromeThenFunction: function() {
    return this._win.Promise.resolve(5).then(function() {
      noSuchMethodExistsYo2();
    });
  },

  testPromiseWithThrowingContentThenFunction: function(func) {
    return this._win.Promise.resolve(10).then(func);
  },

  testPromiseWithDOMExceptionThrowingThenFunction: function() {
    return this._win.Promise.resolve(5).then(() => {
      throw new this._win.DOMException("We are a third DOMException",
                                       "NetworkError");
    });
  },

  testPromiseWithThrowingChromeThenable: function() {
    var thenable =  {
      then: function() {
        noSuchMethodExistsYo3()
      }
    };
    return new this._win.Promise(function(resolve) {
      resolve(thenable)
    });
  },

  testPromiseWithThrowingContentThenable: function(thenable) {
    // Waive Xrays on the thenable, because we're calling resolve() in the
    // chrome compartment, so that's the compartment the "then" property get
    // will happen in, and if we leave the Xray in place the function-valued
    // property won't return the function.
    return this._win.Promise.resolve(Cu.waiveXrays(thenable));
  },

  testPromiseWithDOMExceptionThrowingThenable: function() {
    var thenable =  {
      then: () => {
        throw new this._win.DOMException("We are a fourth DOMException",
                                         "TypeMismatchError");
      }
    };
    return new this._win.Promise(function(resolve) {
      resolve(thenable)
    });
  },

  get onsomething() {
    return this.__DOM_IMPL__.getEventHandler("onsomething");
  },

  set onsomething(val) {
    this.__DOM_IMPL__.setEventHandler("onsomething", val);
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([TestInterfaceJS])
