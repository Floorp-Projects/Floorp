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
  CSS_MESSAGE_ADD_MATCHING_ELEMENTS,
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
 * CSS selectors and store the information in the state.
 *
 * @param {Message} message
 *        The CSSWarning message
 */
function messageGetMatchingElements(message) {
  return async ({ dispatch, commands }) => {
    try {
      // We need to do the querySelectorAll using the target the message is coming from,
      // as well as with the window the warning message was emitted from.
      const selectedTargetFront = message?.targetFront;

      const response = await commands.scriptCommand.execute(
        `document.querySelectorAll('${message.cssSelectors}')`,
        {
          selectedTargetFront,
          innerWindowID: message.innerWindowID,
        }
      );
      dispatch({
        type: CSS_MESSAGE_ADD_MATCHING_ELEMENTS,
        id: message.id,
        elements: response.result,
      });
    } catch (err) {
      console.error(err);
    }
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
  networkMessageUpdates,
  networkUpdateRequests,
  privateMessagesClear,
};
