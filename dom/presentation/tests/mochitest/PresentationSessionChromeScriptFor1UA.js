/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, manager: Cm, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function info(str) {
  dump("INFO " + str + "\n");
}

const sessionId = "test-session-id";
const address = Cc["@mozilla.org/supports-cstring;1"]
                  .createInstance(Ci.nsISupportsCString);
address.data = "127.0.0.1";
const addresses = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
addresses.appendElement(address, false);
var requestPromise = null;

// mockedChannelDescription
function mockedChannelDescription(role) {
  this.QueryInterface = XPCOMUtils.generateQI([Ci.nsIPresentationChannelDescription]);
  this.role = role;
	this.type = 1;
	this.tcpAddress = addresses;
	this.tcpPort = (role === "sender" ? 1234 : 4321); // either sender or receiver
}
const mockedChannelDescriptionOfSender = new mockedChannelDescription("sender");
const mockedChannelDescriptionOfReceiver = new mockedChannelDescription("receiver");

// mockedServerSocket (sender only)
const mockedServerSocket = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIServerSocket,
                                         Ci.nsIFactory]),
  createInstance: function(aOuter, aIID) {
    if (aOuter) {
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    }
    return this.QueryInterface(aIID);
  },
  get port() {
    return this._port;
  },
  set listener(listener) {
    this._listener = listener;
  },
  init: function(port, loopbackOnly, backLog) {
    this._port = (port == -1 ? 5678 : port);
  },
  asyncListen: function(listener) {
    this._listener = listener;
  },
  close: function() {
    this._listener.onStopListening(this, Cr.NS_BINDING_ABORTED);
  },
  onSocketAccepted: function(serverSocket, socketTransport) {
    this._listener.onSocketAccepted(serverSocket, socketTransport);
  }
};

// mockedSocketTransport
const mockedSocketTransport = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISocketTransport]),
};

// mockedSessionTransport
var mockedSessionTransportOfSender   = undefined;
var mockedSessionTransportOfReceiver = undefined;

function mockedSessionTransport() {}

mockedSessionTransport.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationSessionTransport]),
  set callback(callback) {
    this._callback = callback;
  },
  get callback() {
    return this._callback;
  },
  get selfAddress() {
    return this._selfAddress;
  },
  initWithSocketTransport: function(transport, callback) {
    mockedSessionTransportOfSender = this;
    this.role = "sender";
    this._callback = callback; // callback is ControllingInfo
    this.simulateTransportReady();
  },
  initWithChannelDescription: function(description, callback) {
    mockedSessionTransportOfReceiver = this;
    this.role = "receiver";
    this._callback = callback; // callback is PresentingInfo
    var addresses = description
                      .QueryInterface(Ci.nsIPresentationChannelDescription)
                      .tcpAddress;
    this._selfAddress = {
      QueryInterface: XPCOMUtils.generateQI([Ci.nsINetAddr]),
      address: (addresses.length > 0) ?
                addresses.queryElementAt(0, Ci.nsISupportsCString).data : "",
      port: description
              .QueryInterface(Ci.nsIPresentationChannelDescription)
              .tcpPort,
    };
  },
  enableDataNotification: function() {
    if (this.role === "sender") {
      sendAsyncMessage("data-transport-notification-of-sender-enabled");
    } else {
      sendAsyncMessage("data-transport-notification-of-receiver-enabled");
    }
  },
  send: function(data) {
    // var binaryStream = Cc["@mozilla.org/binaryinputstream;1"].
    //                    createInstance(Ci.nsIBinaryInputStream);
    // binaryStream.setInputStream(data);
    // var message = binaryStream.readBytes(binaryStream.available());
    var message = data.QueryInterface(Ci.nsISupportsCString).data;
    info("Send message: \"" + message + "\" from " + this.role);
    if (this.role === "sender") {
      mockedSessionTransportOfReceiver._callback.notifyData(message);
    } else {
      mockedSessionTransportOfSender._callback.notifyData(message);
    }
  },
  close: function(reason) {
    sendAsyncMessage("data-transport-closed", reason);
    this._callback.QueryInterface(Ci.nsIPresentationSessionTransportCallback).notifyTransportClosed(reason);
  },
  simulateTransportReady: function() {
    this._callback.QueryInterface(Ci.nsIPresentationSessionTransportCallback).notifyTransportReady();
  },
};

