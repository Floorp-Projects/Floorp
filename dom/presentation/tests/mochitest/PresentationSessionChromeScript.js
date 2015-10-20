/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
'use strict';

const { classes: Cc, interfaces: Ci, manager: Cm, utils: Cu, results: Cr } = Components;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');

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

const sessionId = 'test-session-id';

const address = Cc["@mozilla.org/supports-cstring;1"]
                  .createInstance(Ci.nsISupportsCString);
address.data = "127.0.0.1";
const addresses = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
addresses.appendElement(address, false);

const mockedChannelDescription = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationChannelDescription]),
  type: 1,
  tcpAddress: addresses,
  tcpPort: 1234,
};

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
    if (port != -1) {
      this._port = port;
    } else {
      this._port = 5678;
    }
  },
  asyncListen: function(listener) {
    this._listener = listener;
  },
  close: function() {
    this._listener.onStopListening(this, Cr.NS_BINDING_ABORTED);
  },
  simulateOnSocketAccepted: function(serverSocket, socketTransport) {
    this._listener.onSocketAccepted(serverSocket, socketTransport);
  }
};

const mockedSocketTransport = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISocketTransport]),
};

const mockedControlChannel = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationControlChannel]),
  set listener(listener) {
    this._listener = listener;
  },
  get listener() {
    return this._listener;
  },
  sendOffer: function(offer) {
    var isValid = false;
    try {
      var addresses = offer.tcpAddress;
      if (addresses.length > 0) {
        for (var i = 0; i < addresses.length; i++) {
          // Ensure CString addresses are used. Otherwise, an error will be thrown.
          addresses.queryElementAt(i, Ci.nsISupportsCString);
        }

        isValid = true;
      }
    } catch (e) {
      isValid = false;
    }

    sendAsyncMessage('offer-sent', isValid);
  },
  sendAnswer: function(answer) {
    var isValid = false;
    try {
      var addresses = answer.tcpAddress;
      if (addresses.length > 0) {
        for (var i = 0; i < addresses.length; i++) {
          // Ensure CString addresses are used. Otherwise, an error will be thrown.
          addresses.queryElementAt(i, Ci.nsISupportsCString);
        }

        isValid = true;
      }
    } catch (e) {
      isValid = false;
    }

    sendAsyncMessage('answer-sent', isValid);

    this._listener.QueryInterface(Ci.nsIPresentationSessionTransportCallback).notifyTransportReady();
  },
  close: function(reason) {
    sendAsyncMessage('control-channel-closed', reason);
    this._listener.QueryInterface(Ci.nsIPresentationControlChannelListener).notifyClosed(reason);
  },
  simulateReceiverReady: function() {
    this._listener.QueryInterface(Ci.nsIPresentationControlChannelListener).notifyReceiverReady();
  },
  simulateOnOffer: function() {
    sendAsyncMessage('offer-received');
    this._listener.QueryInterface(Ci.nsIPresentationControlChannelListener).onOffer(mockedChannelDescription);
  },
  simulateOnAnswer: function() {
    sendAsyncMessage('answer-received');
    this._listener.QueryInterface(Ci.nsIPresentationControlChannelListener).onAnswer(mockedChannelDescription);
  },
  simulateNotifyOpened: function() {
    sendAsyncMessage('control-channel-opened');
    this._listener.QueryInterface(Ci.nsIPresentationControlChannelListener).notifyOpened();
  },
};

const mockedDevice = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationDevice]),
  id: 'id',
  name: 'name',
  type: 'type',
  establishControlChannel: function(url, presentationId) {
    sendAsyncMessage('control-channel-established');
    return mockedControlChannel;
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
    sendAsyncMessage('device-prompt');
  },
  simulateSelect: function() {
    this._request.select(mockedDevice);
  },
  simulateCancel: function() {
    this._request.cancel();
  }
};

const mockedSessionTransport = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationSessionTransport,
                                         Ci.nsIFactory]),
  createInstance: function(aOuter, aIID) {
    if (aOuter) {
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    }
    return this.QueryInterface(aIID);
  },
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
    sendAsyncMessage('data-transport-initialized');
    this._callback = callback;
    this.simulateTransportReady();
  },
  initWithChannelDescription: function(description, callback) {
    this._callback = callback;

    var addresses = description.QueryInterface(Ci.nsIPresentationChannelDescription).tcpAddress;
    this._selfAddress = {
      QueryInterface: XPCOMUtils.generateQI([Ci.nsINetAddr]),
      address: (addresses.length > 0) ?
                addresses.queryElementAt(0, Ci.nsISupportsCString).data : "",
      port: description.QueryInterface(Ci.nsIPresentationChannelDescription).tcpPort,
    };
  },
  enableDataNotification: function() {
    sendAsyncMessage('data-transport-notification-enabled');
  },
  send: function(data) {
    var binaryStream = Cc["@mozilla.org/binaryinputstream;1"].
                       createInstance(Ci.nsIBinaryInputStream);
    binaryStream.setInputStream(data);
    var message = binaryStream.readBytes(binaryStream.available());
    sendAsyncMessage('message-sent', message);
  },
  close: function(reason) {
    sendAsyncMessage('data-transport-closed', reason);
    this._callback.QueryInterface(Ci.nsIPresentationSessionTransportCallback).notifyTransportClosed(reason);
  },
  simulateTransportReady: function() {
    this._callback.QueryInterface(Ci.nsIPresentationSessionTransportCallback).notifyTransportReady();
  },
  simulateIncomingMessage: function(message) {
    this._callback.QueryInterface(Ci.nsIPresentationSessionTransportCallback).notifyData(message);
  },
};

