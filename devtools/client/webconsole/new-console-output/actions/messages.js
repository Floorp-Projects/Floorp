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
const { batchActions } = require("devtools/client/shared/redux/middleware/debounce");

const {
  MESSAGES_ADD,
  NETWORK_MESSAGE_UPDATE,
  NETWORK_UPDATE_REQUEST,
  MESSAGES_CLEAR,
  MESSAGE_OPEN,
  MESSAGE_CLOSE,
  MESSAGE_TYPE,
  MESSAGE_TABLE_RECEIVE,
} = require("../constants");

const defaultIdGenerator = new IdGenerator();

function messagesAdd(packets, idGenerator = null) {
  if (idGenerator == null) {
    idGenerator = defaultIdGenerator;
  }
  let messages = packets.map(packet => prepareMessage(packet, idGenerator));
  for (let i = messages.length - 1; i >= 0; i--) {
    if (messages[i].type === MESSAGE_TYPE.CLEAR) {
      return batchActions([
        messagesClear(),
        {
          type: MESSAGES_ADD,
          messages: messages.slice(i),
        }
      ]);
    }
  }

  // When this is used for non-cached messages then handle clear message and
  // split up into batches
  return {
    type: MESSAGES_ADD,
    messages
  };
}

function messagesClear() {
  return {
    type: MESSAGES_CLEAR
  };
}

function messageOpen(id) {
  return {
    type: MESSAGE_OPEN,
    id
  };
}

function messageClose(id) {
  return {
    type: MESSAGE_CLOSE,
    id
  };
}

function messageTableDataGet(id, client, dataType) {
  return (dispatch) => {
    let fetchObjectActorData;
    if (["Map", "WeakMap", "Set", "WeakSet"].includes(dataType)) {
      fetchObjectActorData = (cb) => client.enumEntries(cb);
    } else {
      fetchObjectActorData = (cb) => client.enumProperties({
        ignoreNonIndexedProperties: dataType === "Array"
      }, cb);
    }

    fetchObjectActorData(enumResponse => {
      const {iterator} = enumResponse;
      iterator.slice(0, iterator.count, sliceResponse => {
        let {ownProperties} = sliceResponse;
        dispatch(messageTableDataReceive(id, ownProperties));
      });
    });
  };
}

function messageTableDataReceive(id, data) {
  return {
    type: MESSAGE_TABLE_RECEIVE,
    id,
    data
  };
}

function networkMessageUpdate(packet, idGenerator = null, response) {
  if (idGenerator == null) {
    idGenerator = defaultIdGenerator;
  }

  let message = prepareMessage(packet, idGenerator);

  return {
    type: NETWORK_MESSAGE_UPDATE,
    message,
    response,
  };
}

function networkUpdateRequest(id, data) {
  return {
    type: NETWORK_UPDATE_REQUEST,
    id,
    data,
  };
}

module.exports = {
  messagesAdd,
  messagesClear,
  messageOpen,
  messageClose,
  messageTableDataGet,
  networkMessageUpdate,
  networkUpdateRequest,
  // for test purpose only.
  messageTableDataReceive,
};
