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
  MESSAGE_ADD,
  NETWORK_MESSAGE_UPDATE,
  MESSAGES_CLEAR,
  MESSAGE_OPEN,
  MESSAGE_CLOSE,
  MESSAGE_TYPE,
  MESSAGE_TABLE_RECEIVE,
  MESSAGE_OBJECT_PROPERTIES_RECEIVE,
  MESSAGE_OBJECT_ENTRIES_RECEIVE,
} = require("../constants");

const defaultIdGenerator = new IdGenerator();

function messageAdd(packet, idGenerator = null) {
  if (idGenerator == null) {
    idGenerator = defaultIdGenerator;
  }
  let message = prepareMessage(packet, idGenerator);
  const addMessageAction = {
    type: MESSAGE_ADD,
    message
  };

  if (message.type === MESSAGE_TYPE.CLEAR) {
    return batchActions([
      messagesClear(),
      addMessageAction,
    ]);
  }
  return addMessageAction;
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

function networkMessageUpdate(packet, idGenerator = null) {
  if (idGenerator == null) {
    idGenerator = defaultIdGenerator;
  }

  let message = prepareMessage(packet, idGenerator);

  return {
    type: NETWORK_MESSAGE_UPDATE,
    message,
  };
}

/**
 * This action is used to load the properties of a grip passed as an argument,
 * for a given message. The action then dispatch the messageObjectPropertiesReceive
 * action with the loaded properties.
 * This action is mainly called by the ObjectInspector component when the user expands
 *  an object.
 *
 * @param {string} id - The message id the grip is in.
 * @param {ObjectClient} client - The ObjectClient built for the grip.
 * @param {object} grip - The grip to load properties from.
 * @returns {async function} - A function that retrieves the properties
 *          and dispatch the messageObjectPropertiesReceive action.
 */
function messageObjectPropertiesLoad(id, client, grip) {
  return async (dispatch) => {
    const response = await client.getPrototypeAndProperties();
    dispatch(messageObjectPropertiesReceive(id, grip.actor, response));
  };
}

function messageObjectEntriesLoad(id, client, grip) {
  return (dispatch) => {
    client.enumEntries(enumResponse => {
      const {iterator} = enumResponse;
      iterator.slice(0, iterator.count, sliceResponse => {
        dispatch(messageObjectEntriesReceive(id, grip.actor, sliceResponse));
      });
    });
  };
}

/**
 * This action is dispatched when properties of a grip are loaded.
 *
 * @param {string} id - The message id the grip is in.
 * @param {string} actor - The actor id of the grip the properties were loaded from.
 * @param {object} properties - A RDP packet that contains the properties of the grip.
 * @returns {object}
 */
function messageObjectPropertiesReceive(id, actor, properties) {
  return {
    type: MESSAGE_OBJECT_PROPERTIES_RECEIVE,
    id,
    actor,
    properties
  };
}

/**
 * This action is dispatched when entries of a grip are loaded.
 *
 * @param {string} id - The message id the grip is in.
 * @param {string} actor - The actor id of the grip the properties were loaded from.
 * @param {object} entries - A RDP packet that contains the entries of the grip.
 * @returns {object}
 */
function messageObjectEntriesReceive(id, actor, entries) {
  return {
    type: MESSAGE_OBJECT_ENTRIES_RECEIVE,
    id,
    actor,
    entries
  };
}

module.exports = {
  messageAdd,
  messagesClear,
  messageOpen,
  messageClose,
  messageTableDataGet,
  networkMessageUpdate,
  messageObjectPropertiesLoad,
  messageObjectEntriesLoad,
  // for test purpose only.
  messageTableDataReceive,
  messageObjectPropertiesReceive,
  messageObjectEntriesReceive,
};

