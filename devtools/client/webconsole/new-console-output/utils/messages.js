/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  LEVELS,
  SEVERITY_CLASS_FRAGMENTS
} = require("../constants");

function prepareMessage(packet) {
  // @TODO turn this into an Immutable Record.
  let allowRepeating;
  let category;
  let data;
  let messageType;
  let repeat;
  let repeatId;
  let severity;

  switch (packet.type) {
    case "consoleAPICall":
      allowRepeating = true;
      category = "console";
      data = Object.assign({}, packet.message);
      messageType = "ConsoleApiCall";
      repeat = 1;
      repeatId = getRepeatId(packet.message);
      severity = SEVERITY_CLASS_FRAGMENTS[LEVELS[packet.message.level]];
      break;
  }

  return {
    allowRepeating,
    category,
    data,
    messageType,
    repeat,
    repeatId,
    severity
  };
}

function getRepeatId(message) {
  let clonedMessage = JSON.parse(JSON.stringify(message));
  delete clonedMessage.timeStamp;
  delete clonedMessage.uniqueID;
  return JSON.stringify(clonedMessage);
}

// Export for use in testing.
exports.prepareMessage = prepareMessage;
exports.getRepeatId = getRepeatId;
