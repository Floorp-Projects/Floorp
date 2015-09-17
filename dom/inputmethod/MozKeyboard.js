/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
  "@mozilla.org/childprocessmessagemanager;1", "nsISyncMessageSender");

XPCOMUtils.defineLazyServiceGetter(this, "tm",
  "@mozilla.org/thread-manager;1", "nsIThreadManager");

/*
 * A WeakMap to map input method iframe window to
 * it's active status, kbID, and ipcHelper.
 */
var WindowMap = {
  // WeakMap of <window, object> pairs.
  _map: null,

  /*
   * Set the object associated to the window and return it.
   */
  _getObjForWin: function(win) {
    if (!this._map) {
      this._map = new WeakMap();
    }
    if (this._map.has(win)) {
      return this._map.get(win);
    } else {
      let obj = {
        active: false,
        kbID: undefined,
        ipcHelper: null
      };
      this._map.set(win, obj);

      return obj;
    }
  },

  /*
   * Check if the given window is active.
   */
  isActive: function(win) {
    if (!this._map || !win) {
      return false;
    }

    return this._getObjForWin(win).active;
  },

  /*
   * Set the active status of the given window.
   */
  setActive: function(win, isActive) {
    if (!win) {
      return;
    }
    let obj = this._getObjForWin(win);
    obj.active = isActive;
  },

  /*
   * Get the keyboard ID (assigned by Keyboard.jsm) of the given window.
   */
  getKbID: function(win) {
    if (!this._map || !win) {
      return undefined;
    }

    let obj = this._getObjForWin(win);
    return obj.kbID;
  },

  /*
   * Set the keyboard ID (assigned by Keyboard.jsm) of the given window.
   */
  setKbID: function(win, kbID) {
    if (!win) {
      return;
    }
    let obj = this._getObjForWin(win);
    obj.kbID = kbID;
  },

  /*
   * Get InputContextDOMRequestIpcHelper instance attached to this window.
   */
  getInputContextIpcHelper: function(win) {
    if (!win) {
      return;
    }
    let obj = this._getObjForWin(win);
    if (!obj.ipcHelper) {
      obj.ipcHelper = new InputContextDOMRequestIpcHelper(win);
    }
    return obj.ipcHelper;
  },

  /*
   * Unset InputContextDOMRequestIpcHelper instance.
   */
  unsetInputContextIpcHelper: function(win) {
    if (!win) {
      return;
    }
    let obj = this._getObjForWin(win);
    if (!obj.ipcHelper) {
      return;
    }
    obj.ipcHelper = null;
  }
};

