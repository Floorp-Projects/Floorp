/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* jshint esnext:true, globalstrict:true, moz:true, undef:true, unused:true */
/* globals Components, dump */

"use strict";

this.EXPORTED_SYMBOLS = ["ReceiverStateMachine"]; // jshint ignore:line

const { utils: Cu } = Components;

/* globals State, CommandType */
Cu.import("resource://gre/modules/presentation/StateMachineHelper.jsm");

const DEBUG = false;
function debug(str) {
  dump("-*- ReceiverStateMachine: " + str + "\n");
}

var handlers = [
  function _initHandler(stateMachine, command) {
    // shouldn't receive any command at init state
    DEBUG && debug("unexpected command: " + JSON.stringify(command)); // jshint ignore:line
  },
  function _connectingHandler(stateMachine, command) {
    switch (command.type) {
      case CommandType.CONNECT:
        stateMachine._sendCommand({
          type: CommandType.CONNECT_ACK
        });
        stateMachine.state = State.CONNECTED;
        stateMachine._notifyDeviceConnected(command.deviceId);
        break;
      case CommandType.DISCONNECT:
        stateMachine.state = State.CLOSED;
        stateMachine._notifyDisconnected(command.reason);
        break;
      default:
        debug("unexpected command: " + JSON.stringify(command));
        // ignore unexpected command
        break;
    }
  },
  function _connectedHandler(stateMachine, command) {
    switch (command.type) {
      case CommandType.DISCONNECT:
        stateMachine.state = State.CLOSED;
        stateMachine._notifyDisconnected(command.reason);
        break;
      case CommandType.LAUNCH:
        stateMachine._notifyLaunch(command.presentationId,
                                   command.url);
        stateMachine._sendCommand({
          type: CommandType.LAUNCH_ACK,
          presentationId: command.presentationId
        });
        break;
      case CommandType.TERMINATE:
        stateMachine._notifyTerminate(command.presentationId);
        break;
      case CommandType.TERMINATE_ACK:
        stateMachine._notifyTerminate(command.presentationId);
        break;
      case CommandType.OFFER:
      case CommandType.ICE_CANDIDATE:
        stateMachine._notifyChannelDescriptor(command);
        break;
      case CommandType.RECONNECT:
        stateMachine._notifyReconnect(command.presentationId,
                                      command.url);
        stateMachine._sendCommand({
          type: CommandType.RECONNECT_ACK,
          presentationId: command.presentationId
        });
        break;
      default:
        debug("unexpected command: " + JSON.stringify(command));
        // ignore unexpected command
        break;
    }
  },
  function _closingHandler(stateMachine, command) {
    switch (command.type) {
      case CommandType.DISCONNECT:
        stateMachine.state = State.CLOSED;
        stateMachine._notifyDisconnected(command.reason);
        break;
      default:
        debug("unexpected command: " + JSON.stringify(command));
        // ignore unexpected command
        break;
    }
  },
  function _closedHandler(stateMachine, command) {
    // ignore every command in closed state.
    DEBUG && debug("unexpected command: " + JSON.stringify(command)); // jshint ignore:line
  },
];

function ReceiverStateMachine(channel) {
  this.state = State.INIT;
  this._channel = channel;
}

ReceiverStateMachine.prototype = {
  launch: function _launch() {
    // presentation session can only be launched by controlling UA.
    debug("receiver shouldn't trigger launch");
  },

  terminate: function _terminate(presentationId) {
    if (this.state === State.CONNECTED) {
      this._sendCommand({
        type: CommandType.TERMINATE,
        presentationId: presentationId,
      });
    }
  },

  terminateAck: function _terminateAck(presentationId) {
    if (this.state === State.CONNECTED) {
      this._sendCommand({
        type: CommandType.TERMINATE_ACK,
        presentationId: presentationId,
      });
    }
  },

  reconnect: function _reconnect() {
    debug("receiver shouldn't trigger reconnect");
  },

  sendOffer: function _sendOffer() {
    // offer can only be sent by controlling UA.
    debug("receiver shouldn't generate offer");
  },

  sendAnswer: function _sendAnswer(answer) {
    if (this.state === State.CONNECTED) {
      this._sendCommand({
        type: CommandType.ANSWER,
        answer: answer,
      });
    }
  },

  updateIceCandidate: function _updateIceCandidate(candidate) {
    if (this.state === State.CONNECTED) {
      this._sendCommand({
        type: CommandType.ICE_CANDIDATE,
        candidate: candidate,
      });
    }
  },

  onCommand: function _onCommand(command) {
    handlers[this.state](this, command);
  },

  onChannelReady: function _onChannelReady() {
    if (this.state === State.INIT) {
      this.state = State.CONNECTING;
    }
  },

  onChannelClosed: function _onChannelClose(reason, isByRemote) {
    switch (this.state) {
      case State.CONNECTED:
        if (isByRemote) {
          this.state = State.CLOSED;
          this._notifyDisconnected(reason);
        } else {
          this._sendCommand({
            type: CommandType.DISCONNECT,
            reason: reason
          });
          this.state = State.CLOSING;
          this._closeReason = reason;
        }
        break;
      case State.CLOSING:
        if (isByRemote) {
          this.state = State.CLOSED;
          if (this._closeReason) {
            reason = this._closeReason;
            delete this._closeReason;
          }
          this._notifyDisconnected(reason);
        } else {
          // do nothing and wait for remote channel closed.
        }
        break;
      default:
        DEBUG && debug("unexpected channel close: " + reason + ", " + isByRemote); // jshint ignore:line
        break;
    }
  },

  _sendCommand: function _sendCommand(command) {
    this._channel.sendCommand(command);
  },

  _notifyDeviceConnected: function _notifyDeviceConnected(deviceName) {
    this._channel.notifyDeviceConnected(deviceName);
  },

  _notifyDisconnected: function _notifyDisconnected(reason) {
    this._channel.notifyDisconnected(reason);
  },

  _notifyLaunch: function _notifyLaunch(presentationId, url) {
    this._channel.notifyLaunch(presentationId, url);
  },

  _notifyTerminate: function _notifyTerminate(presentationId) {
    this._channel.notifyTerminate(presentationId);
  },

  _notifyReconnect: function _notifyReconnect(presentationId, url) {
    this._channel.notifyReconnect(presentationId, url);
  },

  _notifyChannelDescriptor: function _notifyChannelDescriptor(command) {
    switch (command.type) {
      case CommandType.OFFER:
        this._channel.notifyOffer(command.offer);
        break;
      case CommandType.ICE_CANDIDATE:
        this._channel.notifyIceCandidate(command.candidate);
        break;
    }
  },
};

this.ReceiverStateMachine = ReceiverStateMachine; // jshint ignore:line
