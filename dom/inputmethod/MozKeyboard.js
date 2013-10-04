/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/ObjectWrapper.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
  "@mozilla.org/childprocessmessagemanager;1", "nsIMessageSender");

XPCOMUtils.defineLazyServiceGetter(this, "tm",
  "@mozilla.org/thread-manager;1", "nsIThreadManager");

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
    let principal = win.document.nodePrincipal;
    let perm = Services.perms
               .testExactPermissionFromPrincipal(principal, "keyboard");
    if (perm != Ci.nsIPermissionManager.ALLOW_ACTION) {
      dump("No permission to use the keyboard API for " +
           principal.origin + "\n");
      return null;
    }

    Services.obs.addObserver(this, "inner-window-destroyed", false);
    cpmm.addMessageListener('Keyboard:FocusChange', this);
    cpmm.addMessageListener('Keyboard:SelectionChange', this);

    this._window = win;
    this._utils = win.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIDOMWindowUtils);
    this.innerWindowID = this._utils.currentInnerWindowID;
    this._focusHandler = null;
    this._selectionHandler = null;
    this._selectionStart = -1;
    this._selectionEnd = -1;
  },

  uninit: function mozKeyboardUninit() {
    Services.obs.removeObserver(this, "inner-window-destroyed");
    cpmm.removeMessageListener('Keyboard:FocusChange', this);
    cpmm.removeMessageListener('Keyboard:SelectionChange', this);

    this._window = null;
    this._utils = null;
    this._focusHandler = null;
    this._selectionHandler = null;
  },

  sendKey: function mozKeyboardSendKey(keyCode, charCode) {
    charCode = (charCode == undefined) ? keyCode : charCode;

    let mainThread = tm.mainThread;
    let utils = this._utils;

    function send(type) {
      mainThread.dispatch(function() {
	      utils.sendKeyEvent(type, keyCode, charCode, null);
      }, mainThread.DISPATCH_NORMAL);
    }

    send("keydown");
    send("keypress");
    send("keyup");
  },

  setSelectedOption: function mozKeyboardSetSelectedOption(index) {
    cpmm.sendAsyncMessage('Keyboard:SetSelectedOption', {
      'index': index
    });
  },

  setValue: function mozKeyboardSetValue(value) {
    cpmm.sendAsyncMessage('Keyboard:SetValue', {
      'value': value
    });
  },

  setSelectedOptions: function mozKeyboardSetSelectedOptions(indexes) {
    cpmm.sendAsyncMessage('Keyboard:SetSelectedOptions', {
      'indexes': indexes
    });
  },

  set onselectionchange(val) {
    this._selectionHandler = val;
  },

  get onselectionchange() {
    return this._selectionHandler;
  },

  get selectionStart() {
    return this._selectionStart;
  },

  get selectionEnd() {
    return this._selectionEnd;
  },

  setSelectionRange: function mozKeyboardSetSelectionRange(start, end) {
    cpmm.sendAsyncMessage('Keyboard:SetSelectionRange', {
      'selectionStart': start,
      'selectionEnd': end
    });
  },

  removeFocus: function mozKeyboardRemoveFocus() {
    cpmm.sendAsyncMessage('Keyboard:RemoveFocus', {});
  },

  set onfocuschange(val) {
    this._focusHandler = val;
  },

  get onfocuschange() {
    return this._focusHandler;
  },

  replaceSurroundingText: function mozKeyboardReplaceSurroundingText(
    text, beforeLength, afterLength) {
    cpmm.sendAsyncMessage('Keyboard:ReplaceSurroundingText', {
      'text': text || '',
      'beforeLength': (typeof beforeLength === 'number' ? beforeLength : 0),
      'afterLength': (typeof afterLength === 'number' ? afterLength: 0)
    });
  },

  receiveMessage: function mozKeyboardReceiveMessage(msg) {
    if (msg.name == "Keyboard:FocusChange") {
       let msgJson = msg.json;
       if (msgJson.type != "blur") {
         this._selectionStart = msgJson.selectionStart;
         this._selectionEnd = msgJson.selectionEnd;
       } else {
         this._selectionStart = 0;
         this._selectionEnd = 0;
       }

      let handler = this._focusHandler;
      if (!handler || !(handler instanceof Ci.nsIDOMEventListener))
        return;

      let detail = {
        "detail": msgJson
      };

      let evt = new this._window.CustomEvent("focuschanged",
          ObjectWrapper.wrap(detail, this._window));
      handler.handleEvent(evt);
    } else if (msg.name == "Keyboard:SelectionChange") {
      let msgJson = msg.json;

      this._selectionStart = msgJson.selectionStart;
      this._selectionEnd = msgJson.selectionEnd;

      let handler = this._selectionHandler;
      if (!handler || !(handler instanceof Ci.nsIDOMEventListener))
        return;

      let evt = new this._window.CustomEvent("selectionchange",
          ObjectWrapper.wrap({}, this._window));
      handler.handleEvent(evt);
    }
  },

  observe: function mozKeyboardObserve(subject, topic, data) {
    let wId = subject.QueryInterface(Ci.nsISupportsPRUint64).data;
    if (wId == this.innerWindowID)
      this.uninit();
  }
};