var cpmmSendAsyncMessageWithKbID = function (self, msg, data) {
  data.kbID = WindowMap.getKbID(self._window);
  cpmm.sendAsyncMessage(msg, data);
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
  supportsSwitchingForCurrentInputContext: false,
  _window: null,

  classID: Components.ID("{7e9d7280-ef86-11e2-b778-0800200c9a66}"),

  QueryInterface: XPCOMUtils.generateQI([]),

  set oninputcontextfocus(handler) {
    this.__DOM_IMPL__.setEventHandler("oninputcontextfocus", handler);
  },

  get oninputcontextfocus() {
    return this.__DOM_IMPL__.getEventHandler("oninputcontextfocus");
  },

  set oninputcontextblur(handler) {
    this.__DOM_IMPL__.setEventHandler("oninputcontextblur", handler);
  },

  get oninputcontextblur() {
    return this.__DOM_IMPL__.getEventHandler("oninputcontextblur");
  },

  set onshowallrequest(handler) {
    this.__DOM_IMPL__.setEventHandler("onshowallrequest", handler);
  },

  get onshowallrequest() {
    return this.__DOM_IMPL__.getEventHandler("onshowallrequest");
  },

  set onnextrequest(handler) {
    this.__DOM_IMPL__.setEventHandler("onnextrequest", handler);
  },

  get onnextrequest() {
    return this.__DOM_IMPL__.getEventHandler("onnextrequest");
  },

  set onaddinputrequest(handler) {
    this.__DOM_IMPL__.setEventHandler("onaddinputrequest", handler);
  },

  get onaddinputrequest() {
    return this.__DOM_IMPL__.getEventHandler("onaddinputrequest");
  },

  set onremoveinputrequest(handler) {
    this.__DOM_IMPL__.setEventHandler("onremoveinputrequest", handler);
  },

  get onremoveinputrequest() {
    return this.__DOM_IMPL__.getEventHandler("onremoveinputrequest");
  },

  showAll: function() {
    if (!WindowMap.isActive(this._window)) {
      return;
    }
    cpmmSendAsyncMessageWithKbID(this, 'Keyboard:ShowInputMethodPicker', {});
  },

  next: function() {
    if (!WindowMap.isActive(this._window)) {
      return;
    }
    cpmmSendAsyncMessageWithKbID(this, 'Keyboard:SwitchToNextInputMethod', {});
  },

  supportsSwitching: function() {
    if (!WindowMap.isActive(this._window)) {
      return false;
    }
    return this.supportsSwitchingForCurrentInputContext;
  },

  hide: function() {
    if (!WindowMap.isActive(this._window)) {
      return;
    }
    cpmmSendAsyncMessageWithKbID(this, 'Keyboard:RemoveFocus', {});
  },

  setSupportsSwitchingTypes: function(types) {
    cpmm.sendAsyncMessage('System:SetSupportsSwitchingTypes', {
      types: types
    });
  },

  handleFocus: function(data) {
    let detail = new MozInputContextFocusEventDetail(this._window, data);
    let wrappedDetail =
      this._window.MozInputContextFocusEventDetail._create(this._window, detail);
    let event = new this._window.CustomEvent('inputcontextfocus',
      { cancelable: true, detail: wrappedDetail });

    let handled = !this.__DOM_IMPL__.dispatchEvent(event);

    // A gentle warning if the event is not preventDefault() by the content.
    if (!handled) {
      dump('MozKeyboard.js: A frame with input-manage permission did not' +
        ' handle the inputcontextfocus event dispatched.\n');
    }
  },

  handleBlur: function(data) {
    let event =
      new this._window.Event('inputcontextblur', { cancelable: true });

    let handled = !this.__DOM_IMPL__.dispatchEvent(event);

    // A gentle warning if the event is not preventDefault() by the content.
    if (!handled) {
      dump('MozKeyboard.js: A frame with input-manage permission did not' +
        ' handle the inputcontextblur event dispatched.\n');
    }
  },

  dispatchShowAllRequestEvent: function() {
    this._fireSimpleEvent('showallrequest');
  },

  dispatchNextRequestEvent: function() {
    this._fireSimpleEvent('nextrequest');
  },

  _fireSimpleEvent: function(eventType) {
    let event = new this._window.Event(eventType);
    let handled = !this.__DOM_IMPL__.dispatchEvent(event, { cancelable: true });

    // A gentle warning if the event is not preventDefault() by the content.
    if (!handled) {
      dump('MozKeyboard.js: A frame with input-manage permission did not' +
        ' handle the ' + eventType + ' event dispatched.\n');
    }
  },

  handleAddInput: function(data) {
    let p = this._fireInputRegistryEvent('addinputrequest', data);
    if (!p) {
      return;
    }

    p.then(() => {
      cpmm.sendAsyncMessage('System:InputRegistry:Add:Done', {
        id: data.id
      });
    }, (error) => {
      cpmm.sendAsyncMessage('System:InputRegistry:Add:Done', {
        id: data.id,
        error: error || 'Unknown Error'
      });
    });
  },

  handleRemoveInput: function(data) {
    let p = this._fireInputRegistryEvent('removeinputrequest', data);
    if (!p) {
      return;
    }

    p.then(() => {
      cpmm.sendAsyncMessage('System:InputRegistry:Remove:Done', {
        id: data.id
      });
    }, (error) => {
      cpmm.sendAsyncMessage('System:InputRegistry:Remove:Done', {
        id: data.id,
        error: error || 'Unknown Error'
      });
    });
  },

  _fireInputRegistryEvent: function(eventType, data) {
    let detail = new MozInputRegistryEventDetail(this._window, data);
    let wrappedDetail =
      this._window.MozInputRegistryEventDetail._create(this._window, detail);
    let event = new this._window.CustomEvent(eventType,
      { cancelable: true, detail: wrappedDetail });
    let handled = !this.__DOM_IMPL__.dispatchEvent(event);

    // A gentle warning if the event is not preventDefault() by the content.
    if (!handled) {
      dump('MozKeyboard.js: A frame with input-manage permission did not' +
        ' handle the ' + eventType + ' event dispatched.\n');

      return null;
    }
    return detail.takeChainedPromise();
  }
};

