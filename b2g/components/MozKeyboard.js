/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// -----------------------------------------------------------------------
// MozKeyboard
// -----------------------------------------------------------------------

function MozKeyboard() { } 

MozKeyboard.prototype = {
  classID: Components.ID("{397a7fdf-2254-47be-b74e-76625a1a66d5}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIB2GKeyboard, Ci.nsIDOMGlobalPropertyInitializer, Ci.nsIObserver]),
  classInfo: XPCOMUtils.generateCI({classID: Components.ID("{397a7fdf-2254-47be-b74e-76625a1a66d5}"),
                                    contractID: "@mozilla.org/b2g-keyboard;1",
                                    interfaces: [Ci.nsIB2GKeyboard],
                                    flags: Ci.nsIClassInfo.DOM_OBJECT,
                                    classDescription: "B2G Virtual Keyboard"}),

  init: function(aWindow) {
    this._utils = aWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    Services.obs.addObserver(this, "inner-window-destroyed", false);
    this.innerWindowID = this._utils.currentInnerWindowID;
  },

  observe: function(aSubject, aTopic, aData) {
    if (aTopic == "inner-window-destroyed") {
      let wId = aSubject.QueryInterface(Ci.nsISupportsPRUint64).data;
      if (wId == this.innerWindowID) {
        Services.obs.removeObserver(this, "inner-window-destroyed");
        this._utils = null;
      }
    }
  },

  sendKey: function mozKeyboardSendKey(keyCode, charCode) {
    charCode = (charCode == undefined) ? keyCode : charCode;
    ['keydown', 'keypress', 'keyup'].forEach((function sendKey(type) {
      this._utils.sendKeyEvent(type, keyCode, charCode, null);
    }).bind(this));
  }
};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([MozKeyboard]);
