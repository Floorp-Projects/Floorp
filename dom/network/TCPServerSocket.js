/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;
const CC = Components.Constructor;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const ServerSocket = CC(
        '@mozilla.org/network/server-socket;1', 'nsIServerSocket', 'init'),
      TCPSocketInternal = Cc[
        '@mozilla.org/tcp-socket;1'].createInstance(Ci.nsITCPSocketInternal);

/*
 * Debug logging function
 */

let debug = false;
function LOG(msg) {
  if (debug) {
    dump("TCPServerSocket: " + msg + "\n");
  }
}

/*
 * nsIDOMTCPServerSocket object
 */

function TCPServerSocket() {
  this.makeGetterSetterEH("onconnect");
  this.makeGetterSetterEH("onerror");

  this._localPort = 0;
  this._binaryType = null;

  this._inChild = false;
  this._neckoTCPServerSocket = null;
  this._serverBridge = null;
}

TCPServerSocket.prototype = {
  get localPort() {
    return this._localPort;
  },

  getEH: function(type) {
    return this.__DOM_IMPL__.getEventHandler(type);
  },
  setEH: function(type, handler) {
    this.__DOM_IMPL__.setEventHandler(type, handler);
  },
  makeGetterSetterEH: function(name) {
    Object.defineProperty(this, name,
                          {
                            get:function()  { return this.getEH(name); },
                            set:function(h) { return this.setEH(name, h); }
                          });
  },

  _callListenerAcceptCommon: function tss_callListenerAcceptCommon(socket) {
    try {
      this.__DOM_IMPL__.dispatchEvent(new this.useGlobal.TCPServerSocketEvent("connect", {socket: socket}));
    } catch(e) {
      LOG('exception in _callListenerAcceptCommon: ' + e);
      socket.close();
    }
  },
  init: function tss_init(aWindowObj) {
    this.useGlobal = aWindowObj;

    if (aWindowObj) {
      Services.obs.addObserver(this, "inner-window-destroyed", true);

      let util = aWindowObj.QueryInterface(
        Ci.nsIInterfaceRequestor
      ).getInterface(Ci.nsIDOMWindowUtils);
      this.innerWindowID = util.currentInnerWindowID;

      LOG("window init: " + this.innerWindowID);
    }
  },

  __init: function(port, options, backlog) {
    this.listen(port, options, backlog);
  },

  initWithGlobal: function(global) {
    this.useGlobal = global;
  },

  observe: function(aSubject, aTopic, aData) {
    let wId;
    try {
      wId = aSubject.QueryInterface(Ci.nsISupportsPRUint64).data;
    } catch(x) {
      wId = 0;
    }

    if (wId == this.innerWindowID) {
      LOG("inner-window-destroyed: " + this.innerWindowID);

      // This window is now dead, so we want to clear the callbacks
      // so that we don't get a "can't access dead object" when the
      // underlying stream goes to tell us that we are closed
      this.onconnect = null;
      this.onerror = null;

      this.useGlobal = null;

      // Clean up our socket
      this.close();
    }
  },

  /* nsITCPServerSocketInternal method */
  listen: function tss_listen(localPort, options, backlog) {
    this._inChild = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime)
                       .processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
    this._binaryType = options.binaryType;

    if (this._inChild) {
      if (this._serverBridge == null) {
        this._serverBridge = Cc["@mozilla.org/tcp-server-socket-child;1"]
                                .createInstance(Ci.nsITCPServerSocketChild);
        this._serverBridge.listen(this, localPort, backlog, options.binaryType);
      }
      else {
        throw new Error("Child TCPServerSocket has already listening. \n");
      }
    }
    else {
      if (this._neckoTCPServerSocket == null) {
        this._neckoTCPServerSocket = new ServerSocket(localPort, false, backlog);
        this._localPort = this._neckoTCPServerSocket.port;
        this._neckoTCPServerSocket.asyncListen(this);
      }
      else {
        throw new Error("Parent TCPServerSocket has already listening. \n");
      }
    }
  },

  callListenerAccept: function tss_callListenerSocket(socketChild) {
    // this method is called at child process when the socket is accepted at parent process.
    let socket = TCPSocketInternal.createAcceptedChild(socketChild, this._binaryType, this.useGlobal);
    this._callListenerAcceptCommon(socket);
  },

  callListenerError: function tss_callListenerError(message, filename, lineNumber, columnNumber) {
    let init = {
      message: message,
      filename: filename,
      lineno: lineNumber,
      colno: columnNumber,
    };
    this.__DOM_IMPL__.dispatchEvent(new this.useGlobal.ErrorEvent("error", init));
  },
  /* end nsITCPServerSocketInternal method */

  close: function tss_close() {
    if (this._inChild) {
      this._serverBridge.close();
      return;
    }

    /*　Close ServerSocket　*/
    if (this._neckoTCPServerSocket) {
      this._neckoTCPServerSocket.close();
    }
  },

  // nsIServerSocketListener (Triggered by _neckoTCPServerSocket.asyncListen)
  onSocketAccepted: function tss_onSocketAccepted(server, trans) {
    // precondition: this._inChild == false
    try {
      let that = TCPSocketInternal.createAcceptedParent(trans, this._binaryType,
                                                        this.useGlobal);
      this._callListenerAcceptCommon(that);
    }
    catch(e) {
      LOG('exception in onSocketAccepted: ' + e);
      trans.close(Cr.NS_BINDING_ABORTED);
    }
  },

  // nsIServerSocketListener (Triggered by _neckoTCPServerSocket.asyncListen)
  onStopListening: function tss_onStopListening(server, status) {
    if (status != Cr.NS_BINDING_ABORTED) {
      throw new Error("Server socket was closed by unexpected reason.");
    }
    this._neckoTCPServerSocket = null;
  },

  getInternalSocket: function() {
    return this.QueryInterface(Ci.nsITCPServerSocketInternal);
  },

  classID: Components.ID("{73065eae-27dc-11e2-895a-000c29987aa2}"),

  contractID: "@mozilla.org/tcp-server-socket;1",
  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsITCPServerSocketInternal,
    Ci.nsIDOMGlobalPropertyInitializer,
    Ci.nsISupportsWeakReference,
    Ci.nsIObserver
  ])
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([TCPServerSocket]);
