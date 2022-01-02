/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  prepareMessage,
  getNaturalOrder,
} = require("devtools/client/webconsole/utils/messages");
const {
  IdGenerator,
} = require("devtools/client/webconsole/utils/id-generator");
const {
  batchActions,
} = require("devtools/client/shared/redux/middleware/debounce");

const {
  MESSAGES_ADD,
  NETWORK_MESSAGES_UPDATE,
  NETWORK_UPDATES_REQUEST,
  MESSAGES_CLEAR,
  MESSAGE_OPEN,
  MESSAGE_CLOSE,
  MESSAGE_TYPE,
  MESSAGE_REMOVE,
  MESSAGE_UPDATE_PAYLOAD,
  PRIVATE_MESSAGES_CLEAR,
} = require("devtools/client/webconsole/constants");

const defaultIdGenerator = new IdGenerator();

function messagesAdd(packets, idGenerator = null) {
  if (idGenerator == null) {
    idGenerator = defaultIdGenerator;
  }
  const messages = packets.map(packet => prepareMessage(packet, idGenerator));
  // Sort the messages by their timestamps.
  messages.sort(getNaturalOrder);
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
  return async ({ dispatch, commands, getState }) => {
    try {
      // We need to do the querySelectorAll using the target the message is coming from,
      // as well as with the window the warning message was emitted from.
      const message = getState().messages.messagesById.get(id);
      const selectedTargetFront = message?.targetFront;

      const response = await commands.scriptCommand.execute(
        `document.querySelectorAll('${cssSelectors}')`,
        {
          selectedTargetFront,
          innerWindowID: message.innerWindowID,
        }
      );
      dispatch(messageUpdatePayload(id, response.result));
    } catch (err) {
      console.error(err);
    }
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

function messageRemove(id) {
  return {
    type: MESSAGE_REMOVE,
    id,
  };
}

function networkMessageUpdates(packets, idGenerator = null) {
  if (idGenerator == null) {
    idGenerator = defaultIdGenerator;
  }

  const messages = packets.map(packet => prepareMessage(packet, idGenerator));

  return {
    type: NETWORK_MESSAGES_UPDATE,
    messages,
  };
}

function networkUpdateRequests(updates) {
  return {
    type: NETWORK_UPDATES_REQUEST,
    updates,
  };
}

module.exports = {
  messagesAdd,
  messagesClear,
  messageOpen,
  messageClose,
  messageRemove,
  messageGetMatchingElements,
  messageUpdatePayload,
  networkMessageUpdates,
  networkUpdateRequests,
  privateMessagesClear,
};
