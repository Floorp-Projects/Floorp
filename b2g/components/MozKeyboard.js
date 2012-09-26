/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const kFormsFrameScript = "chrome://browser/content/forms.js";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/ObjectWrapper.jsm");

// -----------------------------------------------------------------------
// MozKeyboard
// -----------------------------------------------------------------------

function MozKeyboard() { } 

MozKeyboard.prototype = {
  classID: Components.ID("{397a7fdf-2254-47be-b74e-76625a1a66d5}"),

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIB2GKeyboard, Ci.nsIDOMGlobalPropertyInitializer, Ci.nsIObserver
  ]),

  classInfo: XPCOMUtils.generateCI({
    "classID": Components.ID("{397a7fdf-2254-47be-b74e-76625a1a66d5}"),
    "contractID": "@mozilla.org/b2g-keyboard;1",
    "interfaces": [Ci.nsIB2GKeyboard],
    "flags": Ci.nsIClassInfo.DOM_OBJECT,
    "classDescription": "B2G Virtual Keyboard"
  }),

  init: function mozKeyboardInit(win) {
    Services.obs.addObserver(this, "inner-window-destroyed", false);
    Services.obs.addObserver(this, 'in-process-browser-frame-shown', false);
    Services.obs.addObserver(this, 'remote-browser-frame-shown', false);

    this._window = win;
    this._utils = win.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIDOMWindowUtils);
    this.innerWindowID = this._utils.currentInnerWindowID;

    this._focusHandler = null;
  },

  uninit: function mozKeyboardUninit() {
    Services.obs.removeObserver(this, "inner-window-destroyed");
    this._messageManager = null;
    this._window = null;
    this._utils = null;
    this._focusHandler = null;
  },

  sendKey: function mozKeyboardSendKey(keyCode, charCode) {
    charCode = (charCode == undefined) ? keyCode : charCode;
    ["keydown", "keypress", "keyup"].forEach((function sendKey(type) {
      this._utils.sendKeyEvent(type, keyCode, charCode, null);
    }).bind(this));
  },

  setSelectedOption: function mozKeyboardSetSelectedOption(index) {
    this._messageManager.sendAsyncMessage("Forms:Select:Choice", {
      "index": index
    });
  },

  setValue: function mozKeyboardSetValue(value) {
    this._messageManager.sendAsyncMessage("Forms:Input:Value", {
      "value": value
    });
  },

  setSelectedOptions: function mozKeyboardSetSelectedOptions(indexes) {
    this._messageManager.sendAsyncMessage("Forms:Select:Choice", {
      "indexes": indexes || []
    });
  },

  set onfocuschange(val) {
    this._focusHandler = val;
  },

  get onfocuschange() {
    return this._focusHandler;
  },

  handleMessage: function mozKeyboardHandleMessage(msg) {
    let handler = this._focusHandler;
    if (!handler || !(handler instanceof Ci.nsIDOMEventListener))
      return;

    let detail = {
      "detail": msg.json
    };

    let evt = new this._window.CustomEvent("focuschanged",
                                           ObjectWrapper.wrap(detail, this._window));
    handler.handleEvent(evt);
  },

  observe: function mozKeyboardObserve(subject, topic, data) {
    switch (topic) {
    case "inner-window-destroyed": {
      let wId = subject.QueryInterface(Ci.nsISupportsPRUint64).data;
      if (wId == this.innerWindowID) {
        this.uninit();
      }
      break;
    }
    case 'remote-browser-frame-shown':
    case 'in-process-browser-frame-shown': {
      let frameLoader = subject.QueryInterface(Ci.nsIFrameLoader);
      let mm = frameLoader.messageManager;
      mm.addMessageListener("Forms:Input", (function receiveMessage(msg) {
        // Need to save mm here so later the message can be sent back to the
        // correct app in the methods called by the value selector.
        this._messageManager = mm;
        this.handleMessage(msg);
      }).bind(this));
      try {
        mm.loadFrameScript(kFormsFrameScript, true);
      } catch (e) {
        dump('Error loading ' + kFormsFrameScript + ' as frame script: ' + e + '\n');
      }
      break;
    }
    }
  }
};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([MozKeyboard]);

