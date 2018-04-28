/* -*- Mode: JavaScript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

function TestInterfaceJSMaplike() {}

TestInterfaceJSMaplike.prototype = {
  classID: Components.ID("{4bc6f6f3-e005-4f0a-b42d-4d1663a9013a}"),
  contractID: "@mozilla.org/dom/test-interface-js-maplike;1",
  QueryInterface: ChromeUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer]),

  init: function(win) { this._win = win; },

  __init: function () {},

  setInternal: function(aKey, aValue) {
    return this.__DOM_IMPL__.__set(aKey, aValue);
  },

  deleteInternal: function(aKey) {
    return this.__DOM_IMPL__.__delete(aKey);
  },

  clearInternal: function() {
    return this.__DOM_IMPL__.__clear();
  },

  __onget: function(key, value) {
    /* no-op */
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([TestInterfaceJSMaplike])
