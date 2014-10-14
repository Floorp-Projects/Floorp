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

function visibilityChangeHandler(e) {
  // The visibilitychange event's target is the document.
  let win = e.target.defaultView;

  if (!win._browserElementParents) {
    return;
  }

  let beps = Cu.nondeterministicGetWeakMapKeys(win._browserElementParents);
  if (beps.length == 0) {
    win.removeEventListener('visibilitychange', visibilityChangeHandler);
    return;
  }

  for (let i = 0; i < beps.length; i++) {
    beps[i]._ownerVisibilityChange();
  }
}

this.BrowserElementParentBuilder = {
  create: function create(frameLoader, hasRemoteFrame, isPendingFrame) {
    return new BrowserElementParent(frameLoader, hasRemoteFrame);
  }
}

function BrowserElementParent(frameLoader, hasRemoteFrame, isPendingFrame) {
  debug("Creating new BrowserElementParent object for " + frameLoader);
  this._domRequestCounter = 0;
  this._domRequestReady = false;
  this._pendingAPICalls = [];
  this._pendingDOMRequests = {};
  this._pendingSetInputMethodActive = [];
  this._hasRemoteFrame = hasRemoteFrame;
  this._nextPaintListeners = [];

  this._frameLoader = frameLoader;
  this._frameElement = frameLoader.QueryInterface(Ci.nsIFrameLoader).ownerElement;
  let self = this;
  if (!this._frameElement) {
    debug("No frame element?");
    return;
  }

  Services.obs.addObserver(this, 'ask-children-to-exit-fullscreen', /* ownsWeak = */ true);
  Services.obs.addObserver(this, 'oop-frameloader-crashed', /* ownsWeak = */ true);
  Services.obs.addObserver(this, 'copypaste-docommand', /* ownsWeak = */ true);

  let defineMethod = function(name, fn) {
    XPCNativeWrapper.unwrap(self._frameElement)[name] = function() {
      if (self._isAlive()) {
        return fn.apply(self, arguments);
      }
    };
  }

  let defineNoReturnMethod = function(name, fn) {
    XPCNativeWrapper.unwrap(self._frameElement)[name] = function method() {
      if (!self._domRequestReady) {
        // Remote browser haven't been created, we just queue the API call.
        let args = Array.slice(arguments);
        args.unshift(self);
        self._pendingAPICalls.push(method.bind.apply(fn, args));
        return;
      }
      if (self._isAlive()) {
        fn.apply(self, arguments);
      }
    };
  };

  let defineDOMRequestMethod = function(domName, msgName) {
    XPCNativeWrapper.unwrap(self._frameElement)[domName] = function() {
      return self._sendDOMRequest(msgName);
    };
  }

  // Define methods on the frame element.
  defineNoReturnMethod('setVisible', this._setVisible);
  defineDOMRequestMethod('getVisible', 'get-visible');

  // Not expose security sensitive browser API for widgets
  if (!this._frameLoader.QueryInterface(Ci.nsIFrameLoader).ownerIsWidget) {
    defineNoReturnMethod('sendMouseEvent', this._sendMouseEvent);

    // 0 = disabled, 1 = enabled, 2 - auto detect
    if (getIntPref(TOUCH_EVENTS_ENABLED_PREF, 0) != 0) {
      defineNoReturnMethod('sendTouchEvent', this._sendTouchEvent);
    }
    defineNoReturnMethod('goBack', this._goBack);
    defineNoReturnMethod('goForward', this._goForward);
    defineNoReturnMethod('reload', this._reload);
    defineNoReturnMethod('stop', this._stop);
    defineMethod('download', this._download);
    defineDOMRequestMethod('purgeHistory', 'purge-history');
    defineMethod('getScreenshot', this._getScreenshot);
    defineNoReturnMethod('zoom', this._zoom);

    defineDOMRequestMethod('getCanGoBack', 'get-can-go-back');
    defineDOMRequestMethod('getCanGoForward', 'get-can-go-forward');
    defineDOMRequestMethod('getContentDimensions', 'get-contentdimensions');
  }

  defineMethod('addNextPaintListener', this._addNextPaintListener);
  defineMethod('removeNextPaintListener', this._removeNextPaintListener);
  defineNoReturnMethod('setActive', this._setActive);
  defineMethod('getActive', 'this._getActive');

  let principal = this._frameElement.ownerDocument.nodePrincipal;
  let perm = Services.perms
             .testExactPermissionFromPrincipal(principal, "input-manage");
  if (perm === Ci.nsIPermissionManager.ALLOW_ACTION) {
    defineMethod('setInputMethodActive', this._setInputMethodActive);
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
    this._window.addEventListener('visibilitychange',
                                  visibilityChangeHandler,
                                  /* useCapture = */ false,
                                  /* wantsUntrusted = */ false);
  }

  this._window._browserElementParents.set(this, null);

  // Insert ourself into the prompt service.
  BrowserElementPromptService.mapFrameToBrowserElementParent(this._frameElement, this);
  if (!isPendingFrame) {
    this._setupMessageListener();
    this._registerAppManifest();
  } else {
    // if we are a pending frame, we setup message manager after
    // observing remote-browser-frame-shown
    Services.obs.addObserver(this, 'remote-browser-frame-shown', /* ownsWeak = */ true);
  }
}

