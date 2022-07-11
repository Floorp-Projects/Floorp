/* -*- Mode: JavaScript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/* global noSuchMethodExistsYo1, noSuchMethodExistsYo2, noSuchMethodExistsYo3 */

"use strict";

var EXPORTED_SYMBOLS = ["TestInterfaceJS"];

function TestInterfaceJS() {}

TestInterfaceJS.prototype = {
  QueryInterface: ChromeUtils.generateQI([
    "nsIDOMGlobalPropertyInitializer",
    "mozITestInterfaceJS",
  ]),

  init(win) {
    this._win = win;
  },

  __init(anyArg, objectArg, dictionaryArg) {
    this._anyAttr = undefined;
    this._objectAttr = null;
    this._anyArg = anyArg;
    this._objectArg = objectArg;
    this._dictionaryArg = dictionaryArg;
  },

  get anyArg() {
    return this._anyArg;
  },
  get objectArg() {
    return this._objectArg;
  },
  getDictionaryArg() {
    return this._dictionaryArg;
  },
  get anyAttr() {
    return this._anyAttr;
  },
  set anyAttr(val) {
    this._anyAttr = val;
  },
  get objectAttr() {
    return this._objectAttr;
  },
  set objectAttr(val) {
    this._objectAttr = val;
  },
  getDictionaryAttr() {
    return this._dictionaryAttr;
  },
  setDictionaryAttr(val) {
    this._dictionaryAttr = val;
  },
  pingPongAny(any) {
    return any;
  },
  pingPongObject(obj) {
    return obj;
  },
  pingPongObjectOrString(objectOrString) {
    return objectOrString;
  },
  pingPongDictionary(dict) {
    return dict;
  },
  pingPongDictionaryOrLong(dictOrLong) {
    return dictOrLong.anyMember || dictOrLong;
  },
  pingPongRecord(rec) {
    return JSON.stringify(rec);
  },
  objectSequenceLength(seq) {
    return seq.length;
  },
  anySequenceLength(seq) {
    return seq.length;
  },

  getCallerPrincipal() {
    return Cu.getWebIDLCallerPrincipal().origin;
  },

  convertSVS(svs) {
    return svs;
  },

  pingPongUnion(x) {
    return x;
  },
  pingPongUnionContainingNull(x) {
    return x;
  },
  pingPongNullableUnion(x) {
    return x;
  },
  returnBadUnion(x) {
    return 3;
  },

  testSequenceOverload(arg) {},
  testSequenceUnion(arg) {},

  testThrowError() {
    throw new this._win.Error("We are an Error");
  },

  testThrowDOMException() {
    throw new this._win.DOMException(
      "We are a DOMException",
      "NotSupportedError"
    );
  },

  testThrowTypeError() {
    throw new this._win.TypeError("We are a TypeError");
  },

  testThrowNsresult() {
    // This is explicitly testing preservation of raw thrown Crs in XPCJS
    // eslint-disable-next-line mozilla/no-throw-cr-literal
    throw Cr.NS_BINDING_ABORTED;
  },

  testThrowNsresultFromNative(x) {
    // We want to throw an exception that we generate from an nsresult thrown
    // by a C++ component.
    Services.io.notImplemented();
  },

  testThrowCallbackError(callback) {
    callback();
  },

  testThrowXraySelfHosted() {
    this._win.Array.prototype.forEach();
  },

  testThrowSelfHosted() {
    Array.prototype.forEach();
  },

  testPromiseWithThrowingChromePromiseInit() {
    return new this._win.Promise(function() {
      noSuchMethodExistsYo1();
    });
  },

  testPromiseWithThrowingContentPromiseInit(func) {
    return new this._win.Promise(func);
  },

  testPromiseWithDOMExceptionThrowingPromiseInit() {
    return new this._win.Promise(() => {
      throw new this._win.DOMException(
        "We are a second DOMException",
        "NotFoundError"
      );
    });
  },

  testPromiseWithThrowingChromeThenFunction() {
    return this._win.Promise.resolve(5).then(function() {
      noSuchMethodExistsYo2();
    });
  },

  testPromiseWithThrowingContentThenFunction(func) {
    return this._win.Promise.resolve(10).then(func);
  },

  testPromiseWithDOMExceptionThrowingThenFunction() {
    return this._win.Promise.resolve(5).then(() => {
      throw new this._win.DOMException(
        "We are a third DOMException",
        "NetworkError"
      );
    });
  },

  testPromiseWithThrowingChromeThenable() {
    var thenable = {
      then() {
        noSuchMethodExistsYo3();
      },
    };
    return new this._win.Promise(function(resolve) {
      resolve(thenable);
    });
  },

  testPromiseWithThrowingContentThenable(thenable) {
    // Waive Xrays on the thenable, because we're calling resolve() in the
    // chrome compartment, so that's the compartment the "then" property get
    // will happen in, and if we leave the Xray in place the function-valued
    // property won't return the function.
    return this._win.Promise.resolve(Cu.waiveXrays(thenable));
  },

  testPromiseWithDOMExceptionThrowingThenable() {
    var thenable = {
      then: () => {
        throw new this._win.DOMException(
          "We are a fourth DOMException",
          "TypeMismatchError"
        );
      },
    };
    return new this._win.Promise(function(resolve) {
      resolve(thenable);
    });
  },

  get onsomething() {
    return this.__DOM_IMPL__.getEventHandler("onsomething");
  },

  set onsomething(val) {
    this.__DOM_IMPL__.setEventHandler("onsomething", val);
  },
};
