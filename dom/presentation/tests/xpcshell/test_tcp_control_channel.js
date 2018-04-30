/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

ChromeUtils.import('resource://gre/modules/XPCOMUtils.jsm');
ChromeUtils.import('resource://gre/modules/Services.jsm');

var pcs;

// Call |run_next_test| if all functions in |names| are called
function makeJointSuccess(names) {
  let funcs = {}, successCount = 0;
  names.forEach(function(name) {
    funcs[name] = function() {
      info('got expected: ' + name);
      if (++successCount === names.length)
        run_next_test();
    };
  });
  return funcs;
}

function TestDescription(aType, aTcpAddress, aTcpPort) {
  this.type = aType;
  this.tcpAddress = Cc["@mozilla.org/array;1"]
                      .createInstance(Ci.nsIMutableArray);
  for (let address of aTcpAddress) {
    let wrapper = Cc["@mozilla.org/supports-cstring;1"]
                    .createInstance(Ci.nsISupportsCString);
    wrapper.data = address;
    this.tcpAddress.appendElement(wrapper);
  }
  this.tcpPort = aTcpPort;
}

TestDescription.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIPresentationChannelDescription]),
}

const CONTROLLER_CONTROL_CHANNEL_PORT = 36777;
const PRESENTER_CONTROL_CHANNEL_PORT = 36888;

var CLOSE_CONTROL_CHANNEL_REASON = Cr.NS_OK;
var candidate;

// presenter's presentation channel description
const OFFER_ADDRESS = '192.168.123.123';
const OFFER_PORT = 123;

// controller's presentation channel description
const ANSWER_ADDRESS = '192.168.321.321';
const ANSWER_PORT = 321;

function loopOfferAnser() {
  pcs = Cc["@mozilla.org/presentation/control-service;1"]
        .createInstance(Ci.nsIPresentationControlService);
  pcs.id = 'controllerID';
  pcs.listener = {
    onServerReady: function() {
      testPresentationServer();
    }
  };

  // First run with TLS enabled.
  pcs.startServer(true, PRESENTER_CONTROL_CHANNEL_PORT);
}


