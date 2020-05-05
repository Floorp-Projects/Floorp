/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/* eslint-env mozilla/frame-script */

const Cm = Components.manager;

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

const uuidGenerator = Cc["@mozilla.org/uuid-generator;1"].getService(
  Ci.nsIUUIDGenerator
);

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
    registrar.registerFactory(mockedClassId, "", contractId, mockedFactory);
  }

  return {
    contractId,
    mockedClassId,
    mockedFactory,
    originalClassId,
    originalFactory,
  };
}

function registerOriginalFactory(
  contractId,
  mockedClassId,
  mockedFactory,
  originalClassId,
  originalFactory
) {
  if (originalFactory) {
    var registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
    registrar.unregisterFactory(mockedClassId, mockedFactory);
    // Passing null for the factory remaps the original CID to the
    // contract ID.
    registrar.registerFactory(originalClassId, "", contractId, null);
  }
}

var sessionId = "test-session-id-" + uuidGenerator.generateUUID().toString();

const address = Cc["@mozilla.org/supports-cstring;1"].createInstance(
  Ci.nsISupportsCString
);
address.data = "127.0.0.1";
const addresses = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
addresses.appendElement(address);

const mockedChannelDescription = {
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIPresentationChannelDescription,
  ]),
  get type() {
    if (
      Services.prefs.getBoolPref(
        "dom.presentation.session_transport.data_channel.enable"
      )
    ) {
      return Ci.nsIPresentationChannelDescription.TYPE_DATACHANNEL;
    }
    return Ci.nsIPresentationChannelDescription.TYPE_TCP;
  },
  tcpAddress: addresses,
  tcpPort: 1234,
};

const mockedServerSocket = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIServerSocket, Ci.nsIFactory]),
  createInstance(aOuter, aIID) {
    if (aOuter) {
      throw Components.Exception("", Cr.NS_ERROR_NO_AGGREGATION);
    }
    return this.QueryInterface(aIID);
  },
  get port() {
    return this._port;
  },
  set listener(listener) {
    this._listener = listener;
  },
  init(port, loopbackOnly, backLog) {
    if (port != -1) {
      this._port = port;
    } else {
      this._port = 5678;
    }
  },
  asyncListen(listener) {
    this._listener = listener;
  },
  close() {
    this._listener.onStopListening(this, Cr.NS_BINDING_ABORTED);
  },
  simulateOnSocketAccepted(serverSocket, socketTransport) {
    this._listener.onSocketAccepted(serverSocket, socketTransport);
  },
};

const mockedSocketTransport = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsISocketTransport]),
};

