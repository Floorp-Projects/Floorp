/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function TCPSocketParentIntermediary() {
}

TCPSocketParentIntermediary.prototype = {
  open: function(aParentSide, aHost, aPort, aUseSSL, aBinaryType) {
    aParentSide.initJS(this);

    let baseSocket = Cc["@mozilla.org/tcp-socket;1"].createInstance(Ci.nsIDOMTCPSocket);
    let socket = this._socket = baseSocket.open(aHost, aPort,
                                                {useSSL: aUseSSL,
                                                binaryType: aBinaryType});
    if (!socket)
      return null;

    // Create handlers for every possible callback that attempt to trigger
    // corresponding callbacks on the child object.
    ["open", "drain", "data", "error", "close"].forEach(
      function(p) {
        socket["on" + p] = function(data) {
          aParentSide.sendCallback(p, data.data, socket.readyState,
                                   socket.bufferedAmount);
        };
      }
    );

    return socket;
  },

  sendString: function(aData) {
    return this._socket.send(aData);
  },

  sendArrayBuffer: function(aData) {
    return this._socket.send(aData, 0, aData.byteLength);
  },

  classID: Components.ID("{afa42841-a6cb-4a91-912f-93099f6a3d18}"),
  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsITCPSocketIntermediary
  ])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([TCPSocketParentIntermediary]);
