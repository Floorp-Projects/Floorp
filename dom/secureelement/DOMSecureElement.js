/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Copyright © 2014, Deutsche Telekom, Inc. */

"use strict";

/* globals dump, Components, XPCOMUtils, DOMRequestIpcHelper, cpmm, SE  */

const DEBUG = false;
function debug(s) {
  if (DEBUG) {
    dump("-*- SecureElement DOM: " + s + "\n");
  }
}

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsISyncMessageSender");

XPCOMUtils.defineLazyGetter(this, "SE", function() {
  let obj = {};
  Cu.import("resource://gre/modules/se_consts.js", obj);
  return obj;
});

// Extend / Inherit from Error object
function SEError(name, message) {
  this.name = name || SE.ERROR_GENERIC;
  this.message = message || "";
}

SEError.prototype = {
  __proto__: Error.prototype,
};

function PromiseHelpersSubclass(win) {
  this._window = win;
}

PromiseHelpersSubclass.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  _window: null,

  _context: [],

  createSEPromise: function createSEPromise(callback, /* optional */ ctx) {
    let ctxCallback = (resolverId) => {
      if (ctx) {
        this._context[resolverId] = ctx;
      }

      callback(resolverId);
    };

    return this.createPromiseWithId((aResolverId) => {
      ctxCallback(aResolverId);
    });
  },

  takePromise: function takePromise(resolverId) {
    let resolver = this.takePromiseResolver(resolverId);
    if (!resolver) {
      return;
    }

    // Get the context associated with this resolverId
    let context = this._context[resolverId];
    delete this._context[resolverId];

    return {resolver: resolver, context: context};
  },

  rejectWithSEError: function rejectWithSEError(name, message) {
    let error = new SEError(name, message);
    debug("rejectWithSEError - " + error.toString());

    return this._window.Promise.reject(Cu.cloneInto(error, this._window));
  }
};

// Helper wrapper class to do promises related chores
var PromiseHelpers;

/**
 * Instance of 'SEReaderImpl' class is the connector to a secure element.
 * A reader may or may not have a secure element present, since some
 * secure elements are removable in nature (eg:- 'uicc'). These
 * Readers can be physical devices or virtual devices.
 */
function SEReaderImpl() {}

SEReaderImpl.prototype = {
  _window: null,

  _sessions: [],

  type: null,
  _isSEPresent: false,

  classID: Components.ID("{1c7bdba3-cd35-4f8b-a546-55b3232457d5}"),
  contractID: "@mozilla.org/secureelement/reader;1",
  QueryInterface: XPCOMUtils.generateQI([]),

  // Chrome-only function
  onSessionClose: function onSessionClose(sessionCtx) {
    let index = this._sessions.indexOf(sessionCtx);
    if (index != -1) {
      this._sessions.splice(index, 1);
    }
  },

  initialize: function initialize(win, type, isPresent) {
    this._window = win;
    this.type = type;
    this._isSEPresent = isPresent;
  },

  _checkPresence: function _checkPresence() {
    if (!this._isSEPresent) {
      throw new Error(SE.ERROR_NOTPRESENT);
    }
  },

  openSession: function openSession() {
    this._checkPresence();

    return PromiseHelpers.createSEPromise((resolverId) => {
      let sessionImpl = new SESessionImpl();
      sessionImpl.initialize(this._window, this);
      this._window.SESession._create(this._window, sessionImpl);
      this._sessions.push(sessionImpl);
      PromiseHelpers.takePromiseResolver(resolverId)
                    .resolve(sessionImpl.__DOM_IMPL__);
    });
  },

  closeAll: function closeAll() {
    this._checkPresence();

    return PromiseHelpers.createSEPromise((resolverId) => {
      let promises = [];
      for (let session of this._sessions) {
        if (!session.isClosed) {
          promises.push(session.closeAll());
        }
      }

      let resolver = PromiseHelpers.takePromiseResolver(resolverId);
      // Wait till all the promises are resolved
      Promise.all(promises).then(() => {
        this._sessions = [];
        resolver.resolve();
      }, (reason) => {
        let error = new SEError(SE.ERROR_BADSTATE,
          "Unable to close all channels associated with this reader");
        resolver.reject(Cu.cloneInto(error, this._window));
      });
    });
  },

  updateSEPresence: function updateSEPresence(isSEPresent) {
    if (!isSEPresent) {
      this.invalidate();
      return;
    }

    this._isSEPresent = isSEPresent;
  },

  invalidate: function invalidate() {
    debug("Invalidating SE reader: " + this.type);
    this._isSEPresent = false;
    this._sessions.forEach(s => s.invalidate());
    this._sessions = [];
  },

  get isSEPresent() {
    return this._isSEPresent;
  }
};