const mockedControlChannel = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIPresentationControlChannel]),
  set listener(listener) {
    this._listener = listener;
  },
  get listener() {
    return this._listener;
  },
  sendOffer(offer) {
    sendAsyncMessage("offer-sent", this._isValidSDP(offer));
  },
  sendAnswer(answer) {
    sendAsyncMessage("answer-sent", this._isValidSDP(answer));

    if (answer.type == Ci.nsIPresentationChannelDescription.TYPE_TCP) {
      this._listener
        .QueryInterface(Ci.nsIPresentationSessionTransportCallback)
        .notifyTransportReady();
    }
  },
  _isValidSDP(aSDP) {
    var isValid = false;
    if (aSDP.type == Ci.nsIPresentationChannelDescription.TYPE_TCP) {
      try {
        var sdpAddresses = aSDP.tcpAddress;
        if (sdpAddresses.length > 0) {
          for (var i = 0; i < sdpAddresses.length; i++) {
            // Ensure CString addresses are used. Otherwise, an error will be thrown.
            sdpAddresses.queryElementAt(i, Ci.nsISupportsCString);
          }

          isValid = true;
        }
      } catch (e) {
        isValid = false;
      }
    } else if (
      aSDP.type == Ci.nsIPresentationChannelDescription.TYPE_DATACHANNEL
    ) {
      isValid = aSDP.dataChannelSDP == "test-sdp";
    }
    return isValid;
  },
  launch(presentationId, url) {
    sessionId = presentationId;
  },
  terminate(presentationId) {
    sendAsyncMessage("sender-terminate", presentationId);
  },
  reconnect(presentationId, url) {
    sendAsyncMessage("start-reconnect", url);
  },
  notifyReconnected() {
    this._listener
      .QueryInterface(Ci.nsIPresentationControlChannelListener)
      .notifyReconnected();
  },
  disconnect(reason) {
    sendAsyncMessage("control-channel-closed", reason);
    this._listener
      .QueryInterface(Ci.nsIPresentationControlChannelListener)
      .notifyDisconnected(reason);
  },
  simulateReceiverReady() {
    this._listener
      .QueryInterface(Ci.nsIPresentationControlChannelListener)
      .notifyReceiverReady();
  },
  simulateOnOffer() {
    sendAsyncMessage("offer-received");
    this._listener
      .QueryInterface(Ci.nsIPresentationControlChannelListener)
      .onOffer(mockedChannelDescription);
  },
  simulateOnAnswer() {
    sendAsyncMessage("answer-received");
    this._listener
      .QueryInterface(Ci.nsIPresentationControlChannelListener)
      .onAnswer(mockedChannelDescription);
  },
  simulateNotifyConnected() {
    sendAsyncMessage("control-channel-opened");
    this._listener
      .QueryInterface(Ci.nsIPresentationControlChannelListener)
      .notifyConnected();
  },
};

const mockedDevice = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIPresentationDevice]),
  id: "id",
  name: "name",
  type: "type",
  establishControlChannel(url, presentationId) {
    sendAsyncMessage("control-channel-established");
    return mockedControlChannel;
  },
  disconnect() {},
  isRequestedUrlSupported(requestedUrl) {
    return true;
  },
};

const mockedDevicePrompt = {
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIPresentationDevicePrompt,
    Ci.nsIFactory,
  ]),
  createInstance(aOuter, aIID) {
    if (aOuter) {
      throw Components.Exception("", Cr.NS_ERROR_NO_AGGREGATION);
    }
    return this.QueryInterface(aIID);
  },
  set request(request) {
    this._request = request;
  },
  get request() {
    return this._request;
  },
  promptDeviceSelection(request) {
    this._request = request;
    sendAsyncMessage("device-prompt");
  },
  simulateSelect() {
    this._request.select(mockedDevice);
  },
  simulateCancel(result) {
    this._request.cancel(result);
  },
};