/*
 * A WeakMap to map input method iframe window to its active status.
 */
let WindowMap = {
  // WeakMap of <window, boolean> pairs.
  _map: null,

  /*
   * Check if the given window is active.
   */
  isActive: function(win) {
    if (!this._map || !win) {
      return false;
    }
    return this._map.get(win, false);
  },

  /*
   * Set the active status of the given window.
   */
  setActive: function(win, isActive) {
    if (!win) {
      return;
    }
    if (!this._map) {
      this._map = new WeakMap();
    }
    this._map.set(win, isActive);
  }
};

/**
 * ==============================================
 * InputMethodManager
 * ==============================================
 */
function MozInputMethodManager(win) {
  this._window = win;
}

MozInputMethodManager.prototype = {
  _supportsSwitching: false,
  _window: null,

  classID: Components.ID("{7e9d7280-ef86-11e2-b778-0800200c9a66}"),

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIInputMethodManager
  ]),

  classInfo: XPCOMUtils.generateCI({
    "classID": Components.ID("{7e9d7280-ef86-11e2-b778-0800200c9a66}"),
    "contractID": "@mozilla.org/b2g-imm;1",
    "interfaces": [Ci.nsIInputMethodManager],
    "flags": Ci.nsIClassInfo.DOM_OBJECT,
    "classDescription": "B2G Input Method Manager"
  }),

  showAll: function() {
    if (!WindowMap.isActive(this._window)) {
      return;
    }
    cpmm.sendAsyncMessage('Keyboard:ShowInputMethodPicker', {});
  },

  next: function() {
    if (!WindowMap.isActive(this._window)) {
      return;
    }
    cpmm.sendAsyncMessage('Keyboard:SwitchToNextInputMethod', {});
  },

  supportsSwitching: function() {
    if (!WindowMap.isActive(this._window)) {
      return false;
    }
    return this._supportsSwitching;
  },

  hide: function() {
    if (!WindowMap.isActive(this._window)) {
      return;
    }
    cpmm.sendAsyncMessage('Keyboard:RemoveFocus', {});
  }
};

/**
 * ==============================================
 * InputMethod
 * ==============================================
 */
function MozInputMethod() { }

