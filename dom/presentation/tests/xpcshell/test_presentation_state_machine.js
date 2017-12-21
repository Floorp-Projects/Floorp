/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* jshint esnext:true, globalstrict:true, moz:true, undef:true, unused:true */
/* globals Components,Assert,run_next_test,add_test,do_execute_soon */

'use strict';

const { utils: Cu, results: Cr } = Components;

/* globals ControllerStateMachine */
Cu.import('resource://gre/modules/presentation/ControllerStateMachine.jsm');
/* globals ReceiverStateMachine */
Cu.import('resource://gre/modules/presentation/ReceiverStateMachine.jsm');
/* globals State */
Cu.import('resource://gre/modules/presentation/StateMachineHelper.jsm');

const testControllerId = 'test-controller-id';
const testPresentationId = 'test-presentation-id';
const testUrl = 'http://example.org';

let mockControllerChannel = {};
let mockReceiverChannel = {};

let controllerState = new ControllerStateMachine(mockControllerChannel, testControllerId);
let receiverState = new ReceiverStateMachine(mockReceiverChannel);

mockControllerChannel.sendCommand = function(command) {
  executeSoon(function() {
    receiverState.onCommand(command);
  });
};

mockReceiverChannel.sendCommand = function(command) {
  executeSoon(function() {
    controllerState.onCommand(command);
  });
};

function connect() {
  Assert.equal(controllerState.state, State.INIT, 'controller in init state');
  Assert.equal(receiverState.state, State.INIT, 'receiver in init state');
  // step 1: underlying connection is ready
  controllerState.onChannelReady();
  Assert.equal(controllerState.state, State.CONNECTING, 'controller in connecting state');
  receiverState.onChannelReady();
  Assert.equal(receiverState.state, State.CONNECTING, 'receiver in connecting state');

  // step 2: receiver reply to connect command
  mockReceiverChannel.notifyDeviceConnected = function(deviceId) {
    Assert.equal(deviceId, testControllerId, 'receiver connect to mock controller');
    Assert.equal(receiverState.state, State.CONNECTED, 'receiver in connected state');

    // step 3: controller receive connect-ack command
    mockControllerChannel.notifyDeviceConnected = function() {
      Assert.equal(controllerState.state, State.CONNECTED, 'controller in connected state');
      run_next_test();
    };
  };
}

function launch() {
  Assert.equal(controllerState.state, State.CONNECTED, 'controller in connected state');
  Assert.equal(receiverState.state, State.CONNECTED, 'receiver in connected state');

  controllerState.launch(testPresentationId, testUrl);
  mockReceiverChannel.notifyLaunch = function(presentationId, url) {
    Assert.equal(receiverState.state, State.CONNECTED, 'receiver in connected state');
    Assert.equal(presentationId, testPresentationId, 'expected presentationId received');
    Assert.equal(url, testUrl, 'expected url received');

    mockControllerChannel.notifyLaunch = function(presentationId) {
      Assert.equal(controllerState.state, State.CONNECTED, 'controller in connected state');
      Assert.equal(presentationId, testPresentationId, 'expected presentationId received from ack');

      run_next_test();
    };
  };
}

function terminateByController() {
  Assert.equal(controllerState.state, State.CONNECTED, 'controller in connected state');
  Assert.equal(receiverState.state, State.CONNECTED, 'receiver in connected state');

  controllerState.terminate(testPresentationId);
  mockReceiverChannel.notifyTerminate = function(presentationId) {
    Assert.equal(receiverState.state, State.CONNECTED, 'receiver in connected state');
    Assert.equal(presentationId, testPresentationId, 'expected presentationId received');

    mockControllerChannel.notifyTerminate = function(presentationId) {
      Assert.equal(controllerState.state, State.CONNECTED, 'controller in connected state');
      Assert.equal(presentationId, testPresentationId, 'expected presentationId received from ack');

      run_next_test();
    };

    receiverState.terminateAck(presentationId);
  };
}

function terminateByReceiver() {
  Assert.equal(controllerState.state, State.CONNECTED, 'controller in connected state');
  Assert.equal(receiverState.state, State.CONNECTED, 'receiver in connected state');

  receiverState.terminate(testPresentationId);
  mockControllerChannel.notifyTerminate = function(presentationId) {
    Assert.equal(controllerState.state, State.CONNECTED, 'controller in connected state');
    Assert.equal(presentationId, testPresentationId, 'expected presentationId received');

    mockReceiverChannel.notifyTerminate = function(presentationId) {
      Assert.equal(receiverState.state, State.CONNECTED, 'receiver in connected state');
      Assert.equal(presentationId, testPresentationId, 'expected presentationId received from ack');
      run_next_test();
    };

    controllerState.terminateAck(presentationId);
  };
}

