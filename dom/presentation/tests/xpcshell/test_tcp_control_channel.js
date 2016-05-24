/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import('resource://gre/modules/Services.jsm');

var tps;

// Call |run_next_test| if all functions in |names| are called
function makeJointSuccess(names) {
  let funcs = {}, successCount = 0;
  names.forEach(function(name) {
    funcs[name] = function() {
      do_print('got expected: ' + name);
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
    this.tcpAddress.appendElement(wrapper, false);
  }
  this.tcpPort = aTcpPort;
}

TestDescription.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationChannelDescription]),
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
  tps = Cc["@mozilla.org/presentation/control-service;1"]
        .createInstance(Ci.nsIPresentationControlService);
  tps.id = 'controllerID';
  tps.startServer(PRESENTER_CONTROL_CHANNEL_PORT);

  testPresentationServer();
}


function testPresentationServer() {
  let yayFuncs = makeJointSuccess(['controllerControlChannelClose',
                                   'presenterControlChannelClose']);
  let controllerControlChannel;

  tps.listener = {

    onSessionRequest: function(deviceInfo, url, presentationId, controlChannel) {
      controllerControlChannel = controlChannel;
      Assert.equal(deviceInfo.id, tps.id, 'expected device id');
      Assert.equal(deviceInfo.address, '127.0.0.1', 'expected device address');
      Assert.equal(url, 'http://example.com', 'expected url');
      Assert.equal(presentationId, 'testPresentationId', 'expected presentation id');

      controllerControlChannel.listener = {
        status: 'created',
        onOffer: function(aOffer) {
          Assert.equal(this.status, 'opened', '1. controllerControlChannel: get offer, send answer');
          this.status = 'onOffer';

          let offer = aOffer.QueryInterface(Ci.nsIPresentationChannelDescription);
          Assert.strictEqual(offer.tcpAddress.queryElementAt(0,Ci.nsISupportsCString).data,
                             OFFER_ADDRESS,
                             'expected offer address array');
          Assert.equal(offer.tcpPort, OFFER_PORT, 'expected offer port');
          try {
            let tcpType = Ci.nsIPresentationChannelDescription.TYPE_TCP;
            let answer = new TestDescription(tcpType, [ANSWER_ADDRESS], ANSWER_PORT);
            controllerControlChannel.sendAnswer(answer);
          } catch (e) {
            Assert.ok(false, 'sending answer fails' + e);
          }
        },
        onAnswer: function(aAnswer) {
          Assert.ok(false, 'get answer');
        },
        onIceCandidate: function(aCandidate) {
          Assert.ok(true, '3. controllerControlChannel: get ice candidate, close channel');
          let recvCandidate = JSON.parse(aCandidate);
          for (let key in recvCandidate) {
            if (typeof(recvCandidate[key]) !== "function") {
              Assert.equal(recvCandidate[key], candidate[key], "key " + key + " should match.");
            }
          }
          controllerControlChannel.close(CLOSE_CONTROL_CHANNEL_REASON);
        },
        notifyOpened: function() {
          Assert.equal(this.status, 'created', '0. controllerControlChannel: opened');
          this.status = 'opened';
        },
        notifyClosed: function(aReason) {
          Assert.equal(this.status, 'onOffer', '4. controllerControlChannel: closed');
          Assert.equal(aReason, CLOSE_CONTROL_CHANNEL_REASON, 'presenterControlChannel notify closed');
          this.status = 'closed';
          yayFuncs.controllerControlChannelClose();
        },
        QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationControlChannelListener]),
      };
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationControlServerListener]),
  };

  let presenterDeviceInfo = {
    id: 'presentatorID',
    address: '127.0.0.1',
    port: PRESENTER_CONTROL_CHANNEL_PORT,
    QueryInterface: XPCOMUtils.generateQI([Ci.nsITCPDeviceInfo]),
  };

  let presenterControlChannel = tps.requestSession(presenterDeviceInfo,
                                                   'http://example.com',
                                                   'testPresentationId');

  presenterControlChannel.listener = {
    status: 'created',
    onOffer: function(offer) {
      Assert.ok(false, 'get offer');
    },
    onAnswer: function(aAnswer) {
      Assert.equal(this.status, 'opened', '2. presenterControlChannel: get answer, send ICE candidate');

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
      presenterControlChannel.sendIceCandidate(JSON.stringify(candidate));
    },
    onIceCandidate: function(aCandidate) {
      Assert.ok(false, 'get ICE candidate');
    },
    notifyOpened: function() {
      Assert.equal(this.status, 'created', '0. presenterControlChannel: opened, send offer');
      this.status = 'opened';
      try {
        let tcpType = Ci.nsIPresentationChannelDescription.TYPE_TCP;
        let offer = new TestDescription(tcpType, [OFFER_ADDRESS], OFFER_PORT)
        presenterControlChannel.sendOffer(offer);
      } catch (e) {
        Assert.ok(false, 'sending offer fails:' + e);
      }
    },
    notifyClosed: function(aReason) {
      this.status = 'closed';
      Assert.equal(aReason, CLOSE_CONTROL_CHANNEL_REASON, '4. presenterControlChannel notify closed');
      yayFuncs.presenterControlChannelClose();
    },
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationControlChannelListener]),
  };
}

function setOffline() {
  tps.listener = {
    onPortChange: function(aPort) {
      Assert.notEqual(aPort, 0, 'TCPPresentationServer port changed and the port should be valid');
      tps.close();
      run_next_test();
    },
  };

  // Let the server socket restart automatically.
  Services.io.offline = true;
  Services.io.offline = false;
}

function oneMoreLoop() {
  try {
    tps.startServer(PRESENTER_CONTROL_CHANNEL_PORT);
    testPresentationServer();
  } catch (e) {
    Assert.ok(false, 'TCP presentation init fail:' + e);
    run_next_test();
  }
}


function shutdown()
{
  tps.listener = {
    onPortChange: function(aPort) {
      Assert.ok(false, 'TCPPresentationServer port changed');
    },
  };
  tps.close();
  Assert.equal(tps.port, 0, "TCPPresentationServer closed");
  run_next_test();
}

// Test manually close control channel with NS_ERROR_FAILURE
function changeCloseReason() {
  CLOSE_CONTROL_CHANNEL_REASON = Cr.NS_ERROR_FAILURE;
  run_next_test();
}

add_test(loopOfferAnser);
add_test(setOffline);
add_test(changeCloseReason);
add_test(oneMoreLoop);
add_test(shutdown);

function run_test() {
  Services.prefs.setBoolPref("dom.presentation.tcp_server.debug", true);

  do_register_cleanup(() => {
    Services.prefs.clearUserPref("dom.presentation.tcp_server.debug");
  });

  run_next_test();
}