const mockedSessionTransport = {
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIPresentationSessionTransport,
    Ci.nsIPresentationSessionTransportBuilder,
    Ci.nsIPresentationTCPSessionTransportBuilder,
    Ci.nsIPresentationDataChannelSessionTransportBuilder,
    Ci.nsIPresentationControlChannelListener,
    Ci.nsIFactory,
  ]),
  createInstance(aOuter, aIID) {
    if (aOuter) {
      throw Components.Exception("", Cr.NS_ERROR_NO_AGGREGATION);
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
  buildTCPSenderTransport(transport, listener) {
    this._listener = listener;
    this._role = Ci.nsIPresentationService.ROLE_CONTROLLER;
    this._listener.onSessionTransport(this);
    this._listener = null;
    sendAsyncMessage("data-transport-initialized");

    setTimeout(() => {
      this.simulateTransportReady();
    }, 0);
  },
  buildTCPReceiverTransport(description, listener) {
    this._listener = listener;
    this._role = Ci.nsIPresentationService.ROLE_RECEIVER;

    var tcpAddresses = description.QueryInterface(
      Ci.nsIPresentationChannelDescription
    ).tcpAddress;
    this._selfAddress = {
      QueryInterface: ChromeUtils.generateQI([Ci.nsINetAddr]),
      address:
        tcpAddresses.length > 0
          ? tcpAddresses.queryElementAt(0, Ci.nsISupportsCString).data
          : "",
      port: description.QueryInterface(Ci.nsIPresentationChannelDescription)
        .tcpPort,
    };

    setTimeout(() => {
      this._listener.onSessionTransport(this);
      this._listener = null;
    }, 0);
  },
  // in-process case
  buildDataChannelTransport(role, window, listener) {
    this._listener = listener;
    this._role = role;

    var hasNavigator = window ? typeof window.navigator != "undefined" : false;
    sendAsyncMessage("check-navigator", hasNavigator);

    setTimeout(() => {
      this._listener.onSessionTransport(this);
      this._listener = null;
      this.simulateTransportReady();
    }, 0);
  },
  enableDataNotification() {
    sendAsyncMessage("data-transport-notification-enabled");
  },
  send(data) {
    sendAsyncMessage("message-sent", data);
  },
  close(reason) {
    // Don't send a message after tearDown, to avoid a leak.
    if (this._callback) {
      sendAsyncMessage("data-transport-closed", reason);
      this._callback
        .QueryInterface(Ci.nsIPresentationSessionTransportCallback)
        .notifyTransportClosed(reason);
    }
  },
  simulateTransportReady() {
    this._callback
      .QueryInterface(Ci.nsIPresentationSessionTransportCallback)
      .notifyTransportReady();
  },
  simulateIncomingMessage(message) {
    this._callback
      .QueryInterface(Ci.nsIPresentationSessionTransportCallback)
      .notifyData(message, false);
  },
  onOffer(aOffer) {},
  onAnswer(aAnswer) {},
};

const mockedNetworkInfo = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsINetworkInfo]),
  getAddresses(ips, prefixLengths) {
    ips.value = ["127.0.0.1"];
    prefixLengths.value = [0];
    return 1;
  },
};

const mockedNetworkManager = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsINetworkManager, Ci.nsIFactory]),
  createInstance(aOuter, aIID) {
    if (aOuter) {
      throw Components.Exception("", Cr.NS_ERROR_NO_AGGREGATION);
    }
    return this.QueryInterface(aIID);
  },
  get activeNetworkInfo() {
    return mockedNetworkInfo;
  },
};

var requestPromise = null;

const mockedRequestUIGlue = {
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIPresentationRequestUIGlue,
    Ci.nsIFactory,
  ]),
  createInstance(aOuter, aIID) {
    if (aOuter) {
      throw Components.Exception("", Cr.NS_ERROR_NO_AGGREGATION);
    }
    return this.QueryInterface(aIID);
  },
  sendRequest(aUrl, aSessionId) {
    sendAsyncMessage("receiver-launching", aSessionId);
    return requestPromise;
  },
};

// Register mocked factories.
const originalFactoryData = [];
originalFactoryData.push(
  registerMockedFactory(
    "@mozilla.org/presentation-device/prompt;1",
    uuidGenerator.generateUUID(),
    mockedDevicePrompt
  )
);
originalFactoryData.push(
  registerMockedFactory(
    "@mozilla.org/network/server-socket;1",
    uuidGenerator.generateUUID(),
    mockedServerSocket
  )
);
originalFactoryData.push(
  registerMockedFactory(
    "@mozilla.org/presentation/presentationtcpsessiontransport;1",
    uuidGenerator.generateUUID(),
    mockedSessionTransport
  )
);
originalFactoryData.push(
  registerMockedFactory(
    "@mozilla.org/presentation/datachanneltransportbuilder;1",
    uuidGenerator.generateUUID(),
    mockedSessionTransport
  )
);
originalFactoryData.push(
  registerMockedFactory(
    "@mozilla.org/network/manager;1",
    uuidGenerator.generateUUID(),
    mockedNetworkManager
  )
);
originalFactoryData.push(
  registerMockedFactory(
    "@mozilla.org/presentation/requestuiglue;1",
    uuidGenerator.generateUUID(),
    mockedRequestUIGlue
  )
);

