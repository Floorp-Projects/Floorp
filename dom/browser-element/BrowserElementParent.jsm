/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* BrowserElementParent injects script to listen for certain events in the
 * child.  We then listen to messages from the child script and take
 * appropriate action here in the parent.
 */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { BrowserElementPromptService } = ChromeUtils.import(
  "resource://gre/modules/BrowserElementPromptService.jsm"
);

function debug(msg) {
  // dump("BrowserElementParent - " + msg + "\n");
}

function handleWindowEvent(e) {
  if (this._browserElementParents) {
    let beps = ChromeUtils.nondeterministicGetWeakMapKeys(
      this._browserElementParents
    );
    beps.forEach(bep => bep._handleOwnerEvent(e));
  }
}

function defineNoReturnMethod(fn) {
  return function method() {
    if (!this._domRequestReady) {
      // Remote browser haven't been created, we just queue the API call.
      let args = [this, ...arguments];
      this._pendingAPICalls.push(method.bind.apply(fn, args));
      return;
    }
    if (this._isAlive()) {
      fn.apply(this, arguments);
    }
  };
}

function definePromiseMethod(msgName) {
  return function() {
    return this._sendAsyncRequest(msgName);
  };
}

function BrowserElementParent() {
  debug("Creating new BrowserElementParent object");
  this._promiseCounter = 0;
  this._domRequestReady = false;
  this._pendingAPICalls = [];
  this._pendingPromises = {};
  this._pendingDOMFullscreen = false;

  Services.obs.addObserver(
    this,
    "oop-frameloader-crashed",
    /* ownsWeak = */ true
  );
  Services.obs.addObserver(
    this,
    "ask-children-to-execute-copypaste-command",
    /* ownsWeak = */ true
  );
  Services.obs.addObserver(this, "back-docommand", /* ownsWeak = */ true);
}

