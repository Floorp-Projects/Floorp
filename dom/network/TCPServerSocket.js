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

const ServerSocket = CC(
        '@mozilla.org/network/server-socket;1', 'nsIServerSocket', 'init'),
      TCPSocketInternal = Cc[
        '@mozilla.org/tcp-socket;1'].createInstance(Ci.nsITCPSocketInternal);

/*
 * Debug logging function
 */

let debug = true;
function LOG(msg) {
  if (debug) {
    dump("TCPServerSocket: " + msg + "\n");
  }
}

/*
 * nsIDOMTCPServerSocket object
 */

function TCPServerSocket() {
  this._localPort = 0;
  this._binaryType = null;

  this._onconnect = null;
  this._onerror = null;

  this._inChild = false;
  this._neckoTCPServerSocket = null;
  this._serverBridge = null;
  this.useWin = null;
}

// When this API moves to WebIDL and these __exposedProps__ go away, remove
// this call here and remove the API from XPConnect.
Cu.skipCOWCallableChecks();

TCPServerSocket.prototype = {
  __exposedProps__: {
    port: 'r',
    onconnect: 'rw',
    onerror: 'rw'
  },
  get localPort() {
    return this._localPort;
  },
  get onconnect() {
    return this._onconnect;
  },
  set onconnect(f) {
    this._onconnect = f;
  },
  get onerror() {
    return this._onerror;
  },
  set onerror(f) {
    this._onerror = f;
  },

  _callListenerAcceptCommon: function tss_callListenerAcceptCommon(socket) {
    if (this._onconnect) {
      try {
        this["onconnect"].call(null, socket);
      } catch (e) {
        socket.close();
      }      
    }
    else {
      socket.close();
      dump("Received unexpected connection!");
    }
  },
  init: function tss_init(aWindowObj) {
    this.useWin = aWindowObj;
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
    let socket = TCPSocketInternal.createAcceptedChild(socketChild, this._binaryType, this.useWin);
    this._callListenerAcceptCommon(socket);
  },

  callListenerError: function tss_callListenerError(message, filename, lineNumber, columnNumber) {
    if (this._onerror) {
      var type = "error";
      var error = new Error(message, filename, lineNumber, columnNumber);

      this["onerror"].call(null, new TCPSocketEvent(type, this, error));
    }    
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
      let that = TCPSocketInternal.createAcceptedParent(trans, this._binaryType);
      this._callListenerAcceptCommon(that);
    }
    catch(e) {
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

  classID: Components.ID("{73065eae-27dc-11e2-895a-000c29987aa2}"),

  classInfo: XPCOMUtils.generateCI({
    classID: Components.ID("{73065eae-27dc-11e2-895a-000c29987aa2}"),
    classDescription: "Server TCP Socket",
    interfaces: [
      Ci.nsIDOMTCPServerSocket,
      Ci.nsISupportsWeakReference
    ],
    flags: Ci.nsIClassInfo.DOM_OBJECT,
  }),

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIDOMTCPServerSocket,
    Ci.nsITCPServerSocketInternal,
    Ci.nsISupportsWeakReference
  ])
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([TCPServerSocket]);
