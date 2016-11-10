/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Cu = Components.utils;
var Ci = Components.interfaces;
var Cc = Components.classes;
var Cr = Components.results;

/* BrowserElementParent injects script to listen for certain events in the
 * child.  We then listen to messages from the child script and take
 * appropriate action here in the parent.
 */

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/BrowserElementPromptService.jsm");

function debug(msg) {
  //dump("BrowserElementParent - " + msg + "\n");
}

function getIntPref(prefName, def) {
  try {
    return Services.prefs.getIntPref(prefName);
  }
  catch(err) {
    return def;
  }
}

function handleWindowEvent(e) {
  if (this._browserElementParents) {
    let beps = ThreadSafeChromeUtils.nondeterministicGetWeakMapKeys(this._browserElementParents);
    beps.forEach(bep => bep._handleOwnerEvent(e));
  }
}

function defineNoReturnMethod(fn) {
  return function method() {
    if (!this._domRequestReady) {
      // Remote browser haven't been created, we just queue the API call.
      let args = Array.slice(arguments);
      args.unshift(this);
      this._pendingAPICalls.push(method.bind.apply(fn, args));
      return;
    }
    if (this._isAlive()) {
      fn.apply(this, arguments);
    }
  };
}

function defineDOMRequestMethod(msgName) {
  return function() {
    return this._sendDOMRequest(msgName);
  };
}

function BrowserElementParentProxyCallHandler() {
}

BrowserElementParentProxyCallHandler.prototype = {
  _frameElement: null,
  _mm: null,

  MOZBROWSER_EVENT_NAMES: Object.freeze([
    "loadstart", "loadend", "close", "error", "firstpaint",
    "documentfirstpaint", "audioplaybackchange",
    "contextmenu", "securitychange", "locationchange",
    "iconchange", "scrollareachanged", "titlechange",
    "opensearch", "manifestchange", "metachange",
    "resize", "scrollviewchange",
    "caretstatechanged", "activitydone", "scroll", "opentab"]),

  init: function(frameElement, mm) {
    this._frameElement = frameElement;
    this._mm = mm;
    this.innerWindowIDSet = new Set();

    mm.addMessageListener("browser-element-api:proxy-call", this);
  },

  // Message manager callback receives messages from BrowserElementProxy.js
  receiveMessage: function(mmMsg) {
    let data = mmMsg.json;

    let mm;
    try {
      mm = mmMsg.target.QueryInterface(Ci.nsIFrameLoaderOwner)
                     .frameLoader.messageManager;
    } catch(e) {
      mm = mmMsg.target;
    }
    if (!mm.assertPermission("browser:embedded-system-app")) {
      dump("BrowserElementParent.js: Method call " + data.methodName +
        " from a content process with no 'browser:embedded-system-app'" +
        " privileges.\n");
      return;
    }

    switch (data.methodName) {
      case '_proxyInstanceInit':
        if (!this.innerWindowIDSet.size) {
          this._attachEventListeners();
        }
        this.innerWindowIDSet.add(data.innerWindowID);

        break;

      case '_proxyInstanceUninit':
        this.innerWindowIDSet.delete(data.innerWindowID);
        if (!this.innerWindowIDSet.size) {
          this._detachEventListeners();
        }

        break;

      // void methods
      case 'setVisible':
      case 'setActive':
      case 'sendMouseEvent':
      case 'sendTouchEvent':
      case 'goBack':
      case 'goForward':
      case 'reload':
      case 'stop':
      case 'zoom':
      case 'findAll':
      case 'findNext':
      case 'clearMatch':
      case 'mute':
      case 'unmute':
      case 'setVolume':
        this._frameElement[data.methodName]
          .apply(this._frameElement, data.args);

        break;

      // DOMRequest methods
      case 'getVisible':
      case 'download':
      case 'purgeHistory':
      case 'getCanGoBack':
      case 'getCanGoForward':
      case 'getContentDimensions':
      case 'setInputMethodActive':
      case 'executeScript':
      case 'getMuted':
      case 'getVolume':
        let req = this._frameElement[data.methodName]
          .apply(this._frameElement, data.args);
        req.onsuccess = () => {
          this._sendToProxy({
            domRequestId: data.domRequestId,
            innerWindowID: data.innerWindowID,
            result: req.result
          });
        };
        req.onerror = () => {
          this._sendToProxy({
            domRequestId: data.domRequestId,
            innerWindowID: data.innerWindowID,
            err: req.error
          });
        };

        break;

      // Not implemented
      case 'getActive': // Sync ???
      case 'addNextPaintListener': // Takes a callback
      case 'removeNextPaintListener': // Takes a callback
      case 'getScreenshot': // Need to pass a blob back
        dump("BrowserElementParentProxyCallHandler Error:" +
          "Attempt to call unimplemented method " + data.methodName + ".\n");
        break;

      default:
        dump("BrowserElementParentProxyCallHandler Error:" +
          "Attempt to call non-exist method " + data.methodName + ".\n");
        break;
    }
  },

  // Receving events from the frame element and forward it.
  handleEvent: function(evt) {
    // Ignore the events from nested mozbrowser iframes
    if (evt.target !== this._frameElement) {
      return;
    }

    let detailString;
    try {
      detailString = JSON.stringify(evt.detail);
    } catch (e) {
      dump("BrowserElementParentProxyCallHandler Error:" +
        "Event detail of " + evt.type + " can't be stingified.\n");
      return;
    }

    this.innerWindowIDSet.forEach((innerWindowID) => {
      this._sendToProxy({
        eventName: evt.type,
        innerWindowID: innerWindowID,
        eventDetailString: detailString
      });
    });
  },

  _sendToProxy: function(data) {
    this._mm.sendAsyncMessage("browser-element-api:proxy", data);
  },

  _attachEventListeners: function() {
    this.MOZBROWSER_EVENT_NAMES.forEach(function(eventName) {
      this._frameElement.addEventListener(
        "mozbrowser" + eventName, this, true);
    }, this);
  },

  _detachEventListeners: function() {
    this.MOZBROWSER_EVENT_NAMES.forEach(function(eventName) {
      this._frameElement.removeEventListener(
        "mozbrowser" + eventName, this, true);
    }, this);
  }
};

