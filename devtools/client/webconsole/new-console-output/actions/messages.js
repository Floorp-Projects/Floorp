/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  prepareMessage
} = require("devtools/client/webconsole/new-console-output/utils/messages");
const { IdGenerator } = require("devtools/client/webconsole/new-console-output/utils/id-generator");

const {
  MESSAGE_ADD,
  MESSAGES_CLEAR
} = require("../constants");

const defaultIdGenerator = new IdGenerator();

function messageAdd(packet, idGenerator = null) {
  if (idGenerator == null) {
    idGenerator = defaultIdGenerator;
  }
  let message = prepareMessage(packet, idGenerator);

  return {
    type: MESSAGE_ADD,
    message
  };
}

function messagesClear() {
  return {
    type: MESSAGES_CLEAR
  };
}

exports.messageAdd = messageAdd;
exports.messagesClear = messagesClear;
