/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const CC = Components.Constructor;
const ServerSocket = CC("@mozilla.org/network/server-socket;1",
                        "nsIServerSocket",
                        "init");

ChromeUtils.import('resource://gre/modules/XPCOMUtils.jsm');
ChromeUtils.import('resource://gre/modules/Services.jsm');

var testServer = null;
var clientTransport = null;
var serverTransport = null;

var clientBuilder = null;
var serverBuilder = null;

const clientMessage = "Client Message";
const serverMessage = "Server Message";

const address = Cc["@mozilla.org/supports-cstring;1"]
                  .createInstance(Ci.nsISupportsCString);
address.data = "127.0.0.1";
const addresses = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
addresses.appendElement(address);

const serverChannelDescription = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIPresentationChannelDescription]),
  type: 1,
  tcpAddress: addresses,
};

var isClientReady = false;
var isServerReady = false;
var isClientClosed = false;
var isServerClosed = false;

const clientCallback = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIPresentationSessionTransportCallback]),
  notifyTransportReady: function () {
    Assert.ok(true, "Client transport ready.");

    isClientReady = true;
    if (isClientReady && isServerReady) {
      run_next_test();
    }
  },
  notifyTransportClosed: function (aReason) {
    Assert.ok(true, "Client transport is closed.");

    isClientClosed = true;
    if (isClientClosed && isServerClosed) {
      run_next_test();
    }
  },
  notifyData: function(aData) {
    Assert.equal(aData, serverMessage, "Client transport receives data.");
    run_next_test();
  },
};

const serverCallback = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIPresentationSessionTransportCallback]),
  notifyTransportReady: function () {
    Assert.ok(true, "Server transport ready.");

    isServerReady = true;
    if (isClientReady && isServerReady) {
      run_next_test();
    }
  },
  notifyTransportClosed: function (aReason) {
    Assert.ok(true, "Server transport is closed.");

    isServerClosed = true;
    if (isClientClosed && isServerClosed) {
      run_next_test();
    }
  },
  notifyData: function(aData) {
    Assert.equal(aData, clientMessage, "Server transport receives data.");
    run_next_test();
  },
};

const clientListener = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIPresentationSessionTransportBuilderListener]),
  onSessionTransport(aTransport) {
    Assert.ok(true, "Client Transport is built.");
    clientTransport = aTransport;
    clientTransport.callback = clientCallback;

    if (serverTransport) {
      run_next_test();
    }
  }
}

const serverListener = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIPresentationSessionTransportBuilderListener]),
  onSessionTransport(aTransport) {
    Assert.ok(true, "Server Transport is built.");
    serverTransport = aTransport;
    serverTransport.callback = serverCallback;
    serverTransport.enableDataNotification();

    if (clientTransport) {
      run_next_test();
    }
  }
}

function TestServer() {
  this.serverSocket = ServerSocket(-1, true, -1);
  this.serverSocket.asyncListen(this)
}

TestServer.prototype = {
  onSocketAccepted: function(aSocket, aTransport) {
    print("Test server gets a client connection.");
    serverBuilder = Cc["@mozilla.org/presentation/presentationtcpsessiontransport;1"]
                      .createInstance(Ci.nsIPresentationTCPSessionTransportBuilder);
    serverBuilder.buildTCPSenderTransport(aTransport, serverListener);
  },
  onStopListening: function(aSocket) {
    print("Test server stops listening.");
  },
  close: function() {
    if (this.serverSocket) {
      this.serverSocket.close();
      this.serverSocket = null;
    }
  }
};

// Set up the transport connection and ensure |notifyTransportReady| triggered
// at both sides.
function setup() {
  clientBuilder = Cc["@mozilla.org/presentation/presentationtcpsessiontransport;1"]
                    .createInstance(Ci.nsIPresentationTCPSessionTransportBuilder);
  clientBuilder.buildTCPReceiverTransport(serverChannelDescription, clientListener);
}

// Test |selfAddress| attribute of |nsIPresentationSessionTransport|.
function selfAddress() {
  var serverSelfAddress = serverTransport.selfAddress;
  Assert.equal(serverSelfAddress.address, address.data, "The self address of server transport should be set.");
  Assert.equal(serverSelfAddress.port, testServer.serverSocket.port, "The port of server transport should be set.");

  var clientSelfAddress = clientTransport.selfAddress;
  Assert.ok(clientSelfAddress.address, "The self address of client transport should be set.");
  Assert.ok(clientSelfAddress.port, "The port of client transport should be set.");

  run_next_test();
}

// Test the client sends a message and then a corresponding notification gets
// triggered at the server side.
function clientSendMessage() {
  clientTransport.send(clientMessage);
}

// Test the server sends a message an then a corresponding notification gets
// triggered at the client side.
function serverSendMessage() {
  serverTransport.send(serverMessage);
  // The client enables data notification even after the incoming message has
  // been sent, and should still be able to consume it.
  clientTransport.enableDataNotification();
}

function transportClose() {
  clientTransport.close(Cr.NS_OK);
}

function shutdown() {
  testServer.close();
  run_next_test();
}

add_test(setup);
add_test(selfAddress);
add_test(clientSendMessage);
add_test(serverSendMessage);
add_test(transportClose);
add_test(shutdown);

function run_test() {
  testServer = new TestServer();
  // Get the port of the test server.
  serverChannelDescription.tcpPort = testServer.serverSocket.port;

  run_next_test();
}
