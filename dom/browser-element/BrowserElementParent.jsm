/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let Cu = Components.utils;
let Ci = Components.interfaces;
let Cc = Components.classes;
let Cr = Components.results;

/* BrowserElementParent injects script to listen for certain events in the
 * child.  We then listen to messages from the child script and take
 * appropriate action here in the parent.
 */

this.EXPORTED_SYMBOLS = ["BrowserElementParentBuilder"];

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/BrowserElementPromptService.jsm");

XPCOMUtils.defineLazyGetter(this, "DOMApplicationRegistry", function () {
  Cu.import("resource://gre/modules/Webapps.jsm");
  return DOMApplicationRegistry;
});

const TOUCH_EVENTS_ENABLED_PREF = "dom.w3c_touch_events.enabled";

function debug(msg) {
  //dump("BrowserElementParent.jsm - " + msg + "\n");
}

function getIntPref(prefName, def) {
  try {
    return Services.prefs.getIntPref(prefName);
  }
  catch(err) {
    return def;
  }
}

function exposeAll(obj) {
  // Filter for Objects and Arrays.
  if (typeof obj !== "object" || !obj)
    return;

  // Recursively expose our children.
  Object.keys(obj).forEach(function(key) {
    exposeAll(obj[key]);
  });

  // If we're not an Array, generate an __exposedProps__ object for ourselves.
  if (obj instanceof Array)
    return;
  var exposed = {};
  Object.keys(obj).forEach(function(key) {
    exposed[key] = 'rw';
  });
  obj.__exposedProps__ = exposed;
}

function defineAndExpose(obj, name, value) {
  obj[name] = value;
  if (!('__exposedProps__' in obj))
    obj.__exposedProps__ = {};
  obj.__exposedProps__[name] = 'r';
}

function visibilityChangeHandler(weakBEP, win) {
  let bep = weakBEP.get();
  if (bep) {
    bep._ownerVisibilityChange();
  } else {
    win.removeEventListener('visibilitychange', visibilityChangeHandler,
                            /* useCapture */ false);
  }
}

this.BrowserElementParentBuilder = {
  create: function create(frameLoader, hasRemoteFrame) {
    return new BrowserElementParent(frameLoader, hasRemoteFrame);
  }
}

function BrowserElementParent(frameLoader, hasRemoteFrame) {
  debug("Creating new BrowserElementParent object for " + frameLoader);
  this._domRequestCounter = 0;
  this._pendingDOMRequests = {};
  this._hasRemoteFrame = hasRemoteFrame;
  this._nextPaintListeners = [];

  this._frameLoader = frameLoader;
  this._frameElement = frameLoader.QueryInterface(Ci.nsIFrameLoader).ownerElement;
  if (!this._frameElement) {
    debug("No frame element?");
    return;
  }

  this._mm = frameLoader.messageManager;
  let self = this;

  // Messages we receive are handed to functions which take a (data) argument,
  // where |data| is the message manager's data object.
  // We use a single message and dispatch to various function based
  // on data.msg_name
  let mmCalls = {
    "hello": this._recvHello,
    "contextmenu": this._fireCtxMenuEvent,
    "locationchange": this._gotLocationChange,
    "loadstart": this._fireEventFromMsg,
    "loadend": this._fireEventFromMsg,
    "titlechange": this._fireEventFromMsg,
    "iconchange": this._fireEventFromMsg,
    "close": this._fireEventFromMsg,
    "securitychange": this._fireEventFromMsg,
    "error": this._fireEventFromMsg,
    "scroll": this._fireEventFromMsg,
    "firstpaint": this._fireEventFromMsg,
    "documentfirstpaint": this._fireEventFromMsg,
    "nextpaint": this._recvNextPaint,
    "keyevent": this._fireKeyEvent,
    "showmodalprompt": this._handleShowModalPrompt,
    "got-purge-history": this._gotDOMRequestResult,
    "got-screenshot": this._gotDOMRequestResult,
    "got-can-go-back": this._gotDOMRequestResult,
    "got-can-go-forward": this._gotDOMRequestResult,
    "fullscreen-origin-change": this._remoteFullscreenOriginChange,
    "rollback-fullscreen": this._remoteFrameFullscreenReverted,
    "exit-fullscreen": this._exitFullscreen,
    "got-visible": this._gotDOMRequestResult,
    "visibilitychange": this._childVisibilityChange,
  }

  this._mm.addMessageListener('browser-element-api:call', function(aMsg) {
    if (self._isAlive() && (aMsg.data.msg_name in mmCalls)) {
      return mmCalls[aMsg.data.msg_name].apply(self, arguments);
    }
  });

  Services.obs.addObserver(this, 'ask-children-to-exit-fullscreen', /* ownsWeak = */ true);
  Services.obs.addObserver(this, 'oop-frameloader-crashed', /* ownsWeak = */ true);

  let defineMethod = function(name, fn) {
    XPCNativeWrapper.unwrap(self._frameElement)[name] = function() {
      if (self._isAlive()) {
        return fn.apply(self, arguments);
      }
    };
  }

  let defineDOMRequestMethod = function(domName, msgName) {
    XPCNativeWrapper.unwrap(self._frameElement)[domName] = function() {
      if (self._isAlive()) {
        return self._sendDOMRequest(msgName);
      }
    };
  }

  // Define methods on the frame element.
  defineMethod('setVisible', this._setVisible);
  defineDOMRequestMethod('getVisible', 'get-visible');
  defineMethod('sendMouseEvent', this._sendMouseEvent);

  // 0 = disabled, 1 = enabled, 2 - auto detect
  if (getIntPref(TOUCH_EVENTS_ENABLED_PREF, 0) != 0) {
    defineMethod('sendTouchEvent', this._sendTouchEvent);
  }
  defineMethod('goBack', this._goBack);
  defineMethod('goForward', this._goForward);
  defineMethod('reload', this._reload);
  defineMethod('stop', this._stop);
  defineMethod('purgeHistory', this._purgeHistory);
  defineMethod('getScreenshot', this._getScreenshot);
  defineMethod('addNextPaintListener', this._addNextPaintListener);
  defineMethod('removeNextPaintListener', this._removeNextPaintListener);
  defineDOMRequestMethod('getCanGoBack', 'get-can-go-back');
  defineDOMRequestMethod('getCanGoForward', 'get-can-go-forward');

  // Listen to visibilitychange on the iframe's owner window, and forward it
  // down to the child.  The event listener must hold a weak reference to this,
  // otherwise the window will hold this object alive.

  var weakSelf = Cu.getWeakReference(this);
  this._window.addEventListener('visibilitychange',
                                visibilityChangeHandler.bind(null, weakSelf, this._window),
                                /* useCapture = */ false,
                                /* wantsUntrusted = */ false);

  // Insert ourself into the prompt service.
  BrowserElementPromptService.mapFrameToBrowserElementParent(this._frameElement, this);

  // If this browser represents an app then let the Webapps module register for
  // any messages that it needs.
  let appManifestURL =
    this._frameElement.QueryInterface(Ci.nsIMozBrowserFrame).appManifestURL;
  if (appManifestURL) {
    let appId =
      DOMApplicationRegistry.getAppLocalIdByManifestURL(appManifestURL);
    if (appId != Ci.nsIScriptSecurityManager.NO_APP_ID) {
      DOMApplicationRegistry.registerBrowserElementParentForApp(this, appId);
    }
  }
}