MozInputMethod.prototype = {
  _inputcontext: null,
  _layouts: {},
  _window: null,

  classID: Components.ID("{4607330d-e7d2-40a4-9eb8-43967eae0142}"),

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIInputMethod,
    Ci.nsIDOMGlobalPropertyInitializer,
    Ci.nsIObserver
  ]),

  classInfo: XPCOMUtils.generateCI({
    "classID": Components.ID("{4607330d-e7d2-40a4-9eb8-43967eae0142}"),
    "contractID": "@mozilla.org/b2g-inputmethod;1",
    "interfaces": [Ci.nsIInputMethod],
    "flags": Ci.nsIClassInfo.DOM_OBJECT,
    "classDescription": "B2G Input Method"
  }),

  init: function mozInputMethodInit(win) {
    this._window = win;
    this._mgmt = new MozInputMethodManager(win);
    this.innerWindowID = win.QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIDOMWindowUtils)
                            .currentInnerWindowID;

    Services.obs.addObserver(this, "inner-window-destroyed", false);
    cpmm.addMessageListener('Keyboard:FocusChange', this);
    cpmm.addMessageListener('Keyboard:SelectionChange', this);
    cpmm.addMessageListener('Keyboard:GetContext:Result:OK', this);
    cpmm.addMessageListener('Keyboard:LayoutsChange', this);
  },

  uninit: function mozInputMethodUninit() {
    Services.obs.removeObserver(this, "inner-window-destroyed");
    cpmm.removeMessageListener('Keyboard:FocusChange', this);
    cpmm.removeMessageListener('Keyboard:SelectionChange', this);
    cpmm.removeMessageListener('Keyboard:GetContext:Result:OK', this);
    cpmm.removeMessageListener('Keyboard:LayoutsChange', this);

    this._window = null;
    this._mgmt = null;
  },

  receiveMessage: function mozInputMethodReceiveMsg(msg) {
    if (!WindowMap.isActive(this._window)) {
      return;
    }

    let json = msg.json;

    switch(msg.name) {
      case 'Keyboard:FocusChange':
        if (json.type !== 'blur') {
          // XXX Bug 904339 could receive 'text' event twice
          this.setInputContext(json);
        }
        else {
          this.setInputContext(null);
        }
        break;
      case 'Keyboard:SelectionChange':
        this._inputcontext.updateSelectionContext(json);
        break;
      case 'Keyboard:GetContext:Result:OK':
        this.setInputContext(json);
        break;
      case 'Keyboard:LayoutsChange':
        this._layouts = json;
        break;
    }
  },

  observe: function mozInputMethodObserve(subject, topic, data) {
    let wId = subject.QueryInterface(Ci.nsISupportsPRUint64).data;
    if (wId == this.innerWindowID)
      this.uninit();
  },

  get mgmt() {
    if (!WindowMap.isActive(this._window)) {
      return null;
    }

    return this._mgmt;
  },

  get inputcontext() {
    if (!WindowMap.isActive(this._window)) {
      return null;
    }
    return this._inputcontext;
  },

  set oninputcontextchange(handler) {
    this.__DOM_IMPL__.setEventHandler("oninputcontextchange", handler);
  },

  get oninputcontextchange() {
    return this.__DOM_IMPL__.getEventHandler("oninputcontextchange");
  },

  setInputContext: function mozKeyboardContextChange(data) {
    if (this._inputcontext) {
      this._inputcontext.destroy();
      this._inputcontext = null;
      this._mgmt._supportsSwitching = false;
    }

    if (data) {
      this._mgmt._supportsSwitching = this._layouts[data.type] ?
        this._layouts[data.type] > 1 :
        false;

      this._inputcontext = new MozInputContext(data);
      this._inputcontext.init(this._window);
    }

    let event = new this._window.Event("inputcontextchange",
                                       ObjectWrapper.wrap({}, this._window));
    this.__DOM_IMPL__.dispatchEvent(event);
  },

  setActive: function mozInputMethodSetActive(isActive) {
    if (WindowMap.isActive(this._window) === isActive) {
      return;
    }

    WindowMap.setActive(this._window, isActive);

    if (isActive) {
      // Activate current input method.
      // If there is already an active context, then this will trigger
      // a GetContext:Result:OK event, and we can initialize ourselves.
      // Otherwise silently ignored.
      cpmm.sendAsyncMessage("Keyboard:GetContext", {});
    } else {
      // Deactive current input method.
      if (this._inputcontext) {
        this.setInputContext(null);
      }
    }
  }
};

 /**
 * ==============================================
 * InputContext
 * ==============================================
 */
function MozInputContext(ctx) {
  this._context = {
    inputtype: ctx.type,
    inputmode: ctx.inputmode,
    lang: ctx.lang,
    type: ["textarea", "contenteditable"].indexOf(ctx.type) > -1 ?
              ctx.type :
              "text",
    selectionStart: ctx.selectionStart,
    selectionEnd: ctx.selectionEnd,
    textBeforeCursor: ctx.textBeforeCursor,
    textAfterCursor: ctx.textAfterCursor
  };

  this._contextId = ctx.contextId;
}

