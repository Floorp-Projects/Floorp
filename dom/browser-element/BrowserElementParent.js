/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let Cu = Components.utils;
let Ci = Components.interfaces;
let Cc = Components.classes;
let Cr = Components.results;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/BrowserElementParent.jsm");

const NS_PREFBRANCH_PREFCHANGE_TOPIC_ID = "nsPref:changed";
const BROWSER_FRAMES_ENABLED_PREF = "dom.mozBrowserFramesEnabled";

function debug(msg) {
  //dump("BrowserElementParent.js - " + msg + "\n");
}

/**
 * BrowserElementParent implements one half of <iframe mozbrowser>.  (The other
 * half is, unsurprisingly, BrowserElementChild.)
 *
 * BrowserElementParentFactory detects when we create a windows or docshell
 * contained inside a <iframe mozbrowser> and creates a BrowserElementParent
 * object for that window.
 *
 * It creates a BrowserElementParent that injects script to listen for
 * certain event.
 */

function BrowserElementParentFactory() {
  this._initialized = false;
}

BrowserElementParentFactory.prototype = {
  classID: Components.ID("{ddeafdac-cb39-47c4-9cb8-c9027ee36d26}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  /**
   * Called on app startup, and also when the browser frames enabled pref is
   * changed.
   */
  _init: function() {
    if (this._initialized) {
      return;
    }

    // If the pref is disabled, do nothing except wait for the pref to change.
    // (This is important for tests, if nothing else.)
    if (!this._browserFramesPrefEnabled()) {
      Services.prefs.addObserver(BROWSER_FRAMES_ENABLED_PREF, this, /* ownsWeak = */ true);
      return;
    }

    debug("_init");
    this._initialized = true;

    // Maps frame elements to BrowserElementParent objects.  We never look up
    // anything in this map; the purpose is to keep the BrowserElementParent
    // alive for as long as its frame element lives.
    this._bepMap = new WeakMap();

    Services.obs.addObserver(this, 'remote-browser-pending', /* ownsWeak = */ true);
    Services.obs.addObserver(this, 'inprocess-browser-shown', /* ownsWeak = */ true);
  },

  _browserFramesPrefEnabled: function() {
    try {
      return Services.prefs.getBoolPref(BROWSER_FRAMES_ENABLED_PREF);
    }
    catch(e) {
      return false;
    }
  },

  _observeInProcessBrowserFrameShown: function(frameLoader) {
    // Ignore notifications that aren't from a BrowserOrApp
    if (!frameLoader.QueryInterface(Ci.nsIFrameLoader).ownerIsBrowserOrAppFrame) {
      return;
    }
    debug("In-process browser frame shown " + frameLoader);
    this._createBrowserElementParent(frameLoader,
                                     /* hasRemoteFrame = */ false,
                                     /* pending frame */ false);
  },

  _observeRemoteBrowserFramePending: function(frameLoader) {
    // Ignore notifications that aren't from a BrowserOrApp
    if (!frameLoader.QueryInterface(Ci.nsIFrameLoader).ownerIsBrowserOrAppFrame) {
      return;
    }
    debug("Remote browser frame shown " + frameLoader);
    this._createBrowserElementParent(frameLoader,
                                     /* hasRemoteFrame = */ true,
                                     /* pending frame */ true);
  },

  _createBrowserElementParent: function(frameLoader, hasRemoteFrame, isPendingFrame) {
    let frameElement = frameLoader.QueryInterface(Ci.nsIFrameLoader).ownerElement;
    this._bepMap.set(frameElement, BrowserElementParentBuilder.create(
      frameLoader, hasRemoteFrame, isPendingFrame));
  },

  observe: function(subject, topic, data) {
    switch(topic) {
    case 'app-startup':
      this._init();
      break;
    case NS_PREFBRANCH_PREFCHANGE_TOPIC_ID:
      if (data == BROWSER_FRAMES_ENABLED_PREF) {
        this._init();
      }
      break;
    case 'remote-browser-pending':
      this._observeRemoteBrowserFramePending(subject);
      break;
    case 'inprocess-browser-shown':
      this._observeInProcessBrowserFrameShown(subject);
      break;
    }
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([BrowserElementParentFactory]);
