/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  SELECT_REQUEST,
  MSG_ADD,
  MSG_SELECT,
  MSG_OPEN_DETAILS,
  MSG_CLEAR,
  MSG_TOGGLE_FILTER_TYPE,
  MSG_TOGGLE_CONTROL,
  MSG_SET_FILTER_TEXT,
  MSG_TOGGLE_COLUMN,
  MSG_RESET_COLUMNS,
  MSG_CLOSE_CONNECTION,
} = require("devtools/client/netmonitor/src/constants");

/**
 * The default column state for the MessageListItem component.
 */
const defaultColumnsState = {
  data: true,
  size: false,
  opCode: false,
  maskBit: false,
  finBit: false,
  time: true,
};

/**
 * Returns a new object of default cols.
 */
function getMessageDefaultColumnsState() {
  return Object.assign({}, defaultColumnsState);
}

/**
 * This structure stores list of all WebSocket and EventSource messages received
 * from the backend.
 */
function Messages(initialState = {}) {
  return {
    // Map with all requests (key = channelId, value = array of message objects)
    messages: new Map(),
    messageFilterText: "",
    // Default filter type is "all",
    messageFilterType: "all",
    showControlFrames: false,
    selectedMessage: null,
    messageDetailsOpen: false,
    currentChannelId: null,
    closedConnections: new Map(),
    columns: getMessageDefaultColumnsState(),
    ...initialState,
  };
}

/**
 * When a network request is selected,
 * set the current channelId affiliated with the connection.
 */
function setChannelId(state, action) {
  return {
    ...state,
    currentChannelId: action.httpChannelId,
    // Default filter text is empty string for a new connection
    messageFilterText: "",
  };
}

/**
 * Appending new message into the map.
 */
function addMessage(state, action) {
  const { httpChannelId } = action;
  const nextState = { ...state };

  const newMessage = {
    httpChannelId,
    ...action.data,
  };

  nextState.messages = mapSet(
    nextState.messages,
    newMessage.httpChannelId,
    newMessage
  );
  return nextState;
}

/**
 * Select specific message.
 */
function selectMessage(state, action) {
  return {
    ...state,
    selectedMessage: action.message,
    messageDetailsOpen: action.open,
  };
}

/**
 * Shows/Hides the MessagePayload component.
 */
function openMessageDetails(state, action) {
  return {
    ...state,
    messageDetailsOpen: action.open,
  };
}

/**
 * Clear messages of the request from the state.
 */
function clearMessages(state) {
  const nextState = { ...state };
  nextState.messages = new Map(nextState.messages);
  nextState.messages.delete(nextState.currentChannelId);

  return {
    ...Messages(),
    // Preserving the Map objects as they might contain state for other channelIds
    messages: nextState.messages,
    // Preserving the currentChannelId as there would not be another reset of channelId
    currentChannelId: nextState.currentChannelId,
    // Preserving the columns as they are set from pref
    columns: nextState.columns,
    messageFilterType: nextState.messageFilterType,
    messageFilterText: nextState.messageFilterText,
    showControlFrames: nextState.showControlFrames,
  };
}

/**
 * Toggle the message filter type of the connection.
 */
function toggleMessageFilterType(state, action) {
  return {
    ...state,
    messageFilterType: action.filter,
  };
}

/**
 * Toggle control frames for the WebSocket connection.
 */
function toggleControlFrames(state, action) {
  return {
    ...state,
    showControlFrames: !state.showControlFrames,
  };
}

/**
 * Set the filter text of the current channelId.
 */
function setMessageFilterText(state, action) {
  return {
    ...state,
    messageFilterText: action.text,
  };
}

/**
 * Toggle the user specified column view state.
 */
function toggleColumn(state, action) {
  const { column } = action;

  return {
    ...state,
    columns: {
      ...state.columns,
      [column]: !state.columns[column],
    },
  };
}

/**
 * Reset back to default columns view state.
 */
function resetColumns(state) {
  return {
    ...state,
    columns: getMessageDefaultColumnsState(),
  };
}

function closeConnection(state, action) {
  const { httpChannelId, code, reason } = action;
  const nextState = { ...state };

  nextState.closedConnections.set(httpChannelId, {
    code,
    reason,
  });

  return nextState;
}

/**
 * Append new item into existing map and return new map.
 */
function mapSet(map, key, value) {
  const newMap = new Map(map);
  if (newMap.has(key)) {
    const messagesArray = [...newMap.get(key)];
    messagesArray.push(value);
    newMap.set(key, messagesArray);
    return newMap;
  }
  return newMap.set(key, [value]);
}

/**
 * This reducer is responsible for maintaining list of
 * messages within the Network panel.
 */
function messages(state = Messages(), action) {
  switch (action.type) {
    case SELECT_REQUEST:
      return setChannelId(state, action);
    case MSG_ADD:
      return addMessage(state, action);
    case MSG_SELECT:
      return selectMessage(state, action);
    case MSG_OPEN_DETAILS:
      return openMessageDetails(state, action);
    case MSG_CLEAR:
      return clearMessages(state);
    case MSG_TOGGLE_FILTER_TYPE:
      return toggleMessageFilterType(state, action);
    case MSG_TOGGLE_CONTROL:
      return toggleControlFrames(state, action);
    case MSG_SET_FILTER_TEXT:
      return setMessageFilterText(state, action);
    case MSG_TOGGLE_COLUMN:
      return toggleColumn(state, action);
    case MSG_RESET_COLUMNS:
      return resetColumns(state);
    case MSG_CLOSE_CONNECTION:
      return closeConnection(state, action);
    default:
      return state;
  }
}

module.exports = {
  Messages,
  messages,
  getMessageDefaultColumnsState,
};