function tearDown() {
  requestPromise = null;
  mockedServerSocket.listener = null;
  mockedControlChannel.listener = null;
  mockedDevice.listener = null;
  mockedDevicePrompt.request = null;
  mockedSessionTransport.callback = null;

  var deviceManager = Cc[
    "@mozilla.org/presentation-device/manager;1"
  ].getService(Ci.nsIPresentationDeviceManager);
  deviceManager
    .QueryInterface(Ci.nsIPresentationDeviceListener)
    .removeDevice(mockedDevice);

  // Register original factories.
  for (var data of originalFactoryData) {
    registerOriginalFactory(
      data.contractId,
      data.mockedClassId,
      data.mockedFactory,
      data.originalClassId,
      data.originalFactory
    );
  }

  sendAsyncMessage("teardown-complete");
}

addMessageListener("trigger-device-add", function() {
  var deviceManager = Cc[
    "@mozilla.org/presentation-device/manager;1"
  ].getService(Ci.nsIPresentationDeviceManager);
  deviceManager
    .QueryInterface(Ci.nsIPresentationDeviceListener)
    .addDevice(mockedDevice);
});

addMessageListener("trigger-device-prompt-select", function() {
  mockedDevicePrompt.simulateSelect();
});

addMessageListener("trigger-device-prompt-cancel", function(result) {
  mockedDevicePrompt.simulateCancel(result);
});

addMessageListener("trigger-incoming-session-request", function(url) {
  var deviceManager = Cc[
    "@mozilla.org/presentation-device/manager;1"
  ].getService(Ci.nsIPresentationDeviceManager);
  deviceManager
    .QueryInterface(Ci.nsIPresentationDeviceListener)
    .onSessionRequest(mockedDevice, url, sessionId, mockedControlChannel);
});

addMessageListener("trigger-incoming-terminate-request", function() {
  var deviceManager = Cc[
    "@mozilla.org/presentation-device/manager;1"
  ].getService(Ci.nsIPresentationDeviceManager);
  deviceManager
    .QueryInterface(Ci.nsIPresentationDeviceListener)
    .onTerminateRequest(mockedDevice, sessionId, mockedControlChannel, true);
});

addMessageListener("trigger-reconnected-acked", function(url) {
  mockedControlChannel.notifyReconnected();
});

addMessageListener("trigger-incoming-offer", function() {
  mockedControlChannel.simulateOnOffer();
});

addMessageListener("trigger-incoming-answer", function() {
  mockedControlChannel.simulateOnAnswer();
});

addMessageListener("trigger-incoming-transport", function() {
  mockedServerSocket.simulateOnSocketAccepted(
    mockedServerSocket,
    mockedSocketTransport
  );
});

addMessageListener("trigger-control-channel-open", function(reason) {
  mockedControlChannel.simulateNotifyConnected();
});

addMessageListener("trigger-control-channel-close", function(reason) {
  mockedControlChannel.disconnect(reason);
});

addMessageListener("trigger-data-transport-close", function(reason) {
  mockedSessionTransport.close(reason);
});

addMessageListener("trigger-incoming-message", function(message) {
  mockedSessionTransport.simulateIncomingMessage(message);
});

addMessageListener("teardown", function() {
  tearDown();
});

var controlChannelListener;
addMessageListener("save-control-channel-listener", function() {
  controlChannelListener = mockedControlChannel.listener;
});

addMessageListener("restore-control-channel-listener", function(message) {
  mockedControlChannel.listener = controlChannelListener;
  controlChannelListener = null;
});

Services.obs.addObserver(function observer(aSubject, aTopic, aData) {
  Services.obs.removeObserver(observer, aTopic);

  requestPromise = aSubject;
}, "setup-request-promise");