function MozInputContextFocusEventDetail(win, data) {
  this.type = data.type;
  this.inputType = data.inputType;
  this.value = data.value;
  // Exposed as MozInputContextChoicesInfo dictionary defined in WebIDL
  this.choices = data.choices;
  this.min = data.min;
  this.max = data.max;
}
MozInputContextFocusEventDetail.prototype = {
  classID: Components.ID("{e0794208-ac50-40e8-b22e-6ee0b4c4e6e8}"),
  QueryInterface: XPCOMUtils.generateQI([]),

  type: undefined,
  inputType: undefined,
  value: '',
  choices: null,
  min: undefined,
  max: undefined
};

function MozInputRegistryEventDetail(win, data) {
  this._window = win;

  this.manifestURL = data.manifestURL;
  this.inputId = data.inputId;
  // Exposed as MozInputMethodInputManifest dictionary defined in WebIDL
  this.inputManifest = data.inputManifest;

  this._chainedPromise = Promise.resolve();
}
MozInputRegistryEventDetail.prototype = {
  classID: Components.ID("{02130070-9b3e-4f38-bbd9-f0013aa36717}"),
  QueryInterface: XPCOMUtils.generateQI([]),

  _window: null,

  manifestURL: undefined,
  inputId: undefined,
  inputManifest: null,

  waitUntil: function(p) {
    // Need an extra protection here since waitUntil will be an no-op
    // when chainedPromise is already returned.
    if (!this._chainedPromise) {
      throw new this._window.DOMException(
        'Must call waitUntil() within the event handling loop.',
        'InvalidStateError');
    }

    this._chainedPromise = this._chainedPromise
      .then(function() { return p; });
  },

  takeChainedPromise: function() {
    var p = this._chainedPromise;
    this._chainedPromise = null;
    return p;
  }
};

/**
 * ==============================================
 * InputMethod
 * ==============================================
 */
function MozInputMethod() { }

