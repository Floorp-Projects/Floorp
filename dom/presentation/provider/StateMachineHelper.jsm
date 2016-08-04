/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* jshint esnext:true, globalstrict:true, moz:true, undef:true, unused:true */

"use strict";

this.EXPORTED_SYMBOLS = ["State", "CommandType"]; // jshint ignore:line

const State = Object.freeze({
  INIT: 0,
  CONNECTING: 1,
  CONNECTED: 2,
  CLOSING: 3,
  CLOSED: 4,
});

const CommandType = Object.freeze({
  // control channel life cycle
  CONNECT: "connect", // { deviceId: <string> }
  CONNECT_ACK: "connect-ack", // { presentationId: <string> }
  DISCONNECT: "disconnect", // { reason: <int> }
  // presentation session life cycle
  LAUNCH: "launch", // { presentationId: <string>, url: <string> }
  LAUNCH_ACK: "launch-ack", // { presentationId: <string> }
  TERMINATE: "terminate", // { presentationId: <string> }
  TERMINATE_ACK: "terminate-ack", // { presentationId: <string> }
  RECONNECT: "reconnect", // { presentationId: <string> }
  RECONNECT_ACK: "reconnect-ack", // { presentationId: <string> }
  // session transport establishment
  OFFER: "offer", // { offer: <json> }
  ANSWER: "answer", // { answer: <json> }
  ICE_CANDIDATE: "ice-candidate", // { candidate: <string> }
});

this.State = State; // jshint ignore:line
this.CommandType = CommandType; // jshint ignore:line