BrowserElementParent.prototype = {

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

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

  _registerAppManifest: function() {
    // If this browser represents an app then let the Webapps module register for
    // any messages that it needs.
    let appManifestURL =
          this._frameElement.QueryInterface(Ci.nsIMozBrowserFrame).appManifestURL;
    if (appManifestURL) {
      let inParent = Cc["@mozilla.org/xre/app-info;1"]
                       .getService(Ci.nsIXULRuntime)
                       .processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
      if (inParent) {
        DOMApplicationRegistry.registerBrowserElementParentForApp(
          { manifestURL: appManifestURL }, this._mm);
      } else {
        this._mm.sendAsyncMessage("Webapps:RegisterBEP",
                                  { manifestURL: appManifestURL });
      }
    }
  },

  _setupMessageListener: function() {
    this._mm = this._frameLoader.messageManager;
    let self = this;
    let isWidget = this._frameLoader
                       .QueryInterface(Ci.nsIFrameLoader)
                       .ownerIsWidget;

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
      "keyevent": this._fireKeyEvent,
      "got-purge-history": this._gotDOMRequestResult,
      "got-screenshot": this._gotDOMRequestResult,
      "got-contentdimensions": this._gotDOMRequestResult,
      "got-can-go-back": this._gotDOMRequestResult,
      "got-can-go-forward": this._gotDOMRequestResult,
      "fullscreen-origin-change": this._remoteFullscreenOriginChange,
      "rollback-fullscreen": this._remoteFrameFullscreenReverted,
      "exit-fullscreen": this._exitFullscreen,
      "got-visible": this._gotDOMRequestResult,
      "visibilitychange": this._childVisibilityChange,
      "got-set-input-method-active": this._gotDOMRequestResult,
      "selectionchange": this._handleSelectionChange,
      "scrollviewchange": this._handleScrollViewChange,
      "touchcarettap": this._handleTouchCaretTap
    };

    let mmSecuritySensitiveCalls = {
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
      "scroll": this._fireEventFromMsg
    };

    this._mm.addMessageListener('browser-element-api:call', function(aMsg) {
      if (!self._isAlive()) {
        return;
      }

      if (aMsg.data.msg_name in mmCalls) {
        return mmCalls[aMsg.data.msg_name].apply(self, arguments);
      } else if (!isWidget && aMsg.data.msg_name in mmSecuritySensitiveCalls) {
        return mmSecuritySensitiveCalls[aMsg.data.msg_name]
                 .apply(self, arguments);
      }
    });
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

    // 1. We don't handle password-only prompts.
    // 2. We don't handle for widget case because of security concern.
    if (authDetail.isOnlyPassword ||
        this._frameLoader.QueryInterface(Ci.nsIFrameLoader).ownerIsWidget) {
      cancelCallback();
      return;
    }

    /* username and password */
    let detail = {
      host:     authDetail.host,
      realm:    authDetail.realm
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

    return {
      name: this._frameElement.getAttribute('name'),
      fullscreenAllowed:
        this._frameElement.hasAttribute('allowfullscreen') ||
        this._frameElement.hasAttribute('mozallowfullscreen'),
      isPrivate: this._frameElement.hasAttribute('mozprivatebrowsing')
    };
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

  _handleSelectionChange: function(data) {
    let evt = this._createEvent('selectionchange', data.json,
                                /* cancelable = */ false);
    this._frameElement.dispatchEvent(evt);
  },

  _handleScrollViewChange: function(data) {
    let evt = this._createEvent("scrollviewchange", data.json,
                                /* cancelable = */ false);
    this._frameElement.dispatchEvent(evt);
  },

  _handleTouchCaretTap: function(data) {
    let evt = this._createEvent("touchcarettap", data.json,
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

  _setVisible: function(visible) {
    this._sendAsyncMsg('set-visible', {visible: visible});
    this._frameLoader.visible = visible;
  },

  _setActive: function(active) {
    this._frameLoader.visible = active;
  },

  _getActive: function() {
    return this._frameLoader.visible;
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

    let tabParent = this._frameLoader.tabParent;
    if (tabParent && tabParent.useAsyncPanZoom) {
      tabParent.injectTouchEvent(type,
                                 identifiers,
                                 touchesX,
                                 touchesY,
                                 radiisX,
                                 radiisY,
                                 rotationAngles,
                                 forces,
                                 count,
                                 modifiers);
    } else {
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
    }
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

  /*
   * The valid range of zoom scale is defined in preference "zoom.maxPercent" and "zoom.minPercent".
   */
  _zoom: function(zoom) {
    zoom *= 100;
    zoom = Math.min(getIntPref("zoom.maxPercent", 300), zoom);
    zoom = Math.max(getIntPref("zoom.minPercent", 50), zoom);
    this._sendAsyncMsg('zoom', {zoom: zoom / 100.0});
  },

  _download: function(_url, _options) {
    let ioService =
      Cc['@mozilla.org/network/io-service;1'].getService(Ci.nsIIOService);
    let uri = ioService.newURI(_url, null, null);
    let url = uri.QueryInterface(Ci.nsIURL);

    // Ensure we have _options, we always use it to send the filename.
    _options = _options || {};
    if (!_options.filename) {
      _options.filename = url.fileName;
    }

    debug('_options = ' + uneval(_options));

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

    let channel = ioService.newChannelFromURI(url);

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
      debug('Setting HTTP referrer = ' + this._window.document.documentURIObject);
      channel.referrer = this._window.document.documentURIObject;
      if (channel instanceof Ci.nsIHttpChannelInternal) {
        channel.forceAllowThirdPartyCookie = true;
      }
    }

    // Set-up complete, let's get things started.
    channel.asyncOpen(new DownloadListener(), null);

    return req;
  },

  _getScreenshot: function(_width, _height, _mimeType) {
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
        listener();
      } catch (e) {
        // If a listener throws we'll continue.
      }
    }
  },

  _addNextPaintListener: function(listener) {
    if (typeof listener != 'function')
      throw Components.Exception("Invalid argument", Cr.NS_ERROR_INVALID_ARG);

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

  _removeNextPaintListener: function(listener) {
    if (typeof listener != 'function')
      throw Components.Exception("Invalid argument", Cr.NS_ERROR_INVALID_ARG);

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

  _setInputMethodActive: function(isActive) {
    if (typeof isActive !== 'boolean') {
      throw Components.Exception("Invalid argument",
                                 Cr.NS_ERROR_INVALID_ARG);
    }

    return this._sendDOMRequest('set-input-method-active',
                                {isActive: isActive});
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
    case 'remote-browser-frame-shown':
      if (this._frameLoader == subject) {
        if (!this._mm) {
          this._setupMessageListener();
          this._registerAppManifest();
        }
        Services.obs.removeObserver(this, 'remote-browser-frame-shown');
      }
    case 'copypaste-docommand':
      if (this._isAlive() && this._frameElement.isEqualNode(subject.wrappedJSObject)) {
        this._sendAsyncMsg('do-command', { command: data });
      }
      break;
    default:
      debug('Unknown topic: ' + topic);
      break;
    };
  },
};