MozInputMethod.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  _window: null,
  _inputcontext: null,
  _wrappedInputContext: null,
  _mgmt: null,
  _wrappedMgmt: null,
  _supportsSwitchingTypes: [],
  _inputManageId: undefined,

  classID: Components.ID("{4607330d-e7d2-40a4-9eb8-43967eae0142}"),

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIDOMGlobalPropertyInitializer,
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference
  ]),

  init: function mozInputMethodInit(win) {
    this._window = win;
    this._mgmt = new MozInputMethodManager(win);
    this._wrappedMgmt = win.MozInputMethodManager._create(win, this._mgmt);
    this.innerWindowID = win.QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIDOMWindowUtils)
                            .currentInnerWindowID;

    Services.obs.addObserver(this, "inner-window-destroyed", false);

    cpmm.addWeakMessageListener('Keyboard:Focus', this);
    cpmm.addWeakMessageListener('Keyboard:Blur', this);
    cpmm.addWeakMessageListener('Keyboard:SelectionChange', this);
    cpmm.addWeakMessageListener('Keyboard:GetContext:Result:OK', this);
    cpmm.addWeakMessageListener('Keyboard:SupportsSwitchingTypesChange', this);
    cpmm.addWeakMessageListener('InputRegistry:Result:OK', this);
    cpmm.addWeakMessageListener('InputRegistry:Result:Error', this);

    if (this._hasInputManagePerm(win)) {
      this._inputManageId = cpmm.sendSyncMessage('System:RegisterSync', {})[0];
      cpmm.addWeakMessageListener('System:Focus', this);
      cpmm.addWeakMessageListener('System:Blur', this);
      cpmm.addWeakMessageListener('System:ShowAll', this);
      cpmm.addWeakMessageListener('System:Next', this);
      cpmm.addWeakMessageListener('System:InputRegistry:Add', this);
      cpmm.addWeakMessageListener('System:InputRegistry:Remove', this);
    }
  },

  uninit: function mozInputMethodUninit() {
    this._window = null;
    this._mgmt = null;
    this._wrappedMgmt = null;

    cpmm.removeWeakMessageListener('Keyboard:Focus', this);
    cpmm.removeWeakMessageListener('Keyboard:Blur', this);
    cpmm.removeWeakMessageListener('Keyboard:SelectionChange', this);
    cpmm.removeWeakMessageListener('Keyboard:GetContext:Result:OK', this);
    cpmm.removeWeakMessageListener('Keyboard:SupportsSwitchingTypesChange', this);
    cpmm.removeWeakMessageListener('InputRegistry:Result:OK', this);
    cpmm.removeWeakMessageListener('InputRegistry:Result:Error', this);
    this.setActive(false);

    if (typeof this._inputManageId === 'number') {
      cpmm.sendAsyncMessage('System:Unregister', {
        'id': this._inputManageId
      });
      cpmm.removeWeakMessageListener('System:Focus', this);
      cpmm.removeWeakMessageListener('System:Blur', this);
      cpmm.removeWeakMessageListener('System:ShowAll', this);
      cpmm.removeWeakMessageListener('System:Next', this);
      cpmm.removeWeakMessageListener('System:InputRegistry:Add', this);
      cpmm.removeWeakMessageListener('System:InputRegistry:Remove', this);
    }
  },

  receiveMessage: function mozInputMethodReceiveMsg(msg) {
    if (msg.name.startsWith('Keyboard') &&
        !WindowMap.isActive(this._window)) {
      return;
    }

    let data = msg.data;

    if (msg.name.startsWith('System') &&
      this._inputManageId !== data.inputManageId) {
      return;
    }
    delete data.inputManageId;

    let resolver = ('requestId' in data) ?
      this.takePromiseResolver(data.requestId) : null;

    switch(msg.name) {
      case 'Keyboard:Focus':
        // XXX Bug 904339 could receive 'text' event twice
        this.setInputContext(data);
        break;
      case 'Keyboard:Blur':
        this.setInputContext(null);
        break;
      case 'Keyboard:SelectionChange':
        if (this.inputcontext) {
          this._inputcontext.updateSelectionContext(data, false);
        }
        break;
      case 'Keyboard:GetContext:Result:OK':
        this.setInputContext(data);
        break;
      case 'Keyboard:SupportsSwitchingTypesChange':
        this._supportsSwitchingTypes = data.types;
        break;

      case 'InputRegistry:Result:OK':
        resolver.resolve();

        break;

      case 'InputRegistry:Result:Error':
        resolver.reject(data.error);

        break;

      case 'System:Focus':
        this._mgmt.handleFocus(data);
        break;

      case 'System:Blur':
        this._mgmt.handleBlur(data);
        break;

      case 'System:ShowAll':
        this._mgmt.dispatchShowAllRequestEvent();
        break;

      case 'System:Next':
        this._mgmt.dispatchNextRequestEvent();
        break;

      case 'System:InputRegistry:Add':
        this._mgmt.handleAddInput(data);
        break;

      case 'System:InputRegistry:Remove':
        this._mgmt.handleRemoveInput(data);
        break;
    }
  },

  observe: function mozInputMethodObserve(subject, topic, data) {
    let wId = subject.QueryInterface(Ci.nsISupportsPRUint64).data;
    if (wId == this.innerWindowID)
      this.uninit();
  },

  get mgmt() {
    return this._wrappedMgmt;
  },

  get inputcontext() {
    if (!WindowMap.isActive(this._window)) {
      return null;
    }
    return this._wrappedInputContext;
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
      this._wrappedInputContext = null;
      this._mgmt.supportsSwitchingForCurrentInputContext = false;
    }

    if (data) {
      this._mgmt.supportsSwitchingForCurrentInputContext =
        (this._supportsSwitchingTypes.indexOf(data.inputType) !== -1);

      this._inputcontext = new MozInputContext(data);
      this._inputcontext.init(this._window);
      // inputcontext will be exposed as a WebIDL object. Create its
      // content-side object explicitly to avoid Bug 1001325.
      this._wrappedInputContext =
        this._window.MozInputContext._create(this._window, this._inputcontext);
    }

    let event = new this._window.Event("inputcontextchange");
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

      // get keyboard ID from Keyboard.jsm,
      // or if we already have it, get it from our map
      // Note: if we need to get it from Keyboard.jsm,
      // we have to use a synchronous message
      var kbID = WindowMap.getKbID(this._window);
      if (kbID) {
        cpmmSendAsyncMessageWithKbID(this, 'Keyboard:RegisterSync', {});
      } else {
        let res = cpmm.sendSyncMessage('Keyboard:RegisterSync', {});
        WindowMap.setKbID(this._window, res[0]);
      }

      cpmmSendAsyncMessageWithKbID(this, 'Keyboard:GetContext', {});
    } else {
      // Deactive current input method.
      cpmmSendAsyncMessageWithKbID(this, 'Keyboard:Unregister', {});
      if (this._inputcontext) {
        this.setInputContext(null);
      }
    }
  },

  addInput: function(inputId, inputManifest) {
    return this.createPromiseWithId(function(resolverId) {
      let appId = this._window.document.nodePrincipal.appId;

      cpmm.sendAsyncMessage('InputRegistry:Add', {
        requestId: resolverId,
        inputId: inputId,
        inputManifest: inputManifest,
        appId: appId
      });
    }.bind(this));
  },

  removeInput: function(inputId) {
    return this.createPromiseWithId(function(resolverId) {
      let appId = this._window.document.nodePrincipal.appId;

      cpmm.sendAsyncMessage('InputRegistry:Remove', {
        requestId: resolverId,
        inputId: inputId,
        appId: appId
      });
    }.bind(this));
  },

  setValue: function(value) {
    cpmm.sendAsyncMessage('System:SetValue', {
      'value': value
    });
  },

  setSelectedOption: function(index) {
    cpmm.sendAsyncMessage('System:SetSelectedOption', {
      'index': index
    });
  },

  setSelectedOptions: function(indexes) {
    cpmm.sendAsyncMessage('System:SetSelectedOptions', {
      'indexes': indexes
    });
  },

  removeFocus: function() {
    cpmm.sendAsyncMessage('System:RemoveFocus', {});
  },

  _hasInputManagePerm: function(win) {
    let principal = win.document.nodePrincipal;
    let perm = Services.perms.testExactPermissionFromPrincipal(principal,
                                                               "input-manage");
    return (perm === Ci.nsIPermissionManager.ALLOW_ACTION);
  }
};

 /**
 * ==============================================
 * InputContextDOMRequestIpcHelper
 * ==============================================
 */
