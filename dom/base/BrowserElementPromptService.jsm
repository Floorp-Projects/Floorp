/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/* vim: set ft=javascript : */

"use strict";

let Cu = Components.utils;
let Ci = Components.interfaces;
let Cc = Components.classes;
let Cr = Components.results;
let Cm = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);

let EXPORTED_SYMBOLS = ["BrowserElementPromptService"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function debug(msg) {
  //dump("BrowserElementPromptService - " + msg + "\n");
}

function BrowserElementPrompt(win, browserElementChild) {
  this._win = win;
  this._browserElementChild = browserElementChild;
}

BrowserElementPrompt.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPrompt]),

  alert: function(title, text) {
    this._browserElementChild.showModalPrompt(
      this._win, {promptType: "alert", title: title, message: text});
  },

  alertCheck: function(title, text, checkMsg, checkState) {
    // Treat this like a normal alert() call, ignoring the checkState.  The
    // front-end can do its own suppression of the alert() if it wants.
    this.alert(title, text);
  },

  confirm: function(title, text) {
    return this._browserElementChild.showModalPrompt(
      this._win, {promptType: "confirm", title: title, message: text});
  },

  confirmCheck: function(title, text, checkMsg, checkState) {
    return this.confirm(title, text);
  },

  confirmEx: function(title, text, buttonFlags, button0Title, button1Title, button2Title, checkMsg, checkState) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  prompt: function(title, text, value, checkMsg, checkState) {
    let rv = this._browserElementChild.showModalPrompt(
      this._win,
      { promptType: "prompt",
        title: title,
        message: text,
        initialValue: value.value,
        returnValue: null });

    value.value = rv;

    // nsIPrompt::Prompt returns true if the user pressed "OK" at the prompt,
    // and false if the user pressed "Cancel".
    //
    // BrowserElementChild returns null for "Cancel" and returns the string the
    // user entered otherwise.
    return rv !== null;
  },

  promptUsernameAndPassword: function(title, text, username, password, checkMsg, checkState) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  promptPassword: function(title, text, password, checkMsg, checkState) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  select: function(title, text, aCount, aSelectList, aOutSelection) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },
};

function BrowserElementPromptFactory(toWrap) {
  this._wrapped = toWrap;
}

BrowserElementPromptFactory.prototype = {
  classID: Components.ID("{24f3d0cf-e417-4b85-9017-c9ecf8bb1299}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPromptFactory]),

  getPrompt: function(win, iid) {
    // Try to find a browserelementchild for the window.
    if (iid.number != Ci.nsIPrompt.number) {
      debug("Falling back to wrapped prompt service because " +
            "we don't recognize the requested IID (" + iid + ", " +
            "nsIPrompt=" + Ci.nsIPrompt);
      return this._wrapped.getPrompt(win, iid);
    }

    let browserElementChild =
      BrowserElementPromptService.getBrowserElementChildForWindow(win);
    if (!browserElementChild) {
      debug("Falling back to wrapped prompt service because " +
            "we can't find a browserElementChild for " +
            win + ", " + win.location);
      return this._wrapped.getPrompt(win, iid);
    }

    debug("Returning wrapped getPrompt for " + win);
    return new BrowserElementPrompt(win, browserElementChild)
                                   .QueryInterface(iid);
  }
};

let BrowserElementPromptService = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  _init: function() {
    var os = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
    os.addObserver(this, "outer-window-destroyed", /* ownsWeak = */ true);

    // Wrap the existing @mozilla.org/prompter;1 implementation.
    var contractID = "@mozilla.org/prompter;1";
    var oldCID = Cm.contractIDToCID(contractID);
    var newCID = BrowserElementPromptFactory.prototype.classID;
    var oldFactory = Cm.getClassObject(Cc[contractID], Ci.nsIFactory);

    if (oldCID == newCID) {
      debug("WARNING: Wrapped prompt factory is already installed!");
      return;
    }

    Cm.unregisterFactory(oldCID, oldFactory);

    var oldInstance = oldFactory.createInstance(null, Ci.nsIPromptFactory);
    var newInstance = new BrowserElementPromptFactory(oldInstance);

    var newFactory = {
      createInstance: function(outer, iid) {
        if (outer != null) {
          throw Cr.NS_ERROR_NO_AGGREGATION;
        }
        return newInstance.QueryInterface(iid);
      }
    };
    Cm.registerFactory(newCID,
                       "BrowserElementPromptService's prompter;1 wrapper",
                       contractID, newFactory);

    debug("Done installing new prompt factory.");
  },

  _getOuterWindowID: function(win) {
    return win.QueryInterface(Ci.nsIInterfaceRequestor)
              .getInterface(Ci.nsIDOMWindowUtils)
              .outerWindowID;
  },

  _browserElementChildMap: {},
  mapWindowToBrowserElementChild: function(win, browserElementChild) {
    this._browserElementChildMap[this._getOuterWindowID(win)] = browserElementChild;
  },

  getBrowserElementChildForWindow: function(win) {
    return this._browserElementChildMap[this._getOuterWindowID(win)];
  },

  _observeOuterWindowDestroyed: function(outerWindowID) {
    let id = outerWindowID.QueryInterface(Ci.nsISupportsPRUint64).data;
    debug("observeOuterWindowDestroyed " + id);
    delete this._browserElementChildMap[outerWindowID.data];
  },

  observe: function(subject, topic, data) {
    switch(topic) {
    case "outer-window-destroyed":
      this._observeOuterWindowDestroyed(subject);
      break;
    default:
      debug("Observed unexpected topic " + topic);
    }
  },
};

BrowserElementPromptService._init();
