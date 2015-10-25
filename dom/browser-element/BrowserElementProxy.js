/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import('resource://gre/modules/Services.jsm');

function defineNoReturnMethod(methodName) {
  return function noReturnMethod() {
    let args = Array.slice(arguments);
    this._sendToParent(methodName, args);
  };
}

function defineDOMRequestMethod(methodName) {
  return function domRequestMethod() {
    let args = Array.slice(arguments);
    return this._sendDOMRequest(methodName, args);
  };
}

function defineUnimplementedMethod(methodName) {
  return function unimplementedMethod() {
    throw Components.Exception(
      'Unimplemented method: ' + methodName, Cr.NS_ERROR_FAILURE);
  };
}

/**
 * The BrowserElementProxy talks to the Browser IFrameElement instance on
 * behave of the embedded document. It implements all the methods on
 * the Browser IFrameElement and the methods will work if they are applicable.
 *
 * The message is forwarded to BrowserElementParent.js by creating an
 * 'browser-element-api:proxy-call' observer message.
 * BrowserElementChildPreload will get notified and send the message through
 * to the main process through sendAsyncMessage with message of the same name.
 *
 * The return message will follow the same route. The message name on the
 * return route is 'browser-element-api:proxy'.
 *
 * Both BrowserElementProxy and BrowserElementParent must be modified if there
 * is a new method implemented, or a new event added to the Browser
 * IFrameElement.
 *
 * Other details unmentioned here are checks of message sender and recipients
 * to identify proxy instance on different innerWindows or ensure the content
 * process has the right permission.
 */
function BrowserElementProxy() {
  // Pad the 0th element so that DOMRequest ID will always be a truthy value.
  this._pendingDOMRequests = [ undefined ];
}

BrowserElementProxy.prototype = {
  classDescription: 'BrowserElementProxy allowed embedded frame to control ' +
                    'it\'s own embedding browser element frame instance.',
  classID:          Components.ID('{7e95d54c-9930-49c8-9a10-44fe40fe8251}'),
  contractID:       '@mozilla.org/dom/browser-element-proxy;1',

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIDOMGlobalPropertyInitializer,
    Ci.nsIObserver]),

  _window: null,
  _innerWindowID: undefined,

  get allowedAudioChannels() {
    return this._window.navigator.mozAudioChannelManager ?
      this._window.navigator.mozAudioChannelManager.allowedAudioChannels :
      null;
  },

  init: function(win) {
    this._window = win;
    this._innerWindowID = win.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIDOMWindowUtils)
                          .currentInnerWindowID;

    this._sendToParent('_proxyInstanceInit');
    Services.obs.addObserver(this, 'browser-element-api:proxy', false);
  },

  uninit: function(win) {
    this._sendToParent('_proxyInstanceUninit');

    this._window = null;
    this._innerWindowID = undefined;

    Services.obs.removeObserver(this, 'browser-element-api:proxy');
  },

  observe: function(subject, topic, stringifedData) {
    let data = JSON.parse(stringifedData);

    if (subject !== this._window ||
        data.innerWindowID !== data.innerWindowID) {
      return;
    }

    if (data.eventName) {
      this._fireEvent(data.eventName, JSON.parse(data.eventDetailString));

      return;
    }

    if ('domRequestId' in data) {
      let req = this._pendingDOMRequests[data.domRequestId];
      this._pendingDOMRequests[data.domRequestId] = undefined;

      if (!req) {
        dump('BrowserElementProxy Error: ' +
          'Multiple observer messages for the same DOMRequest result.\n');
        return;
      }

      if ('result' in data) {
        let clientObj = Cu.cloneInto(data.result, this._window);
        Services.DOMRequest.fireSuccess(req, clientObj);
      } else {
        let clientObj = Cu.cloneInto(data.error, this._window);
        Services.DOMRequest.fireSuccess(req, clientObj);
      }

      return;
    }

    dump('BrowserElementProxy Error: ' +
          'Received unhandled observer messages ' + stringifedData + '.\n');
  },

  _sendDOMRequest: function(methodName, args) {
    let id = this._pendingDOMRequests.length;
    let req = Services.DOMRequest.createRequest(this._window);

    this._pendingDOMRequests.push(req);
    this._sendToParent(methodName, args, id);

    return req;
  },

  _sendToParent: function(methodName, args, domRequestId) {
    let data = {
      methodName: methodName,
      args: args,
      innerWindowID: this._innerWindowID
    };

    if (domRequestId) {
      data.domRequestId = domRequestId;
    }

    Services.obs.notifyObservers(
      this._window, 'browser-element-api:proxy-call', JSON.stringify(data));
  },

  _fireEvent: function(name, detail) {
    let evt = this._createEvent(name, detail,
                                /* cancelable = */ false);
    this.__DOM_IMPL__.dispatchEvent(evt);
  },

  _createEvent: function(evtName, detail, cancelable) {
    // This will have to change if we ever want to send a CustomEvent with null
    // detail.  For now, it's OK.
    if (detail !== undefined && detail !== null) {
      detail = Cu.cloneInto(detail, this._window);
      return new this._window.CustomEvent(evtName,
                                          { bubbles: false,
                                            cancelable: cancelable,
                                            detail: detail });
    }

    return new this._window.Event(evtName,
                                  { bubbles: false,
                                    cancelable: cancelable });
  },

  setVisible: defineNoReturnMethod('setVisible'),
  setActive: defineNoReturnMethod('setActive'),
  sendMouseEvent: defineNoReturnMethod('sendMouseEvent'),
  sendTouchEvent: defineNoReturnMethod('sendTouchEvent'),
  goBack: defineNoReturnMethod('goBack'),
  goForward: defineNoReturnMethod('goForward'),
  reload: defineNoReturnMethod('reload'),
  stop: defineNoReturnMethod('stop'),
  zoom: defineNoReturnMethod('zoom'),
  setNFCFocus: defineNoReturnMethod('setNFCFocus'),
  findAll: defineNoReturnMethod('findAll'),
  findNext: defineNoReturnMethod('findNext'),
  clearMatch: defineNoReturnMethod('clearMatch'),
  mute: defineNoReturnMethod('mute'),
  unmute: defineNoReturnMethod('unmute'),
  setVolume: defineNoReturnMethod('setVolume'),

  getVisible: defineDOMRequestMethod('getVisible'),
  download: defineDOMRequestMethod('download'),
  purgeHistory: defineDOMRequestMethod('purgeHistory'),
  getCanGoBack: defineDOMRequestMethod('getCanGoBack'),
  getCanGoForward: defineDOMRequestMethod('getCanGoForward'),
  getContentDimensions: defineDOMRequestMethod('getContentDimensions'),
  setInputMethodActive: defineDOMRequestMethod('setInputMethodActive'),
  executeScript: defineDOMRequestMethod('executeScript'),
  getMuted: defineDOMRequestMethod('getMuted'),
  getVolume: defineDOMRequestMethod('getVolume'),

  getActive: defineUnimplementedMethod('getActive'),
  addNextPaintListener: defineUnimplementedMethod('addNextPaintListener'),
  removeNextPaintListener: defineUnimplementedMethod('removeNextPaintListener'),
  getScreenshot: defineUnimplementedMethod('getScreenshot')
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([BrowserElementProxy]);