const mockedNetworkInfo = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsINetworkInfo]),
  getAddresses: function(ips, prefixLengths) {
    ips.value = ["127.0.0.1"];
    prefixLengths.value = [0];
    return 1;
  },
};

const mockedNetworkManager = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsINetworkManager,
                                         Ci.nsIFactory]),
  createInstance: function(aOuter, aIID) {
    if (aOuter) {
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    }
    return this.QueryInterface(aIID);
  },
  get activeNetworkInfo() {
    return mockedNetworkInfo;
  },
};

var requestPromise = null;

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
    sendAsyncMessage('receiver-launching', aSessionId);
    return requestPromise;
  },
};

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
                                               mockedSessionTransport));
originalFactoryData.push(registerMockedFactory("@mozilla.org/network/manager;1",
                                               uuidGenerator.generateUUID(),
                                               mockedNetworkManager));
originalFactoryData.push(registerMockedFactory("@mozilla.org/presentation/requestuiglue;1",
                                               uuidGenerator.generateUUID(),
                                               mockedRequestUIGlue));

function tearDown() {
  requestPromise = null;
  mockedServerSocket.listener = null;
  mockedControlChannel.listener = null;
  mockedDevice.listener = null;
  mockedDevicePrompt.request = null;
  mockedSessionTransport.callback = null;

  var deviceManager = Cc['@mozilla.org/presentation-device/manager;1']
                      .getService(Ci.nsIPresentationDeviceManager);
  deviceManager.QueryInterface(Ci.nsIPresentationDeviceListener).removeDevice(mockedDevice);

  // Register original factories.
  for (var data in originalFactoryData) {
    registerOriginalFactory(data.contractId, data.mockedClassId,
                            data.mockedFactory, data.originalClassId,
                            data.originalFactory);
  }

  sendAsyncMessage('teardown-complete');
}

addMessageListener('trigger-device-add', function() {
  var deviceManager = Cc['@mozilla.org/presentation-device/manager;1']
                      .getService(Ci.nsIPresentationDeviceManager);
  deviceManager.QueryInterface(Ci.nsIPresentationDeviceListener).addDevice(mockedDevice);
});

addMessageListener('trigger-device-prompt-select', function() {
  mockedDevicePrompt.simulateSelect();
});

addMessageListener('trigger-device-prompt-cancel', function() {
  mockedDevicePrompt.simulateCancel();
});

addMessageListener('trigger-incoming-session-request', function(url) {
  var deviceManager = Cc['@mozilla.org/presentation-device/manager;1']
                      .getService(Ci.nsIPresentationDeviceManager);
  deviceManager.QueryInterface(Ci.nsIPresentationDeviceListener)
	       .onSessionRequest(mockedDevice, url, sessionId, mockedControlChannel);
});

addMessageListener('trigger-incoming-offer', function() {
  mockedControlChannel.simulateOnOffer();
});

addMessageListener('trigger-incoming-answer', function() {
  mockedControlChannel.simulateOnAnswer();
});

addMessageListener('trigger-incoming-transport', function() {
  mockedServerSocket.simulateOnSocketAccepted(mockedServerSocket, mockedSocketTransport);
});

addMessageListener('trigger-control-channel-open', function(reason) {
  mockedControlChannel.simulateNotifyOpened();
});

addMessageListener('trigger-control-channel-close', function(reason) {
  mockedControlChannel.close(reason);
});

addMessageListener('trigger-data-transport-close', function(reason) {
  mockedSessionTransport.close(reason);
});

addMessageListener('trigger-incoming-message', function(message) {
  mockedSessionTransport.simulateIncomingMessage(message);
});

addMessageListener('teardown', function() {
  tearDown();
});

var obs = Cc["@mozilla.org/observer-service;1"]
          .getService(Ci.nsIObserverService);
obs.addObserver(function observer(aSubject, aTopic, aData) {
  obs.removeObserver(observer, aTopic);

  requestPromise = aSubject;
}, 'setup-request-promise', false);