/**
 * Instance of 'SESessionImpl' object represent a connection session
 * to one of the secure elements available on the device.
 * These objects can be used to get a communication channel with an application
 * hosted by the Secure Element.
 */
function SESessionImpl() {}

SESessionImpl.prototype = {
  _window: null,

  _channels: [],

  _isClosed: false,

  _reader: null,

  classID: Components.ID("{2b1809f8-17bd-4947-abd7-bdef1498561c}"),
  contractID: "@mozilla.org/secureelement/session;1",
  QueryInterface: XPCOMUtils.generateQI([]),

  // Chrome-only function
  onChannelOpen: function onChannelOpen(channelCtx) {
    this._channels.push(channelCtx);
  },

  // Chrome-only function
  onChannelClose: function onChannelClose(channelCtx) {
    let index = this._channels.indexOf(channelCtx);
    if (index != -1) {
      this._channels.splice(index, 1);
    }
  },

  initialize: function initialize(win, readerCtx) {
    this._window = win;
    this._reader = readerCtx;
  },

  openLogicalChannel: function openLogicalChannel(aid) {
    if (this._isClosed) {
      return PromiseHelpers.rejectWithSEError(SE.ERROR_BADSTATE,
             "Session Already Closed!");
    }

    let aidLen = aid ? aid.length : 0;
    if (aidLen < SE.MIN_AID_LEN || aidLen > SE.MAX_AID_LEN) {
      return PromiseHelpers.rejectWithSEError(SE.ERROR_ILLEGALPARAMETER,
             "Invalid AID length - " + aidLen);
    }

    return PromiseHelpers.createSEPromise((resolverId) => {
      /**
       * @params for 'SE:OpenChannel'
       *
       * resolverId  : ID that identifies this IPC request.
       * aid         : AID that identifies the applet on SecureElement
       * type        : Reader type ('uicc' / 'eSE')
       * appId       : Current appId obtained from 'Principal' obj
       */
      cpmm.sendAsyncMessage("SE:OpenChannel", {
        resolverId: resolverId,
        aid: aid,
        type: this.reader.type,
        appId: this._window.document.nodePrincipal.appId
      });
    }, this);
  },

  closeAll: function closeAll() {
    if (this._isClosed) {
      return PromiseHelpers.rejectWithSEError(SE.ERROR_BADSTATE,
             "Session Already Closed!");
    }

    return PromiseHelpers.createSEPromise((resolverId) => {
      let promises = [];
      for (let channel of this._channels) {
        if (!channel.isClosed) {
          promises.push(channel.close());
        }
      }

      let resolver = PromiseHelpers.takePromiseResolver(resolverId);
      Promise.all(promises).then(() => {
        this._isClosed = true;
        this._channels = [];
        // Notify parent of this session instance's closure, so that its
        // instance entry can be removed from the parent as well.
        this._reader.onSessionClose(this.__DOM_IMPL__);
        resolver.resolve();
      }, (reason) => {
        resolver.reject(new Error(SE.ERROR_BADSTATE +
          "Unable to close all channels associated with this session"));
      });
    });
  },

  invalidate: function invlidate() {
    this._isClosed = true;
    this._channels.forEach(ch => ch.invalidate());
    this._channels = [];
  },

  get reader() {
    return this._reader.__DOM_IMPL__;
  },

  get isClosed() {
    return this._isClosed;
  },
};

/**
 * Instance of 'SEChannelImpl' object represent an ISO/IEC 7816-4 specification
 * channel opened to a secure element. It can be either a logical channel
 * or basic channel.
 */
function SEChannelImpl() {}