BrowserElementParent.prototype = {
  classDescription: "BrowserElementAPI implementation",
  classID: Components.ID("{9f171ac4-0939-4ef8-b360-3408aedc3060}"),
  contractID: "@mozilla.org/dom/browser-element-api;1",
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIBrowserElementAPI,
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference,
  ]),

  setFrameLoader(frameLoader) {
    debug("Setting frameLoader");
    this._frameLoader = frameLoader;
    this._frameElement = frameLoader.ownerElement;
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
      let windowEvents = ["visibilitychange", "fullscreenchange"];
      for (let event of windowEvents) {
        Services.els.addSystemEventListener(
          this._window,
          event,
          handler,
          /* useCapture = */ true
        );
      }
    }

    this._window._browserElementParents.set(this, null);

    // Insert ourself into the prompt service.
    BrowserElementPromptService.mapFrameToBrowserElementParent(
      this._frameElement,
      this
    );
    this._setupMessageListener();
  },

  destroyFrameScripts() {
    debug("Destroying frame scripts");
    this._mm.sendAsyncMessage("browser-element-api:destroy");
  },

  _runPendingAPICall() {
    if (!this._pendingAPICalls) {
      return;
    }
    for (let i = 0; i < this._pendingAPICalls.length; i++) {
      try {
        this._pendingAPICalls[i]();
      } catch (e) {
        // throw the exceptions from pending functions.
        debug("Exception when running pending API call: " + e);
      }
    }
    delete this._pendingAPICalls;
  },

  _setupMessageListener() {
    this._mm = this._frameLoader.messageManager;
    this._mm.addMessageListener("browser-element-api:call", this);
  },

  receiveMessage(aMsg) {
    if (!this._isAlive()) {
      return undefined;
    }

    // Messages we receive are handed to functions which take a (data) argument,
    // where |data| is the message manager's data object.
    // We use a single message and dispatch to various function based
    // on data.msg_name
    let mmCalls = {
      hello: this._recvHello,
      loadstart: this._fireProfiledEventFromMsg,
      loadend: this._fireProfiledEventFromMsg,
      close: this._fireEventFromMsg,
      error: this._fireEventFromMsg,
      firstpaint: this._fireProfiledEventFromMsg,
      documentfirstpaint: this._fireProfiledEventFromMsg,
      "got-can-go-back": this._gotAsyncResult,
      "got-can-go-forward": this._gotAsyncResult,
      "requested-dom-fullscreen": this._requestedDOMFullscreen,
      "fullscreen-origin-change": this._fullscreenOriginChange,
      "exit-dom-fullscreen": this._exitDomFullscreen,
      scrollviewchange: this._handleScrollViewChange,
      caretstatechanged: this._handleCaretStateChanged,
    };

    let mmSecuritySensitiveCalls = {
      audioplaybackchange: this._fireEventFromMsg,
      showmodalprompt: this._handleShowModalPrompt,
      contextmenu: this._fireCtxMenuEvent,
      securitychange: this._fireEventFromMsg,
      locationchange: this._fireEventFromMsg,
      iconchange: this._fireEventFromMsg,
      scrollareachanged: this._fireEventFromMsg,
      titlechange: this._fireProfiledEventFromMsg,
      opensearch: this._fireEventFromMsg,
      metachange: this._fireEventFromMsg,
      resize: this._fireEventFromMsg,
      activitydone: this._fireEventFromMsg,
      scroll: this._fireEventFromMsg,
      opentab: this._fireEventFromMsg,
    };

    if (aMsg.data.msg_name in mmCalls) {
      return mmCalls[aMsg.data.msg_name].apply(this, arguments);
    } else if (aMsg.data.msg_name in mmSecuritySensitiveCalls) {
      return mmSecuritySensitiveCalls[aMsg.data.msg_name].apply(
        this,
        arguments
      );
    }
    return undefined;
  },

  _removeMessageListener() {
    this._mm.removeMessageListener("browser-element-api:call", this);
  },

  /**
   * You shouldn't touch this._frameElement or this._window if _isAlive is
   * false.  (You'll likely get an exception if you do.)
   */
  _isAlive() {
    return (
      !Cu.isDeadWrapper(this._frameElement) &&
      !Cu.isDeadWrapper(this._frameElement.ownerDocument) &&
      !Cu.isDeadWrapper(this._frameElement.ownerGlobal)
    );
  },

  get _window() {
    return this._frameElement.ownerGlobal;
  },

  get _windowUtils() {
    return this._window.windowUtils;
  },

  promptAuth(authDetail, callback) {
    let evt;
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
      host: authDetail.host,
      path: authDetail.path,
      realm: authDetail.realm,
      isProxy: authDetail.isProxy,
    };

    evt = this._createEvent(
      "usernameandpasswordrequired",
      detail,
      /* cancelable */ true
    );
    Cu.exportFunction(
      function(username, password) {
        if (callbackCalled) {
          return;
        }
        callbackCalled = true;
        callback(true, username, password);
      },
      evt.detail,
      { defineAs: "authenticate" }
    );

    Cu.exportFunction(cancelCallback, evt.detail, { defineAs: "cancel" });

    this._frameElement.dispatchEvent(evt);

    if (!evt.defaultPrevented) {
      cancelCallback();
    }
  },

  _sendAsyncMsg(msg, data) {
    try {
      if (!data) {
        data = {};
      }

      data.msg_name = msg;
      this._mm.sendAsyncMessage("browser-element-api:call", data);
    } catch (e) {
      return false;
    }
    return true;
  },

  _recvHello() {
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

  _fireCtxMenuEvent(data) {
    let detail = data.json;
    let evtName = detail.msg_name;

    debug("fireCtxMenuEventFromMsg: " + evtName + " " + detail);
    let evt = this._createEvent(evtName, detail, /* cancellable */ true);

    if (detail.contextmenu) {
      var self = this;
      Cu.exportFunction(
        function(id) {
          self._sendAsyncMsg("fire-ctx-callback", { menuitem: id });
        },
        evt.detail,
        { defineAs: "contextMenuItemSelected" }
      );
    }

    // The embedder may have default actions on context menu events, so
    // we fire a context menu event even if the child didn't define a
    // custom context menu
    return !this._frameElement.dispatchEvent(evt);
  },

  /**
   * add profiler marker for each event fired.
   */
  _fireProfiledEventFromMsg(data) {
    if (Services.profiler !== undefined) {
      Services.profiler.AddMarker(data.json.msg_name);
    }
    this._fireEventFromMsg(data);
  },

  /**
   * Fire either a vanilla or a custom event, depending on the contents of
   * |data|.
   */
  _fireEventFromMsg(data) {
    let detail = data.json;
    let name = detail.msg_name;

    // For events that send a "_payload_" property, we just want to transmit
    // this in the event.
    if ("_payload_" in detail) {
      detail = detail._payload_;
    }

    debug("fireEventFromMsg: " + name + ", " + JSON.stringify(detail));
    let evt = this._createEvent(name, detail, /* cancelable = */ false);
    this._frameElement.dispatchEvent(evt);
  },

  _handleShowModalPrompt(data) {
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
    debug("handleShowPrompt " + JSON.stringify(detail));

    // Strip off the windowID property from the object we send along in the
    // event.
    let windowID = detail.windowID;
    delete detail.windowID;
    debug("Event will have detail: " + JSON.stringify(detail));
    let evt = this._createEvent(
      "showmodalprompt",
      detail,
      /* cancelable = */ true
    );

    let self = this;
    let unblockMsgSent = false;
    function sendUnblockMsg() {
      if (unblockMsgSent) {
        return;
      }
      unblockMsgSent = true;

      // We don't need to sanitize evt.detail.returnValue (e.g. converting the
      // return value of confirm() to a boolean); Gecko does that for us.

      let data = { windowID, returnValue: evt.detail.returnValue };
      self._sendAsyncMsg("unblock-modal-prompt", data);
    }

    Cu.exportFunction(sendUnblockMsg, evt.detail, { defineAs: "unblock" });

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
  _handleCaretStateChanged(data) {
    let evt = this._createEvent(
      "caretstatechanged",
      data.json,
      /* cancelable = */ false
    );

    let self = this;
    function sendDoCommandMsg(cmd) {
      let data = { command: cmd };
      self._sendAsyncMsg("copypaste-do-command", data);
    }
    Cu.exportFunction(sendDoCommandMsg, evt.detail, {
      defineAs: "sendDoCommandMsg",
    });

    this._frameElement.dispatchEvent(evt);
  },

  _handleScrollViewChange(data) {
    let evt = this._createEvent(
      "scrollviewchange",
      data.json,
      /* cancelable = */ false
    );
    this._frameElement.dispatchEvent(evt);
  },

  _createEvent(evtName, detail, cancelable) {
    // This will have to change if we ever want to send a CustomEvent with null
    // detail.  For now, it's OK.
    if (detail !== undefined && detail !== null) {
      detail = Cu.cloneInto(detail, this._window);
      return new this._window.CustomEvent("mozbrowser" + evtName, {
        bubbles: true,
        cancelable,
        detail,
      });
    }

    return new this._window.Event("mozbrowser" + evtName, {
      bubbles: true,
      cancelable,
    });
  },

  /**
   * Kick off an async operation in the child process.
   *
   * We'll send a message called |msgName| to the child process, passing along
   * an object with two fields:
   *
   *  - id:  the ID of this async call.
   *  - arg: arguments to pass to the child along with this async call.
   *
   * We expect the child to pass the ID back to us upon completion of the
   * call.  See _gotAsyncResult.
   */
  _sendAsyncRequest(msgName, args) {
    let id = "req_" + this._promiseCounter++;
    let resolve, reject;
    let p = new this._window.Promise((res, rej) => {
      resolve = res;
      reject = rej;
    });
    let self = this;
    let send = function() {
      if (!self._isAlive()) {
        return;
      }
      if (self._sendAsyncMsg(msgName, { id, args })) {
        self._pendingPromises[id] = { p, resolve, reject };
      } else {
        reject(new this._window.DOMException("fail"));
      }
    };
    if (this._domRequestReady) {
      send();
    } else {
      // Child haven't been loaded.
      this._pendingAPICalls.push(send);
    }
    return p;
  },

  /**
   * Called when the child process finishes handling an async call.  data.json
   * must have the fields [id, successRv], if the async call was successful, or
   * [id, errorMsg], if the call was not successful.
   *
   * The fields have the following meanings:
   *
   *  - id:        the ID of the async call (see _sendAsyncRequest)
   *  - successRv: the call's return value, if the call succeeded
   *  - errorMsg:  the message to pass to the Promise reject callback, if the
   *               call failed.
   *
   */
  _gotAsyncResult(data) {
    let p = this._pendingPromises[data.json.id];
    delete this._pendingPromises[data.json.id];

    if ("successRv" in data.json) {
      debug("Successful gotAsyncResult.");
      let clientObj = Cu.cloneInto(data.json.successRv, this._window);
      p.resolve(clientObj);
    } else {
      debug("Got error in gotAsyncResult.");
      p.reject(
        new this._window.DOMException(
          Cu.cloneInto(data.json.errorMsg, this._window)
        )
      );
    }
  },

  sendMouseEvent: defineNoReturnMethod(function(
    type,
    x,
    y,
    button,
    clickCount,
    modifiers
  ) {
    // This method used to attempt to transform from the parent
    // coordinate space to the child coordinate space, but the
    // transform was always a no-op, because this._frameLoader.remoteTab
    // was null.
    this._sendAsyncMsg("send-mouse-event", {
      type,
      x,
      y,
      button,
      clickCount,
      modifiers,
    });
  }),

  getCanGoBack: definePromiseMethod("get-can-go-back"),
  getCanGoForward: definePromiseMethod("get-can-go-forward"),

  goBack: defineNoReturnMethod(function() {
    this._sendAsyncMsg("go-back");
  }),

  goForward: defineNoReturnMethod(function() {
    this._sendAsyncMsg("go-forward");
  }),

  reload: defineNoReturnMethod(function(hardReload) {
    this._sendAsyncMsg("reload", { hardReload });
  }),

  stop: defineNoReturnMethod(function() {
    this._sendAsyncMsg("stop");
  }),

  /**
   * Called when the visibility of the window which owns this iframe changes.
   */
  _ownerVisibilityChange() {
    this._sendAsyncMsg("owner-visibility-change", {
      visible: !this._window.document.hidden,
    });
  },

  _requestedDOMFullscreen() {
    this._pendingDOMFullscreen = true;
    this._windowUtils.remoteFrameFullscreenChanged(this._frameElement);
  },

  _fullscreenOriginChange(data) {
    Services.obs.notifyObservers(
      this._frameElement,
      "fullscreen-origin-change",
      data.json.originNoSuffix
    );
  },

  _exitDomFullscreen(data) {
    this._windowUtils.remoteFrameFullscreenReverted();
  },

  _handleOwnerEvent(evt) {
    switch (evt.type) {
      case "visibilitychange":
        this._ownerVisibilityChange();
        break;
      case "fullscreenchange":
        if (!this._window.document.fullscreenElement) {
          this._sendAsyncMsg("exit-fullscreen");
        } else if (this._pendingDOMFullscreen) {
          this._pendingDOMFullscreen = false;
          this._sendAsyncMsg("entered-fullscreen");
        }
        break;
    }
  },

  _fireFatalError() {
    let evt = this._createEvent(
      "error",
      { type: "fatal" },
      /* cancelable = */ false
    );
    this._frameElement.dispatchEvent(evt);
  },

  observe(subject, topic, data) {
    switch (topic) {
      case "oop-frameloader-crashed":
        if (this._isAlive() && subject == this._frameLoader) {
          this._fireFatalError();
        }
        break;
      case "ask-children-to-execute-copypaste-command":
        if (this._isAlive() && this._frameElement == subject.wrappedJSObject) {
          this._sendAsyncMsg("copypaste-do-command", { command: data });
        }
        break;
      case "back-docommand":
        if (this._isAlive() && this._frameLoader.visible) {
          this.goBack();
        }
        break;
      default:
        debug("Unknown topic: " + topic);
        break;
    }
  },
};

var EXPORTED_SYMBOLS = ["BrowserElementParent"];