BrowserElementParent.prototype = {

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

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

    if (authDetail.isOnlyPassword) {
      // We don't handle password-only prompts, so just cancel it.
      cancelCallback();
      return;
    } else { /* username and password */
      let detail = {
        host:     authDetail.host,
        realm:    authDetail.realm
      };

      evt = this._createEvent('usernameandpasswordrequired', detail,
                              /* cancelable */ true);
      defineAndExpose(evt.detail, 'authenticate', function(username, password) {
        if (callbackCalled)
          return;
        callbackCalled = true;
        callback(true, username, password);
      });
    }

    defineAndExpose(evt.detail, 'cancel', function() {
      cancelCallback();
    });

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

  _recvHello: function(data) {
    debug("recvHello");

    // Inform our child if our owner element's document is invisible.  Note
    // that we must do so here, rather than in the BrowserElementParent
    // constructor, because the BrowserElementChild may not be initialized when
    // we run our constructor.
    if (this._window.document.hidden) {
      this._ownerVisibilityChange();
    }

    return {
      name: this._frameElement.getAttribute('name'),
      fullscreenAllowed:
        this._frameElement.hasAttribute('allowfullscreen') ||
        this._frameElement.hasAttribute('mozallowfullscreen')
    }
  },

  _fireCtxMenuEvent: function(data) {
    let detail = data.json;
    let evtName = detail.msg_name;

    debug('fireCtxMenuEventFromMsg: ' + evtName + ' ' + detail);
    let evt = this._createEvent(evtName, detail, /* cancellable */ true);

    if (detail.contextmenu) {
      var self = this;
      defineAndExpose(evt.detail, 'contextMenuItemSelected', function(id) {
        self._sendAsyncMsg('fire-ctx-callback', {menuitem: id});
      });
    }

    // The embedder may have default actions on context menu events, so
    // we fire a context menu event even if the child didn't define a
    // custom context menu
    return !this._frameElement.dispatchEvent(evt);
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

    defineAndExpose(evt.detail, 'unblock', function() {
      sendUnblockMsg();
    });

    this._frameElement.dispatchEvent(evt);

    if (!evt.defaultPrevented) {
      // Unblock the inner frame immediately.  Otherwise we'll unblock upon
      // evt.detail.unblock().
      sendUnblockMsg();
    }
  },

  _createEvent: function(evtName, detail, cancelable) {
    // This will have to change if we ever want to send a CustomEvent with null
    // detail.  For now, it's OK.
    if (detail !== undefined && detail !== null) {
      exposeAll(detail);
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
    if (this._sendAsyncMsg(msgName, {id: id, args: args})) {
      this._pendingDOMRequests[id] = req;
    } else {
      Services.DOMRequest.fireErrorAsync(req, "fail");
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
      Services.DOMRequest.fireSuccess(req, data.json.successRv);
    }
    else {
      debug("Got error in gotDOMRequestResult.");
      Services.DOMRequest.fireErrorAsync(req, data.json.errorMsg);
    }
  },

  _setVisible: function(visible) {
    this._sendAsyncMsg('set-visible', {visible: visible});
    this._frameLoader.visible = visible;
  },

  _sendMouseEvent: function(type, x, y, button, clickCount, modifiers) {
    this._sendAsyncMsg("send-mouse-event", {
      "type": type,
      "x": x,
      "y": y,
      "button": button,
      "clickCount": clickCount,
      "modifiers": modifiers
    });
  },

  _sendTouchEvent: function(type, identifiers, touchesX, touchesY,
                            radiisX, radiisY, rotationAngles, forces,
                            count, modifiers) {
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
  },

  _goBack: function() {
    this._sendAsyncMsg('go-back');
  },

  _goForward: function() {
    this._sendAsyncMsg('go-forward');
  },

  _reload: function(hardReload) {
    this._sendAsyncMsg('reload', {hardReload: hardReload});
  },

  _stop: function() {
    this._sendAsyncMsg('stop');
  },

  _purgeHistory: function() {
    return this._sendDOMRequest('purge-history');
  },

  _getScreenshot: function(_width, _height) {
    let width = parseInt(_width);
    let height = parseInt(_height);
    if (isNaN(width) || isNaN(height) || width < 0 || height < 0) {
      throw Components.Exception("Invalid argument",
                                 Cr.NS_ERROR_INVALID_ARG);
    }

    return this._sendDOMRequest('get-screenshot',
                                {width: width, height: height});
  },

  _recvNextPaint: function(data) {
    let listeners = this._nextPaintListeners;
    this._nextPaintListeners = [];
    for (let listener of listeners) {
      try {
        listener();
      } catch (e) {
        // If a listener throws we'll continue.
      }
    }
  },

  _addNextPaintListener: function(listener) {
    if (typeof listener != 'function')
      throw Components.Exception("Invalid argument", Cr.NS_ERROR_INVALID_ARG);

    if (this._nextPaintListeners.push(listener) == 1)
      this._sendAsyncMsg('activate-next-paint-listener');
  },

  _removeNextPaintListener: function(listener) {
    if (typeof listener != 'function')
      throw Components.Exception("Invalid argument", Cr.NS_ERROR_INVALID_ARG);

    for (let i = this._nextPaintListeners.length - 1; i >= 0; i--) {
      if (this._nextPaintListeners[i] == listener) {
        this._nextPaintListeners.splice(i, 1);
        break;
      }
    }

    if (this._nextPaintListeners.length == 0)
      this._sendAsyncMsg('deactivate-next-paint-listener');
  },

  _fireKeyEvent: function(data) {
    let evt = this._window.document.createEvent("KeyboardEvent");
    evt.initKeyEvent(data.json.type, true, true, this._window,
                     false, false, false, false, // modifiers
                     data.json.keyCode,
                     data.json.charCode);

    this._frameElement.dispatchEvent(evt);
  },

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

  _exitFullscreen: function() {
    this._windowUtils.exitFullscreen();
  },

  _remoteFullscreenOriginChange: function(data) {
    let origin = data.json._payload_;
    this._windowUtils.remoteFrameFullscreenChanged(this._frameElement, origin);
  },

  _remoteFrameFullscreenReverted: function(data) {
    this._windowUtils.remoteFrameFullscreenReverted();
  },

  _fireFatalError: function() {
    let evt = this._createEvent('error', {type: 'fatal'},
                                /* cancelable = */ false);
    this._frameElement.dispatchEvent(evt);
  },

  _gotLocationChange: function(data) {
    this[data._payload_] = 'BEP location';
    this._fireEventFromMsg(data);
  },

  observe: function(subject, topic, data) {
    switch(topic) {
    case 'oop-frameloader-crashed':
      if (this._isAlive() && subject == this._frameLoader) {
        this._fireFatalError();
      }
      break;
    case 'ask-children-to-exit-fullscreen':
      if (this._isAlive() &&
          this._frameElement.ownerDocument == subject &&
          this._hasRemoteFrame) {
        this._sendAsyncMsg('exit-fullscreen');
      }
      break;
    default:
      debug('Unknown topic: ' + topic);
      break;
    };
  },
};