function exchangeSDP() {
  Assert.equal(controllerState.state, State.CONNECTED, 'controller in connected state');
  Assert.equal(receiverState.state, State.CONNECTED, 'receiver in connected state');

  const testOffer = 'test-offer';
  const testAnswer = 'test-answer';
  const testIceCandidate = 'test-ice-candidate';
  controllerState.sendOffer(testOffer);
  mockReceiverChannel.notifyOffer = function(offer) {
    Assert.equal(offer, testOffer, 'expected offer received');

    receiverState.sendAnswer(testAnswer);
    mockControllerChannel.notifyAnswer = function(answer) {
      Assert.equal(answer, testAnswer, 'expected answer received');

      controllerState.updateIceCandidate(testIceCandidate);
      mockReceiverChannel.notifyIceCandidate = function(candidate) {
        Assert.equal(candidate, testIceCandidate, 'expected ice candidate received in receiver');

        receiverState.updateIceCandidate(testIceCandidate);
        mockControllerChannel.notifyIceCandidate = function(candidate) {
          Assert.equal(candidate, testIceCandidate, 'expected ice candidate received in controller');

          run_next_test();
        };
      };
    };
  };
}

function disconnect() {
  // step 1: controller send disconnect command
  controllerState.onChannelClosed(Cr.NS_OK, false);
  Assert.equal(controllerState.state, State.CLOSING, 'controller in closing state');

  mockReceiverChannel.notifyDisconnected = function(reason) {
    Assert.equal(reason, Cr.NS_OK, 'receive close reason');
    Assert.equal(receiverState.state, State.CLOSED, 'receiver in closed state');

    receiverState.onChannelClosed(Cr.NS_OK, true);
    Assert.equal(receiverState.state, State.CLOSED, 'receiver in closed state');

    mockControllerChannel.notifyDisconnected = function(reason) {
      Assert.equal(reason, Cr.NS_OK, 'receive close reason');
      Assert.equal(controllerState.state, State.CLOSED, 'controller in closed state');

      run_next_test();
    };
    controllerState.onChannelClosed(Cr.NS_OK, true);
  };
}

function receiverDisconnect() {
  // initial state: controller and receiver are connected
  controllerState.state = State.CONNECTED;
  receiverState.state = State.CONNECTED;

  // step 1: controller send disconnect command
  receiverState.onChannelClosed(Cr.NS_OK, false);
  Assert.equal(receiverState.state, State.CLOSING, 'receiver in closing state');

  mockControllerChannel.notifyDisconnected = function(reason) {
    Assert.equal(reason, Cr.NS_OK, 'receive close reason');
    Assert.equal(controllerState.state, State.CLOSED, 'controller in closed state');

    controllerState.onChannelClosed(Cr.NS_OK, true);
    Assert.equal(controllerState.state, State.CLOSED, 'controller in closed state');

    mockReceiverChannel.notifyDisconnected = function(reason) {
      Assert.equal(reason, Cr.NS_OK, 'receive close reason');
      Assert.equal(receiverState.state, State.CLOSED, 'receiver in closed state');

      run_next_test();
    };
    receiverState.onChannelClosed(Cr.NS_OK, true);
  };
}

function abnormalDisconnect() {
  // initial state: controller and receiver are connected
  controllerState.state = State.CONNECTED;
  receiverState.state = State.CONNECTED;

  const testErrorReason = Cr.NS_ERROR_FAILURE;
  // step 1: controller send disconnect command
  controllerState.onChannelClosed(testErrorReason, false);
  Assert.equal(controllerState.state, State.CLOSING, 'controller in closing state');

  mockReceiverChannel.notifyDisconnected = function(reason) {
    Assert.equal(reason, testErrorReason, 'receive abnormal close reason');
    Assert.equal(receiverState.state, State.CLOSED, 'receiver in closed state');

    receiverState.onChannelClosed(Cr.NS_OK, true);
    Assert.equal(receiverState.state, State.CLOSED, 'receiver in closed state');

    mockControllerChannel.notifyDisconnected = function(reason) {
      Assert.equal(reason, testErrorReason, 'receive abnormal close reason');
      Assert.equal(controllerState.state, State.CLOSED, 'controller in closed state');

      run_next_test();
    };
    controllerState.onChannelClosed(Cr.NS_OK, true);
  };
}

add_test(connect);
add_test(launch);
add_test(terminateByController);
add_test(terminateByReceiver);
add_test(exchangeSDP);
add_test(disconnect);
add_test(receiverDisconnect);
add_test(abnormalDisconnect);

function run_test() { // jshint ignore:line
  run_next_test();
}