SEChannelImpl.prototype = {
  _window: null,

  _channelToken: null,

  _isClosed: false,

  _session: null,

  openResponse: [],

  type: null,

  classID: Components.ID("{181ebcf4-5164-4e28-99f2-877ec6fa83b9}"),
  contractID: "@mozilla.org/secureelement/channel;1",
  QueryInterface: XPCOMUtils.generateQI([]),

  // Chrome-only function
  onClose: function onClose() {
    this._isClosed = true;
    // Notify the parent
    this._session.onChannelClose(this.__DOM_IMPL__);
  },

  initialize: function initialize(win, channelToken, isBasicChannel,
                                  openResponse, sessionCtx) {
    this._window = win;
    // Update the 'channel token' that identifies and represents this
    // instance of the object
    this._channelToken = channelToken;
    // Update 'session' obj
    this._session = sessionCtx;
    this.openResponse = Cu.cloneInto(new Uint8Array(openResponse), win);
    this.type = isBasicChannel ? "basic" : "logical";
  },

  transmit: function transmit(command) {
    // TODO remove this once it will be possible to have a non-optional dict
    // in the WebIDL
    if (!command) {
      return PromiseHelpers.rejectWithSEError(SE.ERROR_ILLEGALPARAMETER,
             "SECommand dict must be defined");
    }

    if (this._isClosed) {
      return PromiseHelpers.rejectWithSEError(SE.ERROR_BADSTATE,
             "Channel Already Closed!");
    }

    let dataLen = command.data ? command.data.length : 0;
    if (dataLen > SE.MAX_APDU_LEN) {
      return PromiseHelpers.rejectWithSEError(SE.ERROR_ILLEGALPARAMETER,
             " Command data length exceeds max limit - 255. " +
             " Extended APDU is not supported!");
    }

    if ((command.cla & 0x80 === 0) && ((command.cla & 0x60) !== 0x20)) {
      if (command.ins === SE.INS_MANAGE_CHANNEL) {
        return PromiseHelpers.rejectWithSEError(SE.ERROR_SECURITY,
               "MANAGE CHANNEL command not permitted");
      }
      if ((command.ins === SE.INS_SELECT) && (command.p1 == 0x04)) {
        // SELECT by DF Name (p1=04) is not allowed
        return PromiseHelpers.rejectWithSEError(SE.ERROR_SECURITY,
               "SELECT command not permitted");
      }
      debug("Attempting to transmit an ISO command");
    } else {
      debug("Attempting to transmit GlobalPlatform command");
    }

    return PromiseHelpers.createSEPromise((resolverId) => {
      /**
       * @params for 'SE:TransmitAPDU'
       *
       * resolverId  : Id that identifies this IPC request.
       * apdu        : Object containing APDU data
       * channelToken: Token that identifies the current channel over which
                       'c-apdu' is being sent.
       * appId       : Current appId obtained from 'Principal' obj
       */
      cpmm.sendAsyncMessage("SE:TransmitAPDU", {
        resolverId: resolverId,
        apdu: command,
        channelToken: this._channelToken,
        appId: this._window.document.nodePrincipal.appId
      });
    }, this);
  },

  close: function close() {
    if (this._isClosed) {
      return PromiseHelpers.rejectWithSEError(SE.ERROR_BADSTATE,
             "Channel Already Closed!");
    }

    return PromiseHelpers.createSEPromise((resolverId) => {
      /**
       * @params for 'SE:CloseChannel'
       *
       * resolverId  : Id that identifies this IPC request.
       * channelToken: Token that identifies the current channel over which
                       'c-apdu' is being sent.
       * appId       : Current appId obtained from 'Principal' obj
       */
      cpmm.sendAsyncMessage("SE:CloseChannel", {
        resolverId: resolverId,
        channelToken: this._channelToken,
        appId: this._window.document.nodePrincipal.appId
      });
    }, this);
  },

  invalidate: function invalidate() {
    this._isClosed = true;
  },

  get session() {
    return this._session.__DOM_IMPL__;
  },

  get isClosed() {
    return this._isClosed;
  },
};

function SEResponseImpl() {}

SEResponseImpl.prototype = {
  sw1: 0x00,

  sw2: 0x00,

  data: null,

  _channel: null,

  classID: Components.ID("{58bc6c7b-686c-47cc-8867-578a6ed23f4e}"),
  contractID: "@mozilla.org/secureelement/response;1",
  QueryInterface: XPCOMUtils.generateQI([]),

  initialize: function initialize(sw1, sw2, response, channelCtx) {
    // Update the status bytes
    this.sw1 = sw1;
    this.sw2 = sw2;
    this.data = response ? response.slice(0) : null;
    // Update the channel obj
    this._channel = channelCtx;
  },

  get channel() {
    return this._channel.__DOM_IMPL__;
  }
};

/**
 * SEManagerImpl
 */
function SEManagerImpl() {}