function testPresentationServer() {
  let yayFuncs = makeJointSuccess(['controllerControlChannelClose',
                                   'presenterControlChannelClose',
                                   'controllerControlChannelReconnect',
                                   'presenterControlChannelReconnect']);
  let presenterControlChannel;

  pcs.listener = {

    onSessionRequest: function(deviceInfo, url, presentationId, controlChannel) {
      presenterControlChannel = controlChannel;
      Assert.equal(deviceInfo.id, pcs.id, 'expected device id');
      Assert.equal(deviceInfo.address, '127.0.0.1', 'expected device address');
      Assert.equal(url, 'http://example.com', 'expected url');
      Assert.equal(presentationId, 'testPresentationId', 'expected presentation id');

      presenterControlChannel.listener = {
        status: 'created',
        onOffer: function(aOffer) {
          Assert.equal(this.status, 'opened', '1. presenterControlChannel: get offer, send answer');
          this.status = 'onOffer';

          let offer = aOffer.QueryInterface(Ci.nsIPresentationChannelDescription);
          Assert.strictEqual(offer.tcpAddress.queryElementAt(0,Ci.nsISupportsCString).data,
                             OFFER_ADDRESS,
                             'expected offer address array');
          Assert.equal(offer.tcpPort, OFFER_PORT, 'expected offer port');
          try {
            let tcpType = Ci.nsIPresentationChannelDescription.TYPE_TCP;
            let answer = new TestDescription(tcpType, [ANSWER_ADDRESS], ANSWER_PORT);
            presenterControlChannel.sendAnswer(answer);
          } catch (e) {
            Assert.ok(false, 'sending answer fails' + e);
          }
        },
        onAnswer: function(aAnswer) {
          Assert.ok(false, 'get answer');
        },
        onIceCandidate: function(aCandidate) {
          Assert.ok(true, '3. presenterControlChannel: get ice candidate, close channel');
          let recvCandidate = JSON.parse(aCandidate);
          for (let key in recvCandidate) {
            if (typeof(recvCandidate[key]) !== "function") {
              Assert.equal(recvCandidate[key], candidate[key], "key " + key + " should match.");
            }
          }
          presenterControlChannel.disconnect(CLOSE_CONTROL_CHANNEL_REASON);
        },
        notifyConnected: function() {
          Assert.equal(this.status, 'created', '0. presenterControlChannel: opened');
          this.status = 'opened';
        },
        notifyDisconnected: function(aReason) {
          Assert.equal(this.status, 'onOffer', '4. presenterControlChannel: closed');
          Assert.equal(aReason, CLOSE_CONTROL_CHANNEL_REASON, 'presenterControlChannel notify closed');
          this.status = 'closed';
          yayFuncs.controllerControlChannelClose();
        },
        QueryInterface: ChromeUtils.generateQI([Ci.nsIPresentationControlChannelListener]),
      };
    },
    onReconnectRequest: function(deviceInfo, url, presentationId, controlChannel) {
      Assert.equal(url, 'http://example.com', 'expected url');
      Assert.equal(presentationId, 'testPresentationId', 'expected presentation id');
      yayFuncs.presenterControlChannelReconnect();
    },

    QueryInterface: ChromeUtils.generateQI([Ci.nsIPresentationControlServerListener]),
  };

  let presenterDeviceInfo = {
    id: 'presentatorID',
    address: '127.0.0.1',
    port: PRESENTER_CONTROL_CHANNEL_PORT,
    certFingerprint: pcs.certFingerprint,
    QueryInterface: ChromeUtils.generateQI([Ci.nsITCPDeviceInfo]),
  };

  let controllerControlChannel = pcs.connect(presenterDeviceInfo);

  controllerControlChannel.listener = {
    status: 'created',
    onOffer: function(offer) {
      Assert.ok(false, 'get offer');
    },
    onAnswer: function(aAnswer) {
      Assert.equal(this.status, 'opened', '2. controllerControlChannel: get answer, send ICE candidate');

      let answer = aAnswer.QueryInterface(Ci.nsIPresentationChannelDescription);
      Assert.strictEqual(answer.tcpAddress.queryElementAt(0,Ci.nsISupportsCString).data,
                         ANSWER_ADDRESS,
                         'expected answer address array');
      Assert.equal(answer.tcpPort, ANSWER_PORT, 'expected answer port');
      candidate = {
        candidate: "1 1 UDP 1 127.0.0.1 34567 type host",
        sdpMid: "helloworld",
        sdpMLineIndex: 1
      };
      controllerControlChannel.sendIceCandidate(JSON.stringify(candidate));
    },
    onIceCandidate: function(aCandidate) {
      Assert.ok(false, 'get ICE candidate');
    },
    notifyConnected: function() {
      Assert.equal(this.status, 'created', '0. controllerControlChannel: opened, send offer');
      controllerControlChannel.launch('testPresentationId', 'http://example.com');
      this.status = 'opened';
      try {
        let tcpType = Ci.nsIPresentationChannelDescription.TYPE_TCP;
        let offer = new TestDescription(tcpType, [OFFER_ADDRESS], OFFER_PORT)
        controllerControlChannel.sendOffer(offer);
      } catch (e) {
        Assert.ok(false, 'sending offer fails:' + e);
      }
    },
    notifyDisconnected: function(aReason) {
      this.status = 'closed';
      Assert.equal(aReason, CLOSE_CONTROL_CHANNEL_REASON, '4. controllerControlChannel notify closed');
      yayFuncs.presenterControlChannelClose();

      let reconnectControllerControlChannel = pcs.connect(presenterDeviceInfo);
      reconnectControllerControlChannel.listener = {
        notifyConnected: function() {
          reconnectControllerControlChannel.reconnect('testPresentationId', 'http://example.com');
        },
        notifyReconnected: function() {
          yayFuncs.controllerControlChannelReconnect();
        },
        QueryInterface: ChromeUtils.generateQI([Ci.nsIPresentationControlChannelListener]),
      };
    },
    QueryInterface: ChromeUtils.generateQI([Ci.nsIPresentationControlChannelListener]),
  };
}

function terminateRequest() {
  let yayFuncs = makeJointSuccess(['controllerControlChannelConnected',
                                   'controllerControlChannelDisconnected',
                                   'presenterControlChannelDisconnected',
                                   'terminatedByController',
                                   'terminatedByReceiver']);
  let controllerControlChannel;
  let terminatePhase = 'controller';

  pcs.listener = {
    onTerminateRequest: function(deviceInfo, presentationId, controlChannel, isFromReceiver) {
      Assert.equal(deviceInfo.address, '127.0.0.1', 'expected device address');
      Assert.equal(presentationId, 'testPresentationId', 'expected presentation id');
      controlChannel.terminate(presentationId); // Reply terminate ack.

      if (terminatePhase === 'controller') {
        controllerControlChannel = controlChannel;
        Assert.equal(deviceInfo.id, pcs.id, 'expected controller device id');
        Assert.equal(isFromReceiver, false, 'expected request from controller');
        yayFuncs.terminatedByController();

        controllerControlChannel.listener = {
          notifyConnected: function() {
            Assert.ok(true, 'control channel notify connected');
            yayFuncs.controllerControlChannelConnected();

            terminatePhase = 'receiver';
            controllerControlChannel.terminate('testPresentationId');
          },
          notifyDisconnected: function(aReason) {
            Assert.equal(aReason, CLOSE_CONTROL_CHANNEL_REASON, 'controllerControlChannel notify disconncted');
            yayFuncs.controllerControlChannelDisconnected();
          },
          QueryInterface: ChromeUtils.generateQI([Ci.nsIPresentationControlChannelListener]),
        };
      } else {
        Assert.equal(deviceInfo.id, presenterDeviceInfo.id, 'expected presenter device id');
        Assert.equal(isFromReceiver, true, 'expected request from receiver');
        yayFuncs.terminatedByReceiver();
        presenterControlChannel.disconnect(CLOSE_CONTROL_CHANNEL_REASON);
      }
    },
    QueryInterface: ChromeUtils.generateQI([Ci.nsITCPPresentationServerListener]),
  };

  let presenterDeviceInfo = {
    id: 'presentatorID',
    address: '127.0.0.1',
    port: PRESENTER_CONTROL_CHANNEL_PORT,
    certFingerprint: pcs.certFingerprint,
    QueryInterface: ChromeUtils.generateQI([Ci.nsITCPDeviceInfo]),
  };

  let presenterControlChannel = pcs.connect(presenterDeviceInfo);

  presenterControlChannel.listener = {
    notifyConnected: function() {
      presenterControlChannel.terminate('testPresentationId');
    },
    notifyDisconnected: function(aReason) {
      Assert.equal(aReason, CLOSE_CONTROL_CHANNEL_REASON, '4. presenterControlChannel notify disconnected');
      yayFuncs.presenterControlChannelDisconnected();
    },
    QueryInterface: ChromeUtils.generateQI([Ci.nsIPresentationControlChannelListener]),
  };
}

