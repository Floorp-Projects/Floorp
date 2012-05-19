/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let Cu = Components.utils;
let Ci = Components.interfaces;
let Cc = Components.classes;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const NS_PREFBRANCH_PREFCHANGE_TOPIC_ID = "nsPref:changed";
const BROWSER_FRAMES_ENABLED_PREF = "dom.mozBrowserFramesEnabled";

function debug(msg) {
  //dump("BrowserElementParent - " + msg + "\n");
}

/**
 * The BrowserElementParent implements one half of <iframe mozbrowser>.
 * (The other half is, unsurprisingly, BrowserElementChild.)
 *
 * We detect windows and docshells contained inside <iframe mozbrowser>s and
 * inject script to listen for certain events in the child.  We then listen to
 * messages from the child script and take appropriate action here.
 */

function BrowserElementParent() {}
BrowserElementParent.prototype = {
  classID: Components.ID("{ddeafdac-cb39-47c4-9cb8-c9027ee36d26}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  /**
   * Called on app startup, and also when the browser frames enabled pref is
   * changed.
   */
  _init: function() {
    debug("_init");

    if (this._initialized) {
      return;
    }

    // If the pref is disabled, do nothing except wait for the pref to change.
    // (This is important for tests, if nothing else.)
    if (!this._browserFramesPrefEnabled()) {
      var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
      prefs.addObserver(BROWSER_FRAMES_ENABLED_PREF, this, /* ownsWeak = */ true);
      return;
    }

    this._initialized = true;

    this._screenshotListeners = {};
    this._screenshotReqCounter = 0;

    var os = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
    os.addObserver(this, 'remote-browser-frame-shown', /* ownsWeak = */ true);
    os.addObserver(this, 'in-process-browser-frame-shown', /* ownsWeak = */ true);
  },

  _browserFramesPrefEnabled: function() {
    var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
    try {
      return prefs.getBoolPref(BROWSER_FRAMES_ENABLED_PREF);
    }
    catch(e) {
      return false;
    }
  },

  _observeInProcessBrowserFrameShown: function(frameLoader) {
    debug("In-process browser frame shown " + frameLoader);
    this._setUpMessageManagerListeners(frameLoader);
  },

  _observeRemoteBrowserFrameShown: function(frameLoader) {
    debug("Remote browser frame shown " + frameLoader);
    this._setUpMessageManagerListeners(frameLoader);
  },

  _setUpMessageManagerListeners: function(frameLoader) {
    let frameElement = frameLoader.QueryInterface(Ci.nsIFrameLoader).ownerElement;
    if (!frameElement) {
      debug("No frame element?");
      return;
    }

    let mm = frameLoader.messageManager;

    // Messages we receive are handled by functions with parameters
    // (frameElement, data), where |data| is the message manager's data object.

    let self = this;
    function addMessageListener(msg, handler) {
      mm.addMessageListener('browser-element-api:' + msg, handler.bind(self, frameElement));
    }

    addMessageListener("hello", this._recvHello);
    addMessageListener("locationchange", this._fireEventFromMsg);
    addMessageListener("loadstart", this._fireEventFromMsg);
    addMessageListener("loadend", this._fireEventFromMsg);
    addMessageListener("titlechange", this._fireEventFromMsg);
    addMessageListener("iconchange", this._fireEventFromMsg);
    addMessageListener("get-mozapp-manifest-url", this._sendMozAppManifestURL);
    mm.addMessageListener('browser-element-api:got-screenshot',
                          this._recvGotScreenshot.bind(this));

    XPCNativeWrapper.unwrap(frameElement).getScreenshot =
      this._getScreenshot.bind(this, mm, frameElement);

    mm.loadFrameScript("chrome://global/content/BrowserElementChild.js",
                       /* allowDelayedLoad = */ true);
  },

  _recvHello: function(frameElement, data) {
    debug("recvHello " + frameElement);
  },

  /**
   * Fire either a vanilla or a custom event, depending on the contents of
   * |data|.
   */
  _fireEventFromMsg: function(frameElement, data) {
    let name = data.name;
    let detail = data.json;

    debug('fireEventFromMsg: ' + name + ' ' + detail);
    let evtName = name.substring('browser-element-api:'.length);
    let win = frameElement.ownerDocument.defaultView;
    let evt;

    // This will have to change if we ever want to send a CustomEvent with null
    // detail.  For now, it's OK.
    if (detail !== undefined && detail !== null) {
      evt = new win.CustomEvent('mozbrowser' + evtName, {detail: detail});
    }
    else {
      evt = new win.Event('mozbrowser' + evtName);
    }

    frameElement.dispatchEvent(evt);
  },

  _sendMozAppManifestURL: function(frameElement, data) {
    return frameElement.getAttribute('mozapp');
  },

  _recvGotScreenshot: function(data) {
    var req = this._screenshotListeners[data.json.id];
    delete this._screenshotListeners[data.json.id];
    Services.DOMRequest.fireSuccess(req, data.json.screenshot);
  },

  _getScreenshot: function(mm, frameElement) {
    let id = 'req_' + this._screenshotReqCounter++;
    let req = Services.DOMRequest
      .createRequest(frameElement.ownerDocument.defaultView);
    this._screenshotListeners[id] = req;
    mm.sendAsyncMessage('browser-element-api:get-screenshot', {id: id});
    return req;
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
    case 'remote-browser-frame-shown':
      this._observeRemoteBrowserFrameShown(subject);
      break;
    case 'in-process-browser-frame-shown':
      this._observeInProcessBrowserFrameShown(subject);
      break;
    case 'content-document-global-created':
      this._observeContentGlobalCreated(subject);
      break;
    }
  },
};

var NSGetFactory = XPCOMUtils.generateNSGetFactory([BrowserElementParent]);