SEManagerImpl.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  _window: null,

  classID: Components.ID("{4a8b6ec0-4674-11e4-916c-0800200c9a66}"),
  contractID: "@mozilla.org/secureelement/manager;1",
  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIDOMGlobalPropertyInitializer,
    Ci.nsISupportsWeakReference,
    Ci.nsIObserver
  ]),

  _readers: [],

  init: function init(win) {
    this._window = win;
    PromiseHelpers = new PromiseHelpersSubclass(this._window);

    // Add the messages to be listened to.
    const messages = ["SE:GetSEReadersResolved",
                      "SE:OpenChannelResolved",
                      "SE:CloseChannelResolved",
                      "SE:TransmitAPDUResolved",
                      "SE:GetSEReadersRejected",
                      "SE:OpenChannelRejected",
                      "SE:CloseChannelRejected",
                      "SE:TransmitAPDURejected",
                      "SE:ReaderPresenceChanged"];

    this.initDOMRequestHelper(win, messages);
  },

  // This function will be called from DOMRequestIPCHelper.
  uninit: function uninit() {
    // All requests that are still pending need to be invalidated
    // because the context is no longer valid.
    this.forEachPromiseResolver((k) => {
      this.takePromiseResolver(k).reject("Window Context got destroyed!");
    });
    PromiseHelpers = null;
    this._window = null;
  },

  getSEReaders: function getSEReaders() {
    // invalidate previous readers on new request
    if (this._readers.length) {
      this._readers.forEach(r => r.invalidate());
      this._readers = [];
    }

    return PromiseHelpers.createSEPromise((resolverId) => {
      cpmm.sendAsyncMessage("SE:GetSEReaders", {
        resolverId: resolverId,
        appId: this._window.document.nodePrincipal.appId
      });
    });
  },

  receiveMessage: function receiveMessage(message) {
    DEBUG && debug("Message received: " + JSON.stringify(message));

    let result = message.data.result;
    let resolver = null;
    let context = null;

    let promiseResolver = PromiseHelpers.takePromise(result.resolverId);
    if (promiseResolver) {
      resolver = promiseResolver.resolver;
      // This 'context' is the instance that originated this IPC message.
      context = promiseResolver.context;
    }

    switch (message.name) {
      case "SE:GetSEReadersResolved":
        let readers = new this._window.Array();
        result.readers.forEach(reader => {
          let readerImpl = new SEReaderImpl();
          readerImpl.initialize(this._window, reader.type, reader.isPresent);
          this._window.SEReader._create(this._window, readerImpl);

          this._readers.push(readerImpl);
          readers.push(readerImpl.__DOM_IMPL__);
        });
        resolver.resolve(readers);
        break;
      case "SE:OpenChannelResolved":
        let channelImpl = new SEChannelImpl();
        channelImpl.initialize(this._window,
                               result.channelToken,
                               result.isBasicChannel,
                               result.openResponse,
                               context);
        this._window.SEChannel._create(this._window, channelImpl);
        if (context) {
          // Notify context's handler with SEChannel instance
          context.onChannelOpen(channelImpl);
        }
        resolver.resolve(channelImpl.__DOM_IMPL__);
        break;
      case "SE:TransmitAPDUResolved":
        let responseImpl = new SEResponseImpl();
        responseImpl.initialize(result.sw1,
                                result.sw2,
                                result.response,
                                context);
        this._window.SEResponse._create(this._window, responseImpl);
        resolver.resolve(responseImpl.__DOM_IMPL__);
        break;
      case "SE:CloseChannelResolved":
        if (context) {
          // Notify context's onClose handler
          context.onClose();
        }
        resolver.resolve();
        break;
      case "SE:GetSEReadersRejected":
      case "SE:OpenChannelRejected":
      case "SE:CloseChannelRejected":
      case "SE:TransmitAPDURejected":
        let error = new SEError(result.error, result.reason);
        resolver.reject(Cu.cloneInto(error, this._window));
        break;
      case "SE:ReaderPresenceChanged":
        debug("Reader " + result.type + " present: " + result.isPresent);
        let reader = this._readers.find(r => r.type === result.type);
        if (reader) {
          reader.updateSEPresence(result.isPresent);
        }
        break;
      default:
        debug("Could not find a handler for " + message.name);
        resolver.reject(Cu.cloneInto(new SEError(), this._window));
        break;
    }
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([
  SEResponseImpl, SEChannelImpl, SESessionImpl, SEReaderImpl, SEManagerImpl
]);