function terminateRequestAbnormal() {
  let yayFuncs = makeJointSuccess(['controllerControlChannelConnected',
                                   'controllerControlChannelDisconnected',
                                   'presenterControlChannelDisconnected']);
  let controllerControlChannel;

  pcs.listener = {
    onTerminateRequest: function(deviceInfo, presentationId, controlChannel, isFromReceiver) {
      Assert.equal(deviceInfo.id, pcs.id, 'expected controller device id');
      Assert.equal(deviceInfo.address, '127.0.0.1', 'expected device address');
      Assert.equal(presentationId, 'testPresentationId', 'expected presentation id');
      Assert.equal(isFromReceiver, false, 'expected request from controller');
      controlChannel.terminate('unmatched-presentationId'); // Reply abnormal terminate ack.

      controllerControlChannel = controlChannel;

      controllerControlChannel.listener = {
          notifyConnected: function() {
          Assert.ok(true, 'control channel notify connected');
          yayFuncs.controllerControlChannelConnected();
        },
        notifyDisconnected: function(aReason) {
          Assert.equal(aReason, Cr.NS_ERROR_FAILURE, 'controllerControlChannel notify disconncted with error');
          yayFuncs.controllerControlChannelDisconnected();
        },
        QueryInterface: ChromeUtils.generateQI([Ci.nsIPresentationControlChannelListener]),
      };
    },
    QueryInterface: ChromeUtils.generateQI([Ci.nsITCPPresentationServerListener]),
  };

  let presenterDeviceInfo = {
    id: 'presentatorID',
    address: '127.0.0.1',
    port: PRESENTER_CONTROL_CHANNEL_PORT,
    certFingerprint: pcs.certFingerprint,
    QueryInterface: ChromeUtils.generateQI([Ci.nsITCPDeviceInfo]),
  };

  let presenterControlChannel = pcs.connect(presenterDeviceInfo);

  presenterControlChannel.listener = {
    notifyConnected: function() {
      presenterControlChannel.terminate('testPresentationId');
    },
    notifyDisconnected: function(aReason) {
      Assert.equal(aReason, Cr.NS_ERROR_FAILURE, '4. presenterControlChannel notify disconnected with error');
      yayFuncs.presenterControlChannelDisconnected();
    },
    QueryInterface: ChromeUtils.generateQI([Ci.nsIPresentationControlChannelListener]),
  };
}

function setOffline() {
  pcs.listener = {
    onServerReady: function(aPort, aCertFingerprint) {
      Assert.notEqual(aPort, 0, 'TCPPresentationServer port changed and the port should be valid');
      pcs.close();
      run_next_test();
    },
  };

  // Let the server socket restart automatically.
  Services.io.offline = true;
  Services.io.offline = false;
}

function oneMoreLoop() {
  try {
    pcs.listener = {
      onServerReady: function() {
        testPresentationServer();
      }
    };

    // Second run with TLS disabled.
    pcs.startServer(false, PRESENTER_CONTROL_CHANNEL_PORT);
  } catch (e) {
    Assert.ok(false, 'TCP presentation init fail:' + e);
    run_next_test();
  }
}


function shutdown()
{
  pcs.listener = {
    onServerReady: function(aPort, aCertFingerprint) {
      Assert.ok(false, 'TCPPresentationServer port changed');
    },
  };
  pcs.close();
  Assert.equal(pcs.port, 0, "TCPPresentationServer closed");
  run_next_test();
}

// Test manually close control channel with NS_ERROR_FAILURE
function changeCloseReason() {
  CLOSE_CONTROL_CHANNEL_REASON = Cr.NS_ERROR_FAILURE;
  run_next_test();
}

add_test(loopOfferAnser);
add_test(terminateRequest);
add_test(terminateRequestAbnormal);
add_test(setOffline);
add_test(changeCloseReason);
add_test(oneMoreLoop);
add_test(shutdown);

function run_test() {
  // Need profile dir to store the key / cert
  do_get_profile();
  // Ensure PSM is initialized
  Cc["@mozilla.org/psm;1"].getService(Ci.nsISupports);

  Services.prefs.setBoolPref("dom.presentation.tcp_server.debug", true);

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("dom.presentation.tcp_server.debug");
  });

  run_next_test();
}