MozInputContext.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  _window: null,
  _context: null,
  _contextId: -1,

  classID: Components.ID("{1e38633d-d08b-4867-9944-afa5c648adb6}"),

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIB2GInputContext,
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference
  ]),

  classInfo: XPCOMUtils.generateCI({
    "classID": Components.ID("{1e38633d-d08b-4867-9944-afa5c648adb6}"),
    "contractID": "@mozilla.org/b2g-inputcontext;1",
    "interfaces": [Ci.nsIB2GInputContext],
    "flags": Ci.nsIClassInfo.DOM_OBJECT,
    "classDescription": "B2G Input Context"
  }),

  init: function ic_init(win) {
    this._window = win;
    this._utils = win.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIDOMWindowUtils);
    this.initDOMRequestHelper(win,
      ["Keyboard:GetText:Result:OK",
       "Keyboard:GetText:Result:Error",
       "Keyboard:SetSelectionRange:Result:OK",
       "Keyboard:ReplaceSurroundingText:Result:OK",
       "Keyboard:SendKey:Result:OK",
       "Keyboard:SetComposition:Result:OK",
       "Keyboard:EndComposition:Result:OK",
       "Keyboard:SequenceError"]);
  },

  destroy: function ic_destroy() {
    let self = this;

    // All requests that are still pending need to be invalidated
    // because the context is no longer valid.
    this.forEachPromiseResolver(function(k) {
      self.takePromiseResolver(k).reject("InputContext got destroyed");
    });
    this.destroyDOMRequestHelper();

    // A consuming application might still hold a cached version of
    // this object. After destroying all methods will throw because we
    // cannot create new promises anymore, but we still hold
    // (outdated) information in the context. So let's clear that out.
    for (var k in this._context) {
      if (this._context.hasOwnProperty(k)) {
        this._context[k] = null;
      }
    }

    this._window = null;
  },

  receiveMessage: function ic_receiveMessage(msg) {
    if (!msg || !msg.json) {
      dump('InputContext received message without data\n');
      return;
    }

    let json = msg.json;
    let resolver = this.takePromiseResolver(json.requestId);

    if (!resolver) {
      return;
    }

    switch (msg.name) {
      case "Keyboard:SendKey:Result:OK":
        resolver.resolve();
        break;
      case "Keyboard:GetText:Result:OK":
        resolver.resolve(json.text);
        break;
      case "Keyboard:GetText:Result:Error":
        resolver.reject(json.error);
        break;
      case "Keyboard:SetSelectionRange:Result:OK":
      case "Keyboard:ReplaceSurroundingText:Result:OK":
        resolver.resolve(
          ObjectWrapper.wrap(json.selectioninfo, this._window));
        break;
      case "Keyboard:SequenceError":
        // Occurs when a new element got focus, but the inputContext was
        // not invalidated yet...
        resolver.reject("InputContext has expired");
        break;
      case "Keyboard:SetComposition:Result:OK": // Fall through.
      case "Keyboard:EndComposition:Result:OK":
        resolver.resolve();
        break;
      default:
        dump("Could not find a handler for " + msg.name);
        resolver.reject();
        break;
    }
  },

  updateSelectionContext: function ic_updateSelectionContext(ctx) {
    if (!this._context) {
      return;
    }

    let selectionDirty = this._context.selectionStart !== ctx.selectionStart ||
          this._context.selectionEnd !== ctx.selectionEnd;
    let surroundDirty = this._context.textBeforeCursor !== ctx.textBeforeCursor ||
          this._context.textAfterCursor !== ctx.textAfterCursor;

    this._context.selectionStart = ctx.selectionStart;
    this._context.selectionEnd = ctx.selectionEnd;
    this._context.textBeforeCursor = ctx.textBeforeCursor;
    this._context.textAfterCursor = ctx.textAfterCursor;

    if (selectionDirty) {
      this._fireEvent("selectionchange", {
        selectionStart: ctx.selectionStart,
        selectionEnd: ctx.selectionEnd
      });
    }

    if (surroundDirty) {
      this._fireEvent("surroundingtextchange", {
        beforeString: ctx.textBeforeCursor,
        afterString: ctx.textAfterCursor
      });
    }
  },

  _fireEvent: function ic_fireEvent(eventName, aDetail) {
    let detail = {
      detail: aDetail
    };

    let event = new this._window.Event(eventName,
                                       ObjectWrapper.wrap(aDetail, this._window));
    this.__DOM_IMPL__.dispatchEvent(event);
  },

  // tag name of the input field
  get type() {
    return this._context.type;
  },

  // type of the input field
  get inputType() {
    return this._context.inputtype;
  },

  get inputMode() {
    return this._context.inputmode;
  },

  get lang() {
    return this._context.lang;
  },

  getText: function ic_getText(offset, length) {
    let self = this;
    return this._sendPromise(function(resolverId) {
      cpmm.sendAsyncMessage('Keyboard:GetText', {
        contextId: self._contextId,
        requestId: resolverId,
        offset: offset,
        length: length
      });
    });
  },

  get selectionStart() {
    return this._context.selectionStart;
  },

  get selectionEnd() {
    return this._context.selectionEnd;
  },

  get textBeforeCursor() {
    return this._context.textBeforeCursor;
  },

  get textAfterCursor() {
    return this._context.textAfterCursor;
  },

  setSelectionRange: function ic_setSelectionRange(start, length) {
    let self = this;
    return this._sendPromise(function(resolverId) {
      cpmm.sendAsyncMessage("Keyboard:SetSelectionRange", {
        contextId: self._contextId,
        requestId: resolverId,
        selectionStart: start,
        selectionEnd: start + length
      });
    });
  },

  get onsurroundingtextchange() {
    return this.__DOM_IMPL__.getEventHandler("onsurroundingtextchange");
  },

  set onsurroundingtextchange(handler) {
    this.__DOM_IMPL__.setEventHandler("onsurroundingtextchange");
  },

  get onselectionchange() {
    return this.__DOM_IMPL__.getEventHandler("onselectionchange");
  },

  set onselectionchange(handler) {
    this.__DOM_IMPL__.setEventHandler("onselectionchange");
  },

  replaceSurroundingText: function ic_replaceSurrText(text, offset, length) {
    let self = this;
    return this._sendPromise(function(resolverId) {
      cpmm.sendAsyncMessage('Keyboard:ReplaceSurroundingText', {
        contextId: self._contextId,
        requestId: resolverId,
        text: text,
        beforeLength: offset || 0,
        afterLength: length || 0
      });
    });
  },

  deleteSurroundingText: function ic_deleteSurrText(offset, length) {
    return this.replaceSurroundingText(null, offset, length);
  },

  sendKey: function ic_sendKey(keyCode, charCode, modifiers) {
    let self = this;
    return this._sendPromise(function(resolverId) {
      cpmm.sendAsyncMessage('Keyboard:SendKey', {
        contextId: self._contextId,
        requestId: resolverId,
        keyCode: keyCode,
        charCode: charCode,
        modifiers: modifiers
      });
    });
  },

  setComposition: function ic_setComposition(text, cursor, clauses) {
    let self = this;
    return this._sendPromise(function(resolverId) {
      cpmm.sendAsyncMessage('Keyboard:SetComposition', {
        contextId: self._contextId,
        requestId: resolverId,
        text: text,
        cursor: cursor || text.length,
        clauses: clauses || null
      });
    });
  },

  endComposition: function ic_endComposition(text) {
    let self = this;
    return this._sendPromise(function(resolverId) {
      cpmm.sendAsyncMessage('Keyboard:EndComposition', {
        contextId: self._contextId,
        requestId: resolverId,
        text: text || ''
      });
    });
  },

  _sendPromise: function(callback) {
    let self = this;
    return this.createPromise(function(resolve, reject) {
      let resolverId = self.getPromiseResolverId({ resolve: resolve, reject: reject });
      if (!WindowMap.isActive(self._window)) {
        self.removePromiseResolver(resolverId);
        reject('Input method is not active.');
        return;
      }
      callback(resolverId);
    });
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory(
  [MozKeyboard, MozInputMethod]);