// factory of mockedSessionTransport
const mockedSessionTransportFactory = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFactory]),
  createInstance: function(aOuter, aIID) {
    if (aOuter) {
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    }
    var result = new mockedSessionTransport();
    return result.QueryInterface(aIID);
  }
};

// control channel of sender
const mockedControlChannelOfSender = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationControlChannel]),
  set listener(listener) {
    if (listener) {
      sendAsyncMessage("controlling-info-created");
    }
    this._listener = listener;
  },
  get listener() {
    return this._listener;
  },
  notifyOpened: function() {
    // send offer after notifyOpened immediately
    this._listener
        .QueryInterface(Ci.nsIPresentationControlChannelListener)
        .notifyOpened();
  },
  sendOffer: function(offer) {
    sendAsyncMessage("offer-sent");
  },
  onAnswer: function(answer) {
    sendAsyncMessage("answer-received", answer.role);
    this._listener
        .QueryInterface(Ci.nsIPresentationControlChannelListener)
        .onAnswer(answer);
  },
  close: function(reason) {
    sendAsyncMessage("control-channel-of-sender-closed", reason);
    this._listener
        .QueryInterface(Ci.nsIPresentationControlChannelListener)
        .notifyClosed(reason);
  }
};

// control channel of receiver
const mockedControlChannelOfReceiver = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationControlChannel]),
  set listener(listener) {
    if (listener) {
      sendAsyncMessage("presenting-info-created");
    }
    this._listener = listener;
  },
  get listener() {
    return this._listener;
  },
  notifyOpened: function() {
    // do nothing
    this._listener
        .QueryInterface(Ci.nsIPresentationControlChannelListener)
        .notifyOpened();
  },
  onOffer: function(offer) {
    sendAsyncMessage("offer-received", offer.role);
    this._listener
        .QueryInterface(Ci.nsIPresentationControlChannelListener)
        .onOffer(offer);
  },
  sendAnswer: function(answer) {
    sendAsyncMessage("answer-sent");
    this._listener
        .QueryInterface(Ci.nsIPresentationSessionTransportCallback)
        .notifyTransportReady();
  },
  close: function(reason) {
    sendAsyncMessage("control-channel-receiver-closed", reason);
    this._listener
        .QueryInterface(Ci.nsIPresentationControlChannelListener)
        .notifyClosed(reason);
  }
};

const mockedDevice = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationDevice]),
  id:   "id",
  name: "name",
  type: "type",
  establishControlChannel: function(url, presentationId) {
    sendAsyncMessage("control-channel-established");
    return mockedControlChannelOfSender;
  },
};

const mockedDevicePrompt = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationDevicePrompt,
                                         Ci.nsIFactory]),
  createInstance: function(aOuter, aIID) {
    if (aOuter) {
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    }
    return this.QueryInterface(aIID);
  },
  set request(request) {
    this._request = request;
  },
  get request() {
    return this._request;
  },
  promptDeviceSelection: function(request) {
    this._request = request;
    sendAsyncMessage("device-prompt");
  },
  simulateSelect: function() {
    this._request.select(mockedDevice);
  },
  simulateCancel: function() {
    this._request.cancel();
  }
};

const mockedRequestUIGlue = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationRequestUIGlue,
                                         Ci.nsIFactory]),
  createInstance: function(aOuter, aIID) {
    if (aOuter) {
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    }
    return this.QueryInterface(aIID);
  },
  sendRequest: function(aUrl, aSessionId) {
    return requestPromise;
  },
};

function registerMockedFactory(contractId, mockedClassId, mockedFactory) {
  var originalClassId, originalFactory;

  var registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
  if (!registrar.isCIDRegistered(mockedClassId)) {
    try {
      originalClassId = registrar.contractIDToCID(contractId);
      originalFactory = Cm.getClassObject(Cc[contractId], Ci.nsIFactory);
    } catch (ex) {
      originalClassId = "";
      originalFactory = null;
    }
    if (originalFactory) {
      registrar.unregisterFactory(originalClassId, originalFactory);
    }
    registrar.registerFactory(mockedClassId, "", contractId, mockedFactory);
  }

  return { contractId: contractId,
           mockedClassId: mockedClassId,
           mockedFactory: mockedFactory,
           originalClassId: originalClassId,
           originalFactory: originalFactory };
}