function InputContextDOMRequestIpcHelper(win) {
  this.initDOMRequestHelper(win,
    ["Keyboard:GetText:Result:OK",
     "Keyboard:GetText:Result:Error",
     "Keyboard:SetSelectionRange:Result:OK",
     "Keyboard:ReplaceSurroundingText:Result:OK",
     "Keyboard:SendKey:Result:OK",
     "Keyboard:SendKey:Result:Error",
     "Keyboard:SetComposition:Result:OK",
     "Keyboard:EndComposition:Result:OK",
     "Keyboard:SequenceError"]);
}

InputContextDOMRequestIpcHelper.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,
  _inputContext: null,

  attachInputContext: function(inputCtx) {
    if (this._inputContext) {
      throw new Error("InputContextDOMRequestIpcHelper: detach the context first.");
    }

    this._inputContext = inputCtx;
  },

  // Unset ourselves when the window is destroyed.
  uninit: function() {
    WindowMap.unsetInputContextIpcHelper(this._window);
  },

  detachInputContext: function() {
    // All requests that are still pending need to be invalidated
    // because the context is no longer valid.
    this.forEachPromiseResolver(k => {
      this.takePromiseResolver(k).reject("InputContext got destroyed");
    });

    this._inputContext = null;
  },

  receiveMessage: function(msg) {
    if (!this._inputContext) {
      dump('InputContextDOMRequestIpcHelper received message without context attached.\n');
      return;
    }

    this._inputContext.receiveMessage(msg);
  }
};

 /**
 * ==============================================
 * InputContext
 * ==============================================
 */
function MozInputContext(ctx) {
  this._context = {
    type: ctx.type,
    inputType: ctx.inputType,
    inputMode: ctx.inputMode,
    lang: ctx.lang,
    selectionStart: ctx.selectionStart,
    selectionEnd: ctx.selectionEnd,
    textBeforeCursor: ctx.textBeforeCursor,
    textAfterCursor: ctx.textAfterCursor
  };

  this._contextId = ctx.contextId;
}

