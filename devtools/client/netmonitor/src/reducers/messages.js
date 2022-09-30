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
  CHANNEL_TYPE,
  SET_EVENT_STREAM_FLAG,
} = require("resource://devtools/client/netmonitor/src/constants.js");

/**
 * The default column states for the MessageListItem component.
 */
const defaultColumnsState = {
  data: true,
  size: false,
  time: true,
};

const defaultWSColumnsState = {
  ...defaultColumnsState,
  opCode: false,
  maskBit: false,
  finBit: false,
};

const defaultSSEColumnsState = {
  ...defaultColumnsState,
  eventName: false,
  lastEventId: false,
  retry: false,
};

/**
 * Returns a new object of default cols.
 */
function getMessageDefaultColumnsState(channelType) {
  let columnsState = defaultColumnsState;
  const { EVENT_STREAM, WEB_SOCKET } = CHANNEL_TYPE;

  if (channelType === WEB_SOCKET) {
    columnsState = defaultWSColumnsState;
  } else if (channelType === EVENT_STREAM) {
    columnsState = defaultSSEColumnsState;
  }

  return Object.assign({}, columnsState);
}

/**
 * This structure stores list of all WebSocket and EventSource messages received
 * from the backend.
 */
function Messages(initialState = {}) {
  const { EVENT_STREAM, WEB_SOCKET } = CHANNEL_TYPE;

  return {
    // Map with all requests (key = resourceId, value = array of message objects)
    messages: new Map(),
    messageFilterText: "",
    // Default filter type is "all",
    messageFilterType: "all",
    showControlFrames: false,
    selectedMessage: null,
    messageDetailsOpen: false,
    currentChannelId: null,
    currentChannelType: null,
    currentRequestId: null,
    closedConnections: new Map(),
    columns: null,
    sseColumns: getMessageDefaultColumnsState(EVENT_STREAM),
    wsColumns: getMessageDefaultColumnsState(WEB_SOCKET),
    ...initialState,
  };
}

/**
 * When a network request is selected,
 * set the current resourceId affiliated with the connection.
 */
function setCurrentChannel(state, action) {
  if (!action.request) {
    return state;
  }

  const { id, cause, resourceId, isEventStream } = action.request;
  const { EVENT_STREAM, WEB_SOCKET } = CHANNEL_TYPE;
  let currentChannelType = null;
  let columnsKey = "columns";
  if (cause.type === "websocket") {
    currentChannelType = WEB_SOCKET;
    columnsKey = "wsColumns";
  } else if (isEventStream) {
    currentChannelType = EVENT_STREAM;
    columnsKey = "sseColumns";
  }

  return {
    ...state,
    columns:
      currentChannelType === state.currentChannelType
        ? { ...state.columns }
        : { ...state[columnsKey] },
    currentChannelId: resourceId,
    currentChannelType,
    currentRequestId: id,
    // Default filter text is empty string for a new connection
    messageFilterText: "",
  };
}

/**
 * If the request is already selected and isEventStream flag
 * is added later, we need to update currentChannelType & columns.
 */
function updateCurrentChannel(state, action) {
  if (state.currentRequestId === action.id) {
    const currentChannelType = CHANNEL_TYPE.EVENT_STREAM;
    return {
      ...state,
      columns: { ...state.sseColumns },
      currentChannelType,
    };
  }
  return state;
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
  const defaultState = Messages();
  nextState.messages = new Map(state.messages);
  nextState.messages.delete(nextState.currentChannelId);

  // Reset fields to default state.
  nextState.selectedMessage = defaultState.selectedMessage;
  nextState.messageDetailsOpen = defaultState.messageDetailsOpen;

  return nextState;
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
  let columnsKey = null;
  if (state.currentChannelType === CHANNEL_TYPE.WEB_SOCKET) {
    columnsKey = "wsColumns";
  } else {
    columnsKey = "sseColumns";
  }
  const newColumnsState = {
    ...state[columnsKey],
    [column]: !state[columnsKey][column],
  };
  return {
    ...state,
    columns: newColumnsState,
    [columnsKey]: newColumnsState,
  };
}

/**
 * Reset back to default columns view state.
 */
function resetColumns(state) {
  let columnsKey = null;
  if (state.currentChannelType === CHANNEL_TYPE.WEB_SOCKET) {
    columnsKey = "wsColumns";
  } else {
    columnsKey = "sseColumns";
  }
  const newColumnsState = getMessageDefaultColumnsState(
    state.currentChannelType
  );
  return {
    ...state,
    [columnsKey]: newColumnsState,
    columns: newColumnsState,
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
      return setCurrentChannel(state, action);
    case SET_EVENT_STREAM_FLAG:
      return updateCurrentChannel(state, action);
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