function registerOriginalFactory(contractId, mockedClassId, mockedFactory, originalClassId, originalFactory) {
  if (originalFactory) {
    registrar.unregisterFactory(mockedClassId, mockedFactory);
    registrar.registerFactory(originalClassId, "", contractId, originalFactory);
  }
}

function tearDown() {
  var deviceManager = Cc["@mozilla.org/presentation-device/manager;1"]
                      .getService(Ci.nsIPresentationDeviceManager);
  deviceManager.QueryInterface(Ci.nsIPresentationDeviceListener).removeDevice(mockedDevice);

  mockedServerSocket.listener = null;
  mockedSessionTransportOfSender.callback = null;
  mockedSessionTransportOfReceiver.callback = null;
  mockedControlChannelOfSender.listener = null;
  mockedControlChannelOfReceiver.listener = null;

  // Register original factories.
  for (var data in originalFactoryData) {
    registerOriginalFactory(data.contractId, data.mockedClassId,
                            data.mockedFactory, data.originalClassId,
                            data.originalFactory);
  }
  sendAsyncMessage("teardown-complete");
}

// Register mocked factories.
const uuidGenerator = Cc["@mozilla.org/uuid-generator;1"]
                      .getService(Ci.nsIUUIDGenerator);
const originalFactoryData = [];
originalFactoryData.push(registerMockedFactory("@mozilla.org/presentation-device/prompt;1",
                                               uuidGenerator.generateUUID(),
                                               mockedDevicePrompt));
originalFactoryData.push(registerMockedFactory("@mozilla.org/network/server-socket;1",
                                               uuidGenerator.generateUUID(),
                                               mockedServerSocket));
originalFactoryData.push(registerMockedFactory("@mozilla.org/presentation/presentationsessiontransport;1",
                                               uuidGenerator.generateUUID(),
                                               mockedSessionTransportFactory));
originalFactoryData.push(registerMockedFactory("@mozilla.org/presentation/requestuiglue;1",
                                               uuidGenerator.generateUUID(),
                                               mockedRequestUIGlue));

// Add mocked device into device list.
addMessageListener("trigger-device-add", function() {
  var deviceManager = Cc["@mozilla.org/presentation-device/manager;1"]
                      .getService(Ci.nsIPresentationDeviceManager);
  deviceManager.QueryInterface(Ci.nsIPresentationDeviceListener).addDevice(mockedDevice);
});

// Select the mocked device for presentation.
addMessageListener("trigger-device-prompt-select", function() {
  mockedDevicePrompt.simulateSelect();
});

// Trigger nsIPresentationDeviceManager::OnSessionRequest
// to create session at receiver.
addMessageListener("trigger-on-session-request", function(url) {
  var deviceManager = Cc["@mozilla.org/presentation-device/manager;1"]
                        .getService(Ci.nsIPresentationDeviceManager);
  deviceManager.QueryInterface(Ci.nsIPresentationDeviceListener)
	             .onSessionRequest(mockedDevice,
                                 url,
                                 sessionId,
                                 mockedControlChannelOfReceiver);
});

// Trigger control channel opened
addMessageListener("trigger-control-channel-open", function(reason) {
  mockedControlChannelOfSender.notifyOpened();
  mockedControlChannelOfReceiver.notifyOpened();
});

// Trigger server socket of controlling info accepted
addMessageListener("trigger-socket-accepted", function() {
});

addMessageListener("trigger-on-offer", function() {
  mockedControlChannelOfReceiver.onOffer(mockedChannelDescriptionOfSender);
  mockedServerSocket.onSocketAccepted(mockedServerSocket, mockedSocketTransport);
});

addMessageListener("trigger-on-answer", function() {
  mockedControlChannelOfSender.onAnswer(mockedChannelDescriptionOfReceiver);
});

addMessageListener("forward-command", function(command_data) {
  let command = JSON.parse(command_data);
  sendAsyncMessage(command.name, command.data);
});

addMessageListener("teardown", function() {
  tearDown();
});

var obs = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
obs.addObserver(function observer(aSubject, aTopic, aData) {
  obs.removeObserver(observer, aTopic);
  requestPromise = aSubject;
}, "setup-request-promise", false);