MozInputContext.prototype = {
  _window: null,
  _context: null,
  _contextId: -1,
  _ipcHelper: null,

  classID: Components.ID("{1e38633d-d08b-4867-9944-afa5c648adb6}"),

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference
  ]),

  init: function ic_init(win) {
    this._window = win;

    this._ipcHelper = WindowMap.getInputContextIpcHelper(win);
    this._ipcHelper.attachInputContext(this);
  },

  destroy: function ic_destroy() {
    // A consuming application might still hold a cached version of
    // this object. After destroying all methods will throw because we
    // cannot create new promises anymore, but we still hold
    // (outdated) information in the context. So let's clear that out.
    for (var k in this._context) {
      if (this._context.hasOwnProperty(k)) {
        this._context[k] = null;
      }
    }

    this._ipcHelper.detachInputContext();
    this._ipcHelper = null;

    this._window = null;
  },

  receiveMessage: function ic_receiveMessage(msg) {
    if (!msg || !msg.json) {
      dump('InputContext received message without data\n');
      return;
    }

    let json = msg.json;
    let resolver = this._ipcHelper.takePromiseResolver(json.requestId);

    if (!resolver) {
      dump('InputContext received invalid requestId.\n');
      return;
    }

    // Update context first before resolving promise to avoid race condition
    if (json.selectioninfo) {
      this.updateSelectionContext(json.selectioninfo, true);
    }

    switch (msg.name) {
      case "Keyboard:SendKey:Result:OK":
        resolver.resolve(true);
        break;
      case "Keyboard:SendKey:Result:Error":
        resolver.reject(json.error);
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
          Cu.cloneInto(json.selectioninfo, this._window));
        break;
      case "Keyboard:SequenceError":
        // Occurs when a new element got focus, but the inputContext was
        // not invalidated yet...
        resolver.reject("InputContext has expired");
        break;
      case "Keyboard:SetComposition:Result:OK": // Fall through.
      case "Keyboard:EndComposition:Result:OK":
        resolver.resolve(true);
        break;
      default:
        dump("Could not find a handler for " + msg.name);
        resolver.reject();
        break;
    }
  },

  updateSelectionContext: function ic_updateSelectionContext(ctx, ownAction) {
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
        selectionEnd: ctx.selectionEnd,
        ownAction: ownAction
      });
    }

    if (surroundDirty) {
      this._fireEvent("surroundingtextchange", {
        beforeString: ctx.textBeforeCursor,
        afterString: ctx.textAfterCursor,
        ownAction: ownAction
      });
    }
  },

  _fireEvent: function ic_fireEvent(eventName, aDetail) {
    let detail = {
      detail: aDetail
    };

    let event = new this._window.CustomEvent(eventName,
                                             Cu.cloneInto(detail, this._window));
    this.__DOM_IMPL__.dispatchEvent(event);
  },

  // tag name of the input field
  get type() {
    return this._context.type;
  },

  // type of the input field
  get inputType() {
    return this._context.inputType;
  },

  get inputMode() {
    return this._context.inputMode;
  },

  get lang() {
    return this._context.lang;
  },

  getText: function ic_getText(offset, length) {
    let self = this;
    return this._sendPromise(function(resolverId) {
      cpmmSendAsyncMessageWithKbID(self, 'Keyboard:GetText', {
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
      cpmmSendAsyncMessageWithKbID(self, 'Keyboard:SetSelectionRange', {
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
    this.__DOM_IMPL__.setEventHandler("onsurroundingtextchange", handler);
  },

  get onselectionchange() {
    return this.__DOM_IMPL__.getEventHandler("onselectionchange");
  },

  set onselectionchange(handler) {
    this.__DOM_IMPL__.setEventHandler("onselectionchange", handler);
  },

  replaceSurroundingText: function ic_replaceSurrText(text, offset, length) {
    let self = this;
    return this._sendPromise(function(resolverId) {
      cpmmSendAsyncMessageWithKbID(self, 'Keyboard:ReplaceSurroundingText', {
        contextId: self._contextId,
        requestId: resolverId,
        text: text,
        offset: offset || 0,
        length: length || 0
      });
    });
  },

  deleteSurroundingText: function ic_deleteSurrText(offset, length) {
    return this.replaceSurroundingText(null, offset, length);
  },

  sendKey: function ic_sendKey(dictOrKeyCode, charCode, modifiers, repeat) {
    if (typeof dictOrKeyCode === 'number') {
      // XXX: modifiers are ignored in this API method.

      return this._sendPromise((resolverId) => {
        cpmmSendAsyncMessageWithKbID(this, 'Keyboard:SendKey', {
          contextId: this._contextId,
          requestId: resolverId,
          method: 'sendKey',
          keyCode: dictOrKeyCode,
          charCode: charCode,
          repeat: repeat
        });
      });
    } else if (typeof dictOrKeyCode === 'object') {
      return this._sendPromise((resolverId) => {
        cpmmSendAsyncMessageWithKbID(this, 'Keyboard:SendKey', {
          contextId: this._contextId,
          requestId: resolverId,
          method: 'sendKey',
          keyboardEventDict: this._getkeyboardEventDict(dictOrKeyCode)
        });
      });
    } else {
      // XXX: Should not reach here; implies WebIDL binding error.
      throw new TypeError('Unknown argument passed.');
    }
  },

  keydown: function ic_keydown(dict) {
    return this._sendPromise((resolverId) => {
      cpmmSendAsyncMessageWithKbID(this, 'Keyboard:SendKey', {
        contextId: this._contextId,
         requestId: resolverId,
        method: 'keydown',
        keyboardEventDict: this._getkeyboardEventDict(dict)
       });
     });
   },

  keyup: function ic_keyup(dict) {
    return this._sendPromise((resolverId) => {
      cpmmSendAsyncMessageWithKbID(this, 'Keyboard:SendKey', {
        contextId: this._contextId,
        requestId: resolverId,
        method: 'keyup',
        keyboardEventDict: this._getkeyboardEventDict(dict)
      });
    });
  },

  setComposition: function ic_setComposition(text, cursor, clauses, dict) {
    let self = this;
    return this._sendPromise((resolverId) => {
      cpmmSendAsyncMessageWithKbID(self, 'Keyboard:SetComposition', {
        contextId: self._contextId,
        requestId: resolverId,
        text: text,
        cursor: (typeof cursor !== 'undefined') ? cursor : text.length,
        clauses: clauses || null,
        keyboardEventDict: this._getkeyboardEventDict(dict)
      });
    });
  },

  endComposition: function ic_endComposition(text, dict) {
    let self = this;
    return this._sendPromise((resolverId) => {
      cpmmSendAsyncMessageWithKbID(self, 'Keyboard:EndComposition', {
        contextId: self._contextId,
        requestId: resolverId,
        text: text || '',
        keyboardEventDict: this._getkeyboardEventDict(dict)
      });
    });
  },

  _sendPromise: function(callback) {
    let self = this;
    return this._ipcHelper.createPromiseWithId(function(aResolverId) {
      if (!WindowMap.isActive(self._window)) {
        self._ipcHelper.removePromiseResolver(aResolverId);
        reject('Input method is not active.');
        return;
      }
      callback(aResolverId);
    });
  },

  // Take a MozInputMethodKeyboardEventDict dict, creates a keyboardEventDict
  // object that can be sent to forms.js
  _getkeyboardEventDict: function(dict) {
    if (typeof dict !== 'object' || !dict.key) {
      return;
    }

    var keyboardEventDict = {
      key: dict.key,
      code: dict.code,
      repeat: dict.repeat,
      flags: 0
    };

    if (dict.printable) {
      keyboardEventDict.flags |=
        Ci.nsITextInputProcessor.KEY_FORCE_PRINTABLE_KEY;
    }

    if (/^[a-zA-Z0-9]$/.test(dict.key)) {
      // keyCode must follow the key value in this range;
      // disregard the keyCode from content.
      keyboardEventDict.keyCode = dict.key.toUpperCase().charCodeAt(0);
    } else if (typeof dict.keyCode === 'number') {
      // Allow keyCode to be specified for other key values.
      keyboardEventDict.keyCode = dict.keyCode;

      // Allow keyCode to be explicitly set to zero.
      if (dict.keyCode === 0) {
        keyboardEventDict.flags |=
          Ci.nsITextInputProcessor.KEY_KEEP_KEYCODE_ZERO;
      }
    }

    return keyboardEventDict;
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([MozInputMethod]);
