/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  prepareMessage,
} = require("devtools/client/webconsole/utils/messages");
const { IdGenerator } = require("devtools/client/webconsole/utils/id-generator");
const { batchActions } = require("devtools/client/shared/redux/middleware/debounce");

const {
  MESSAGES_ADD,
  NETWORK_MESSAGE_UPDATE,
  NETWORK_UPDATE_REQUEST,
  MESSAGES_CLEAR,
  MESSAGES_CLEAR_LOGPOINT,
  MESSAGE_OPEN,
  MESSAGE_CLOSE,
  MESSAGE_TYPE,
  MESSAGE_UPDATE_PAYLOAD,
  MESSAGE_TABLE_RECEIVE,
  PAUSED_EXCECUTION_POINT,
  PRIVATE_MESSAGES_CLEAR,
} = require("../constants");

const defaultIdGenerator = new IdGenerator();

function messagesAdd(packets, idGenerator = null) {
  if (idGenerator == null) {
    idGenerator = defaultIdGenerator;
  }
  const messages = packets.map(packet => prepareMessage(packet, idGenerator));
  for (let i = messages.length - 1; i >= 0; i--) {
    if (messages[i].type === MESSAGE_TYPE.CLEAR) {
      return batchActions([
        messagesClear(),
        {
          type: MESSAGES_ADD,
          messages: messages.slice(i),
        },
      ]);
    }
  }

  // When this is used for non-cached messages then handle clear message and
  // split up into batches
  return {
    type: MESSAGES_ADD,
    messages,
  };
}

function messagesClear() {
  return {
    type: MESSAGES_CLEAR,
  };
}

function messagesClearLogpoint(logpointId) {
  return {
    type: MESSAGES_CLEAR_LOGPOINT,
    logpointId,
  };
}

function setPauseExecutionPoint(executionPoint) {
  return {
    type: PAUSED_EXCECUTION_POINT,
    executionPoint,
  };
}

function privateMessagesClear() {
  return {
    type: PRIVATE_MESSAGES_CLEAR,
  };
}

function messageOpen(id) {
  return {
    type: MESSAGE_OPEN,
    id,
  };
}

function messageClose(id) {
  return {
    type: MESSAGE_CLOSE,
    id,
  };
}

/**
 * Make a query on the server to get a list of DOM elements matching the given
 * CSS selectors and set the result as a message's additional data payload.
 *
 * @param {String} id
 *        Message ID
 * @param {String} cssSelectors
 *        CSS selectors string to use in the querySelectorAll() call
 * @return {[type]} [description]
 */
function messageGetMatchingElements(id, cssSelectors) {
  return ({ dispatch, services }) => {
    services
      .requestEvaluation(`document.querySelectorAll('${cssSelectors}')`)
      .then(response => {
        dispatch(messageUpdatePayload(id, response.result));
      })
      .catch(err => {
        console.error(err);
      });
  };
}

function messageTableDataGet(id, client, dataType) {
  return ({dispatch}) => {
    let fetchObjectActorData;
    if (["Map", "WeakMap", "Set", "WeakSet"].includes(dataType)) {
      fetchObjectActorData = (cb) => client.enumEntries(cb);
    } else {
      fetchObjectActorData = (cb) => client.enumProperties({
        ignoreNonIndexedProperties: dataType === "Array",
      }, cb);
    }

    fetchObjectActorData(enumResponse => {
      const {iterator} = enumResponse;
      // eslint-disable-next-line mozilla/use-returnValue
      iterator.slice(0, iterator.count, sliceResponse => {
        const {ownProperties} = sliceResponse;
        dispatch(messageTableDataReceive(id, ownProperties));
      });
    });
  };
}

function messageTableDataReceive(id, data) {
  return {
    type: MESSAGE_TABLE_RECEIVE,
    id,
    data,
  };
}

/**
 * Associate additional data with a message without mutating the original message object.
 *
 * @param {String} id
 *        Message ID
 * @param {Object} data
 *        Object with arbitrary data.
 */
function messageUpdatePayload(id, data) {
  return {
    type: MESSAGE_UPDATE_PAYLOAD,
    id,
    data,
  };
}

function networkMessageUpdate(packet, idGenerator = null, response) {
  if (idGenerator == null) {
    idGenerator = defaultIdGenerator;
  }

  const message = prepareMessage(packet, idGenerator);

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
  messagesClearLogpoint,
  messageOpen,
  messageClose,
  messageGetMatchingElements,
  messageTableDataGet,
  messageUpdatePayload,
  networkMessageUpdate,
  networkUpdateRequest,
  privateMessagesClear,
  // for test purpose only.
  messageTableDataReceive,
  setPauseExecutionPoint,
};