function BrowserElementParent() {
  debug("Creating new BrowserElementParent object");
  this._domRequestCounter = 0;
  this._domRequestReady = false;
  this._pendingAPICalls = [];
  this._pendingDOMRequests = {};
  this._pendingSetInputMethodActive = [];
  this._nextPaintListeners = [];
  this._pendingDOMFullscreen = false;

  Services.obs.addObserver(this, 'oop-frameloader-crashed', /* ownsWeak = */ true);
  Services.obs.addObserver(this, 'ask-children-to-execute-copypaste-command', /* ownsWeak = */ true);
  Services.obs.addObserver(this, 'back-docommand', /* ownsWeak = */ true);

  this.proxyCallHandler = new BrowserElementParentProxyCallHandler();
}

BrowserElementParent.prototype = {

  classDescription: "BrowserElementAPI implementation",
  classID: Components.ID("{9f171ac4-0939-4ef8-b360-3408aedc3060}"),
  contractID: "@mozilla.org/dom/browser-element-api;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIBrowserElementAPI,
                                         Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  setFrameLoader: function(frameLoader) {
    debug("Setting frameLoader");
    this._frameLoader = frameLoader;
    this._frameElement = frameLoader.QueryInterface(Ci.nsIFrameLoader).ownerElement;
    if (!this._frameElement) {
      debug("No frame element?");
      return;
    }
    // Listen to visibilitychange on the iframe's owner window, and forward
    // changes down to the child.  We want to do this while registering as few
    // visibilitychange listeners on _window as possible, because such a listener
    // may live longer than this BrowserElementParent object.
    //
    // To accomplish this, we register just one listener on the window, and have
    // it reference a WeakMap whose keys are all the BrowserElementParent objects
    // on the window.  Then when the listener fires, we iterate over the
    // WeakMap's keys (which we can do, because we're chrome) to notify the
    // BrowserElementParents.
    if (!this._window._browserElementParents) {
      this._window._browserElementParents = new WeakMap();
      let handler = handleWindowEvent.bind(this._window);
      let windowEvents = ['visibilitychange', 'fullscreenchange'];
      let els = Cc["@mozilla.org/eventlistenerservice;1"]
                  .getService(Ci.nsIEventListenerService);
      for (let event of windowEvents) {
        els.addSystemEventListener(this._window, event, handler,
                                   /* useCapture = */ true);
      }
    }

    this._window._browserElementParents.set(this, null);

    // Insert ourself into the prompt service.
    BrowserElementPromptService.mapFrameToBrowserElementParent(this._frameElement, this);
    this._setupMessageListener();

    this.proxyCallHandler.init(
      this._frameElement, this._frameLoader.messageManager);
  },

  destroyFrameScripts() {
    debug("Destroying frame scripts");
    this._mm.sendAsyncMessage("browser-element-api:destroy");
  },

  _runPendingAPICall: function() {
    if (!this._pendingAPICalls) {
      return;
    }
    for (let i = 0; i < this._pendingAPICalls.length; i++) {
      try {
        this._pendingAPICalls[i]();
      } catch (e) {
        // throw the expections from pending functions.
        debug('Exception when running pending API call: ' +  e);
      }
    }
    delete this._pendingAPICalls;
  },

  _setupMessageListener: function() {
    this._mm = this._frameLoader.messageManager;
    this._mm.addMessageListener('browser-element-api:call', this);
  },

  receiveMessage: function(aMsg) {
    if (!this._isAlive()) {
      return;
    }

    // Messages we receive are handed to functions which take a (data) argument,
    // where |data| is the message manager's data object.
    // We use a single message and dispatch to various function based
    // on data.msg_name
    let mmCalls = {
      "hello": this._recvHello,
      "loadstart": this._fireProfiledEventFromMsg,
      "loadend": this._fireProfiledEventFromMsg,
      "close": this._fireEventFromMsg,
      "error": this._fireEventFromMsg,
      "firstpaint": this._fireProfiledEventFromMsg,
      "documentfirstpaint": this._fireProfiledEventFromMsg,
      "nextpaint": this._recvNextPaint,
      "got-purge-history": this._gotDOMRequestResult,
      "got-screenshot": this._gotDOMRequestResult,
      "got-contentdimensions": this._gotDOMRequestResult,
      "got-can-go-back": this._gotDOMRequestResult,
      "got-can-go-forward": this._gotDOMRequestResult,
      "got-muted": this._gotDOMRequestResult,
      "got-volume": this._gotDOMRequestResult,
      "requested-dom-fullscreen": this._requestedDOMFullscreen,
      "fullscreen-origin-change": this._fullscreenOriginChange,
      "exit-dom-fullscreen": this._exitDomFullscreen,
      "got-visible": this._gotDOMRequestResult,
      "visibilitychange": this._childVisibilityChange,
      "got-set-input-method-active": this._gotDOMRequestResult,
      "scrollviewchange": this._handleScrollViewChange,
      "caretstatechanged": this._handleCaretStateChanged,
      "findchange": this._handleFindChange,
      "execute-script-done": this._gotDOMRequestResult,
      "got-audio-channel-volume": this._gotDOMRequestResult,
      "got-set-audio-channel-volume": this._gotDOMRequestResult,
      "got-audio-channel-muted": this._gotDOMRequestResult,
      "got-set-audio-channel-muted": this._gotDOMRequestResult,
      "got-is-audio-channel-active": this._gotDOMRequestResult,
      "got-web-manifest": this._gotDOMRequestResult,
    };

    let mmSecuritySensitiveCalls = {
      "audioplaybackchange": this._fireEventFromMsg,
      "showmodalprompt": this._handleShowModalPrompt,
      "contextmenu": this._fireCtxMenuEvent,
      "securitychange": this._fireEventFromMsg,
      "locationchange": this._fireEventFromMsg,
      "iconchange": this._fireEventFromMsg,
      "scrollareachanged": this._fireEventFromMsg,
      "titlechange": this._fireProfiledEventFromMsg,
      "opensearch": this._fireEventFromMsg,
      "manifestchange": this._fireEventFromMsg,
      "metachange": this._fireEventFromMsg,
      "resize": this._fireEventFromMsg,
      "activitydone": this._fireEventFromMsg,
      "scroll": this._fireEventFromMsg,
      "opentab": this._fireEventFromMsg
    };

    if (aMsg.data.msg_name in mmCalls) {
      return mmCalls[aMsg.data.msg_name].apply(this, arguments);
    } else if (aMsg.data.msg_name in mmSecuritySensitiveCalls) {
      return mmSecuritySensitiveCalls[aMsg.data.msg_name].apply(this, arguments);
    }
  },

  _removeMessageListener: function() {
    this._mm.removeMessageListener('browser-element-api:call', this);
  },

  /**
   * You shouldn't touch this._frameElement or this._window if _isAlive is
   * false.  (You'll likely get an exception if you do.)
   */
  _isAlive: function() {
    return !Cu.isDeadWrapper(this._frameElement) &&
           !Cu.isDeadWrapper(this._frameElement.ownerDocument) &&
           !Cu.isDeadWrapper(this._frameElement.ownerDocument.defaultView);
  },

  get _window() {
    return this._frameElement.ownerDocument.defaultView;
  },

  get _windowUtils() {
    return this._window.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDOMWindowUtils);
  },

  promptAuth: function(authDetail, callback) {
    let evt;
    let self = this;
    let callbackCalled = false;
    let cancelCallback = function() {
      if (!callbackCalled) {
        callbackCalled = true;
        callback(false, null, null);
      }
    };

    // We don't handle password-only prompts.
    if (authDetail.isOnlyPassword) {
      cancelCallback();
      return;
    }

    /* username and password */
    let detail = {
      host:     authDetail.host,
      path:     authDetail.path,
      realm:    authDetail.realm,
      isProxy:  authDetail.isProxy
    };

    evt = this._createEvent('usernameandpasswordrequired', detail,
                            /* cancelable */ true);
    Cu.exportFunction(function(username, password) {
      if (callbackCalled)
        return;
      callbackCalled = true;
      callback(true, username, password);
    }, evt.detail, { defineAs: 'authenticate' });

    Cu.exportFunction(cancelCallback, evt.detail, { defineAs: 'cancel' });

    this._frameElement.dispatchEvent(evt);

    if (!evt.defaultPrevented) {
      cancelCallback();
    }
  },

  _sendAsyncMsg: function(msg, data) {
    try {
      if (!data) {
        data = { };
      }

      data.msg_name = msg;
      this._mm.sendAsyncMessage('browser-element-api:call', data);
    } catch (e) {
      return false;
    }
    return true;
  },

  _recvHello: function() {
    debug("recvHello");

    // Inform our child if our owner element's document is invisible.  Note
    // that we must do so here, rather than in the BrowserElementParent
    // constructor, because the BrowserElementChild may not be initialized when
    // we run our constructor.
    if (this._window.document.hidden) {
      this._ownerVisibilityChange();
    }

    if (!this._domRequestReady) {
      // At least, one message listener such as for hello is registered.
      // So we can use sendAsyncMessage now.
      this._domRequestReady = true;
      this._runPendingAPICall();
    }
  },

  _fireCtxMenuEvent: function(data) {
    let detail = data.json;
    let evtName = detail.msg_name;

    debug('fireCtxMenuEventFromMsg: ' + evtName + ' ' + detail);
    let evt = this._createEvent(evtName, detail, /* cancellable */ true);

    if (detail.contextmenu) {
      var self = this;
      Cu.exportFunction(function(id) {
        self._sendAsyncMsg('fire-ctx-callback', {menuitem: id});
      }, evt.detail, { defineAs: 'contextMenuItemSelected' });
    }

    // The embedder may have default actions on context menu events, so
    // we fire a context menu event even if the child didn't define a
    // custom context menu
    return !this._frameElement.dispatchEvent(evt);
  },

  /**
   * add profiler marker for each event fired.
   */
  _fireProfiledEventFromMsg: function(data) {
    if (Services.profiler !== undefined) {
      Services.profiler.AddMarker(data.json.msg_name);
    }
    this._fireEventFromMsg(data);
  },

  /**
   * Fire either a vanilla or a custom event, depending on the contents of
   * |data|.
   */
  _fireEventFromMsg: function(data) {
    let detail = data.json;
    let name = detail.msg_name;

    // For events that send a "_payload_" property, we just want to transmit
    // this in the event.
    if ("_payload_" in detail) {
      detail = detail._payload_;
    }

    debug('fireEventFromMsg: ' + name + ', ' + JSON.stringify(detail));
    let evt = this._createEvent(name, detail,
                                /* cancelable = */ false);
    this._frameElement.dispatchEvent(evt);
  },

  _handleShowModalPrompt: function(data) {
    // Fire a showmodalprmopt event on the iframe.  When this method is called,
    // the child is spinning in a nested event loop waiting for an
    // unblock-modal-prompt message.
    //
    // If the embedder calls preventDefault() on the showmodalprompt event,
    // we'll block the child until event.detail.unblock() is called.
    //
    // Otherwise, if preventDefault() is not called, we'll send the
    // unblock-modal-prompt message to the child as soon as the event is done
    // dispatching.

    let detail = data.json;
    debug('handleShowPrompt ' + JSON.stringify(detail));

    // Strip off the windowID property from the object we send along in the
    // event.
    let windowID = detail.windowID;
    delete detail.windowID;
    debug("Event will have detail: " + JSON.stringify(detail));
    let evt = this._createEvent('showmodalprompt', detail,
                                /* cancelable = */ true);

    let self = this;
    let unblockMsgSent = false;
    function sendUnblockMsg() {
      if (unblockMsgSent) {
        return;
      }
      unblockMsgSent = true;

      // We don't need to sanitize evt.detail.returnValue (e.g. converting the
      // return value of confirm() to a boolean); Gecko does that for us.

      let data = { windowID: windowID,
                   returnValue: evt.detail.returnValue };
      self._sendAsyncMsg('unblock-modal-prompt', data);
    }

    Cu.exportFunction(sendUnblockMsg, evt.detail, { defineAs: 'unblock' });

    this._frameElement.dispatchEvent(evt);

    if (!evt.defaultPrevented) {
      // Unblock the inner frame immediately.  Otherwise we'll unblock upon
      // evt.detail.unblock().
      sendUnblockMsg();
    }
  },

  // Called when state of accessible caret in child has changed.
  // The fields of data is as following:
  //  - rect: Contains bounding rectangle of selection, Include width, height,
  //          top, bottom, left and right.
  //  - commands: Describe what commands can be executed in child. Include canSelectAll,
  //              canCut, canCopy and canPaste. For example: if we want to check if cut
  //              command is available, using following code, if (data.commands.canCut) {}.
  //  - zoomFactor: Current zoom factor in child frame.
  //  - reason: The reason causes the state changed. Include "visibilitychange",
  //            "updateposition", "longpressonemptycontent", "taponcaret", "presscaret",
  //            "releasecaret".
  //  - collapsed: Indicate current selection is collapsed or not.
  //  - caretVisible: Indicate the caret visiibility.
  //  - selectionVisible: Indicate current selection is visible or not.
  //  - selectionEditable: Indicate current selection is editable or not.
  //  - selectedTextContent: Contains current selected text content, which is
  //                         equivalent to the string returned by Selection.toString().
  _handleCaretStateChanged: function(data) {
    let evt = this._createEvent('caretstatechanged', data.json,
                                /* cancelable = */ false);

    let self = this;
    function sendDoCommandMsg(cmd) {
      let data = { command: cmd };
      self._sendAsyncMsg('copypaste-do-command', data);
    }
    Cu.exportFunction(sendDoCommandMsg, evt.detail, { defineAs: 'sendDoCommandMsg' });

    this._frameElement.dispatchEvent(evt);
  },

  _handleScrollViewChange: function(data) {
    let evt = this._createEvent("scrollviewchange", data.json,
                                /* cancelable = */ false);
    this._frameElement.dispatchEvent(evt);
  },

  _handleFindChange: function(data) {
    let evt = this._createEvent("findchange", data.json,
                                /* cancelable = */ false);
    this._frameElement.dispatchEvent(evt);
  },

  _createEvent: function(evtName, detail, cancelable) {
    // This will have to change if we ever want to send a CustomEvent with null
    // detail.  For now, it's OK.
    if (detail !== undefined && detail !== null) {
      detail = Cu.cloneInto(detail, this._window);
      return new this._window.CustomEvent('mozbrowser' + evtName,
                                          { bubbles: true,
                                            cancelable: cancelable,
                                            detail: detail });
    }

    return new this._window.Event('mozbrowser' + evtName,
                                  { bubbles: true,
                                    cancelable: cancelable });
  },

  /**
   * Kick off a DOMRequest in the child process.
   *
   * We'll fire an event called |msgName| on the child process, passing along
   * an object with two fields:
   *
   *  - id:  the ID of this request.
   *  - arg: arguments to pass to the child along with this request.
   *
   * We expect the child to pass the ID back to us upon completion of the
   * request.  See _gotDOMRequestResult.
   */
  _sendDOMRequest: function(msgName, args) {
    let id = 'req_' + this._domRequestCounter++;
    let req = Services.DOMRequest.createRequest(this._window);
    let self = this;
    let send = function() {
      if (!self._isAlive()) {
        return;
      }
      if (self._sendAsyncMsg(msgName, {id: id, args: args})) {
        self._pendingDOMRequests[id] = req;
      } else {
        Services.DOMRequest.fireErrorAsync(req, "fail");
      }
    };
    if (this._domRequestReady) {
      send();
    } else {
      // Child haven't been loaded.
      this._pendingAPICalls.push(send);
    }
    return req;
  },

  /**
   * Called when the child process finishes handling a DOMRequest.  data.json
   * must have the fields [id, successRv], if the DOMRequest was successful, or
   * [id, errorMsg], if the request was not successful.
   *
   * The fields have the following meanings:
   *
   *  - id:        the ID of the DOM request (see _sendDOMRequest)
   *  - successRv: the request's return value, if the request succeeded
   *  - errorMsg:  the message to pass to DOMRequest.fireError(), if the request
   *               failed.
   *
   */
  _gotDOMRequestResult: function(data) {
    let req = this._pendingDOMRequests[data.json.id];
    delete this._pendingDOMRequests[data.json.id];

    if ('successRv' in data.json) {
      debug("Successful gotDOMRequestResult.");
      let clientObj = Cu.cloneInto(data.json.successRv, this._window);
      Services.DOMRequest.fireSuccess(req, clientObj);
    }
    else {
      debug("Got error in gotDOMRequestResult.");
      Services.DOMRequest.fireErrorAsync(req,
        Cu.cloneInto(data.json.errorMsg, this._window));
    }
  },

  setVisible: defineNoReturnMethod(function(visible) {
    this._sendAsyncMsg('set-visible', {visible: visible});
    this._frameLoader.visible = visible;
  }),

  getVisible: defineDOMRequestMethod('get-visible'),

  setActive: defineNoReturnMethod(function(active) {
    this._frameLoader.visible = active;
  }),

  getActive: function() {
    if (!this._isAlive()) {
      throw Components.Exception("Dead content process",
                                 Cr.NS_ERROR_DOM_INVALID_STATE_ERR);
    }

    return this._frameLoader.visible;
  },

  getChildProcessOffset: function() {
    let offset = { x: 0, y: 0 };
    let tabParent = this._frameLoader.tabParent;
    if (tabParent) {
      let offsetX = {};
      let offsetY = {};
      tabParent.getChildProcessOffset(offsetX, offsetY);
      offset.x = offsetX.value;
      offset.y = offsetY.value;
    }
    return offset;
  },

  sendMouseEvent: defineNoReturnMethod(function(type, x, y, button, clickCount, modifiers) {
    let offset = this.getChildProcessOffset();
    x += offset.x;
    y += offset.y;

    this._sendAsyncMsg("send-mouse-event", {
      "type": type,
      "x": x,
      "y": y,
      "button": button,
      "clickCount": clickCount,
      "modifiers": modifiers
    });
  }),

  sendTouchEvent: defineNoReturnMethod(function(type, identifiers, touchesX, touchesY,
                                                radiisX, radiisY, rotationAngles, forces,
                                                count, modifiers) {

    let offset = this.getChildProcessOffset();
    for (var i = 0; i < touchesX.length; i++) {
      touchesX[i] += offset.x;
    }
    for (var i = 0; i < touchesY.length; i++) {
      touchesY[i] += offset.y;
    }
    this._sendAsyncMsg("send-touch-event", {
      "type": type,
      "identifiers": identifiers,
      "touchesX": touchesX,
      "touchesY": touchesY,
      "radiisX": radiisX,
      "radiisY": radiisY,
      "rotationAngles": rotationAngles,
      "forces": forces,
      "count": count,
      "modifiers": modifiers
    });
  }),

  getCanGoBack: defineDOMRequestMethod('get-can-go-back'),
  getCanGoForward: defineDOMRequestMethod('get-can-go-forward'),
  getContentDimensions: defineDOMRequestMethod('get-contentdimensions'),

  findAll: defineNoReturnMethod(function(searchString, caseSensitivity) {
    return this._sendAsyncMsg('find-all', {
      searchString,
      caseSensitive: caseSensitivity == Ci.nsIBrowserElementAPI.FIND_CASE_SENSITIVE
    });
  }),

  findNext: defineNoReturnMethod(function(direction) {
    return this._sendAsyncMsg('find-next', {
      backward: direction == Ci.nsIBrowserElementAPI.FIND_BACKWARD
    });
  }),

  clearMatch: defineNoReturnMethod(function() {
    return this._sendAsyncMsg('clear-match');
  }),

  mute: defineNoReturnMethod(function() {
    this._sendAsyncMsg('mute');
  }),

  unmute: defineNoReturnMethod(function() {
    this._sendAsyncMsg('unmute');
  }),

  getMuted: defineDOMRequestMethod('get-muted'),

  getVolume: defineDOMRequestMethod('get-volume'),

  setVolume: defineNoReturnMethod(function(volume) {
    this._sendAsyncMsg('set-volume', {volume});
  }),

  goBack: defineNoReturnMethod(function() {
    this._sendAsyncMsg('go-back');
  }),

  goForward: defineNoReturnMethod(function() {
    this._sendAsyncMsg('go-forward');
  }),

  reload: defineNoReturnMethod(function(hardReload) {
    this._sendAsyncMsg('reload', {hardReload: hardReload});
  }),

  stop: defineNoReturnMethod(function() {
    this._sendAsyncMsg('stop');
  }),

  executeScript: function(script, options) {
    if (!this._isAlive()) {
      throw Components.Exception("Dead content process",
                                 Cr.NS_ERROR_DOM_INVALID_STATE_ERR);
    }

    // Enforcing options.url or options.origin
    if (!options.url && !options.origin) {
      throw Components.Exception("Invalid argument", Cr.NS_ERROR_INVALID_ARG);
    }
    return this._sendDOMRequest('execute-script', {script, options});
  },

  /*
   * The valid range of zoom scale is defined in preference "zoom.maxPercent" and "zoom.minPercent".
   */
  zoom: defineNoReturnMethod(function(zoom) {
    zoom *= 100;
    zoom = Math.min(getIntPref("zoom.maxPercent", 300), zoom);
    zoom = Math.max(getIntPref("zoom.minPercent", 50), zoom);
    this._sendAsyncMsg('zoom', {zoom: zoom / 100.0});
  }),

  purgeHistory: defineDOMRequestMethod('purge-history'),


  download: function(_url, _options) {
    if (!this._isAlive()) {
      return null;
    }

    let uri = Services.io.newURI(_url, null, null);
    let url = uri.QueryInterface(Ci.nsIURL);

    debug('original _options = ' + uneval(_options));

    // Ensure we have _options, we always use it to send the filename.
    _options = _options || {};
    if (!_options.filename) {
      _options.filename = url.fileName;
    }

    debug('final _options = ' + uneval(_options));

    // Ensure we have a filename.
    if (!_options.filename) {
      throw Components.Exception("Invalid argument", Cr.NS_ERROR_INVALID_ARG);
    }

    let interfaceRequestor =
      this._frameLoader.loadContext.QueryInterface(Ci.nsIInterfaceRequestor);
    let req = Services.DOMRequest.createRequest(this._window);

    function DownloadListener() {
      debug('DownloadListener Constructor');
    }
    DownloadListener.prototype = {
      extListener: null,
      onStartRequest: function(aRequest, aContext) {
        debug('DownloadListener - onStartRequest');
        let extHelperAppSvc =
          Cc['@mozilla.org/uriloader/external-helper-app-service;1'].
          getService(Ci.nsIExternalHelperAppService);
        let channel = aRequest.QueryInterface(Ci.nsIChannel);

        // First, we'll ensure the filename doesn't have any leading
        // periods. We have to do it here to avoid ending up with a filename
        // that's only an extension with no extension (e.g. Sending in
        // '.jpeg' without stripping the '.' would result in a filename of
        // 'jpeg' where we want 'jpeg.jpeg'.
        _options.filename = _options.filename.replace(/^\.+/, "");

        let ext = null;
        let mimeSvc = extHelperAppSvc.QueryInterface(Ci.nsIMIMEService);
        try {
          ext = '.' + mimeSvc.getPrimaryExtension(channel.contentType, '');
        } catch (e) { ext = null; }

        // Check if we need to add an extension to the filename.
        if (ext && !_options.filename.endsWith(ext)) {
          _options.filename += ext;
        }
        // Set the filename to use when saving to disk.
        channel.contentDispositionFilename = _options.filename;

        this.extListener =
          extHelperAppSvc.doContent(
              channel.contentType,
              aRequest,
              interfaceRequestor,
              true);
        this.extListener.onStartRequest(aRequest, aContext);
      },
      onStopRequest: function(aRequest, aContext, aStatusCode) {
        debug('DownloadListener - onStopRequest (aStatusCode = ' +
               aStatusCode + ')');
        if (aStatusCode == Cr.NS_OK) {
          // Everything looks great.
          debug('DownloadListener - Download Successful.');
          Services.DOMRequest.fireSuccess(req, aStatusCode);
        }
        else {
          // In case of failure, we'll simply return the failure status code.
          debug('DownloadListener - Download Failed!');
          Services.DOMRequest.fireError(req, aStatusCode);
        }

        if (this.extListener) {
          this.extListener.onStopRequest(aRequest, aContext, aStatusCode);
        }
      },
      onDataAvailable: function(aRequest, aContext, aInputStream,
                                aOffset, aCount) {
        this.extListener.onDataAvailable(aRequest, aContext, aInputStream,
                                         aOffset, aCount);
      },
      QueryInterface: XPCOMUtils.generateQI([Ci.nsIStreamListener,
                                             Ci.nsIRequestObserver])
    };

    let referrer = Services.io.newURI(_options.referrer, null, null);
    let principal =
      Services.scriptSecurityManager.createCodebasePrincipal(
        referrer, this._frameLoader.loadContext.originAttributes);

    let channel = NetUtil.newChannel({
      uri: url,
      loadingPrincipal: principal,
      securityFlags: SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS,
      contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER
    });

    // XXX We would set private browsing information prior to calling this.
    channel.notificationCallbacks = interfaceRequestor;

    // Since we're downloading our own local copy we'll want to bypass the
    // cache and local cache if the channel let's us specify this.
    let flags = Ci.nsIChannel.LOAD_CALL_CONTENT_SNIFFERS |
                Ci.nsIChannel.LOAD_BYPASS_CACHE;
    if (channel instanceof Ci.nsICachingChannel) {
      debug('This is a caching channel. Forcing bypass.');
      flags |= Ci.nsICachingChannel.LOAD_BYPASS_LOCAL_CACHE_IF_BUSY;
    }

    channel.loadFlags |= flags;

    if (channel instanceof Ci.nsIHttpChannel) {
      debug('Setting HTTP referrer = ' + (referrer && referrer.spec));
      channel.referrer = referrer;
      if (channel instanceof Ci.nsIHttpChannelInternal) {
        channel.forceAllowThirdPartyCookie = true;
      }
    }

    // Set-up complete, let's get things started.
    channel.asyncOpen2(new DownloadListener());

    return req;
  },

  getScreenshot: function(_width, _height, _mimeType) {
    if (!this._isAlive()) {
      throw Components.Exception("Dead content process",
                                 Cr.NS_ERROR_DOM_INVALID_STATE_ERR);
    }

    let width = parseInt(_width);
    let height = parseInt(_height);
    let mimeType = (typeof _mimeType === 'string') ?
      _mimeType.trim().toLowerCase() : 'image/jpeg';
    if (isNaN(width) || isNaN(height) || width < 0 || height < 0) {
      throw Components.Exception("Invalid argument",
                                 Cr.NS_ERROR_INVALID_ARG);
    }

    return this._sendDOMRequest('get-screenshot',
                                {width: width, height: height,
                                 mimeType: mimeType});
  },

  _recvNextPaint: function(data) {
    let listeners = this._nextPaintListeners;
    this._nextPaintListeners = [];
    for (let listener of listeners) {
      try {
        listener.recvNextPaint();
      } catch (e) {
        // If a listener throws we'll continue.
      }
    }
  },

  addNextPaintListener: function(listener) {
    if (!this._isAlive()) {
      throw Components.Exception("Dead content process",
                                 Cr.NS_ERROR_DOM_INVALID_STATE_ERR);
    }

    let self = this;
    let run = function() {
      if (self._nextPaintListeners.push(listener) == 1)
        self._sendAsyncMsg('activate-next-paint-listener');
    };
    if (!this._domRequestReady) {
      this._pendingAPICalls.push(run);
    } else {
      run();
    }
  },

  removeNextPaintListener: function(listener) {
    if (!this._isAlive()) {
      throw Components.Exception("Dead content process",
                                 Cr.NS_ERROR_DOM_INVALID_STATE_ERR);
    }

    let self = this;
    let run = function() {
      for (let i = self._nextPaintListeners.length - 1; i >= 0; i--) {
        if (self._nextPaintListeners[i] == listener) {
          self._nextPaintListeners.splice(i, 1);
          break;
        }
      }

      if (self._nextPaintListeners.length == 0)
        self._sendAsyncMsg('deactivate-next-paint-listener');
    };
    if (!this._domRequestReady) {
      this._pendingAPICalls.push(run);
    } else {
      run();
    }
  },

  setInputMethodActive: function(isActive) {
    if (!this._isAlive()) {
      throw Components.Exception("Dead content process",
                                 Cr.NS_ERROR_DOM_INVALID_STATE_ERR);
    }

    if (typeof isActive !== 'boolean') {
      throw Components.Exception("Invalid argument",
                                 Cr.NS_ERROR_INVALID_ARG);
    }

    return this._sendDOMRequest('set-input-method-active',
                                {isActive: isActive});
  },

  getAudioChannelVolume: function(aAudioChannel) {
    return this._sendDOMRequest('get-audio-channel-volume',
                                {audioChannel: aAudioChannel});
  },

  setAudioChannelVolume: function(aAudioChannel, aVolume) {
    return this._sendDOMRequest('set-audio-channel-volume',
                                {audioChannel: aAudioChannel,
                                 volume: aVolume});
  },

  getAudioChannelMuted: function(aAudioChannel) {
    return this._sendDOMRequest('get-audio-channel-muted',
                                {audioChannel: aAudioChannel});
  },

  setAudioChannelMuted: function(aAudioChannel, aMuted) {
    return this._sendDOMRequest('set-audio-channel-muted',
                                {audioChannel: aAudioChannel,
                                 muted: aMuted});
  },

  isAudioChannelActive: function(aAudioChannel) {
    return this._sendDOMRequest('get-is-audio-channel-active',
                                {audioChannel: aAudioChannel});
  },

  getWebManifest: defineDOMRequestMethod('get-web-manifest'),
  /**
   * Called when the visibility of the window which owns this iframe changes.
   */
  _ownerVisibilityChange: function() {
    this._sendAsyncMsg('owner-visibility-change',
                       {visible: !this._window.document.hidden});
  },

  /*
   * Called when the child notices that its visibility has changed.
   *
   * This is sometimes redundant; for example, the child's visibility may
   * change in response to a setVisible request that we made here!  But it's
   * not always redundant; for example, the child's visibility may change in
   * response to its parent docshell being hidden.
   */
  _childVisibilityChange: function(data) {
    debug("_childVisibilityChange(" + data.json.visible + ")");
    this._frameLoader.visible = data.json.visible;

    this._fireEventFromMsg(data);
  },

  _requestedDOMFullscreen: function() {
    this._pendingDOMFullscreen = true;
    this._windowUtils.remoteFrameFullscreenChanged(this._frameElement);
  },

  _fullscreenOriginChange: function(data) {
    Services.obs.notifyObservers(
      this._frameElement, "fullscreen-origin-change", data.json.originNoSuffix);
  },

  _exitDomFullscreen: function(data) {
    this._windowUtils.remoteFrameFullscreenReverted();
  },

  _handleOwnerEvent: function(evt) {
    switch (evt.type) {
      case 'visibilitychange':
        this._ownerVisibilityChange();
        break;
      case 'fullscreenchange':
        if (!this._window.document.fullscreenElement) {
          this._sendAsyncMsg('exit-fullscreen');
        } else if (this._pendingDOMFullscreen) {
          this._pendingDOMFullscreen = false;
          this._sendAsyncMsg('entered-fullscreen');
        }
        break;
    }
  },

  _fireFatalError: function() {
    let evt = this._createEvent('error', {type: 'fatal'},
                                /* cancelable = */ false);
    this._frameElement.dispatchEvent(evt);
  },

  observe: function(subject, topic, data) {
    switch(topic) {
    case 'oop-frameloader-crashed':
      if (this._isAlive() && subject == this._frameLoader) {
        this._fireFatalError();
      }
      break;
    case 'ask-children-to-execute-copypaste-command':
      if (this._isAlive() && this._frameElement == subject.wrappedJSObject) {
        this._sendAsyncMsg('copypaste-do-command', { command: data });
      }
      break;
    case 'back-docommand':
      if (this._isAlive() && this._frameLoader.visible) {
          this.goBack();
      }
      break;
    default:
      debug('Unknown topic: ' + topic);
      break;
    };
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([BrowserElementParent]);
