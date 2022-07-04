/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const reduxActions = require("devtools/client/webconsole/actions/index");
const { configureStore } = require("devtools/client/webconsole/store");
const {
  IdGenerator,
} = require("devtools/client/webconsole/utils/id-generator");
const {
  stubPackets,
} = require("devtools/client/webconsole/test/node/fixtures/stubs/index");
const {
  getMutableMessagesById,
} = require("devtools/client/webconsole/selectors/messages");
const { getPrefsService } = require("devtools/client/webconsole/utils/prefs");
const prefsService = getPrefsService({});
const { PREFS } = require("devtools/client/webconsole/constants");
const Telemetry = require("devtools/client/shared/telemetry");
const {
  getSerializedPacket,
  parsePacketAndCreateFronts,
} = require("devtools/client/webconsole/test/browser/stub-generator-helpers");

/**
 * Prepare actions for use in testing.
 */
function setupActions() {
  // Some actions use dependency injection. This helps them avoid using state in
  // a hard-to-test way. We need to inject stubbed versions of these dependencies.
  const wrappedActions = Object.assign({}, reduxActions);

  const idGenerator = new IdGenerator();
  wrappedActions.messagesAdd = packets => {
    return reduxActions.messagesAdd(packets, idGenerator);
  };

  return {
    ...reduxActions,
    messagesAdd: packets => reduxActions.messagesAdd(packets, idGenerator),
  };
}

/**
 * Prepare the store for use in testing.
 */
function setupStore(
  input = [],
  { storeOptions = {}, actions, webConsoleUI } = {}
) {
  if (!webConsoleUI) {
    webConsoleUI = getWebConsoleUiMock();
  }
  const store = configureStore(webConsoleUI, {
    ...storeOptions,
    thunkArgs: { toolbox: { sessionId: -1 } },
    telemetry: new Telemetry(),
  });

  // Add the messages from the input commands to the store.
  const messagesAdd = actions ? actions.messagesAdd : reduxActions.messagesAdd;
  store.dispatch(messagesAdd(input.map(cmd => stubPackets.get(cmd))));

  return store;
}

/**
 * Create deep copy of given packet object.
 */
function clonePacket(packet) {
  const strPacket = getSerializedPacket(packet);
  return parsePacketAndCreateFronts(JSON.parse(strPacket));
}

/**
 * Return the message in the store at the given index.
 *
 * @param {object} state - The redux state of the console.
 * @param {int} index - The index of the message in the map.
 * @return {Message} - The message, or undefined if the index does not exists in the map.
 */
function getMessageAt(state, index) {
  const messageMap = getMutableMessagesById(state);
  return messageMap.get(state.messages.mutableMessagesOrder[index]);
}

/**
 * Return the first message in the store.
 *
 * @param {object} state - The redux state of the console.
 * @return {Message} - The last message, or undefined if there are no message in store.
 */
function getFirstMessage(state) {
  return getMessageAt(state, 0);
}

/**
 * Return the last message in the store.
 *
 * @param {object} state - The redux state of the console.
 * @return {Message} - The last message, or undefined if there are no message in store.
 */
function getLastMessage(state) {
  const lastIndex = state.messages.mutableMessagesOrder.length - 1;
  return getMessageAt(state, lastIndex);
}

function getFiltersPrefs() {
  return Object.values(PREFS.FILTER).reduce((res, pref) => {
    res[pref] = prefsService.getBoolPref(pref);
    return res;
  }, {});
}

function clearPrefs() {
  [
    "devtools.hud.loglimit",
    ...Object.values(PREFS.FILTER),
    ...Object.values(PREFS.UI),
  ].forEach(prefsService.clearUserPref);
}

function getPrivatePacket(key) {
  const packet = clonePacket(stubPackets.get(key));
  if (packet.message) {
    packet.message.private = true;
  } else if (packet.pageError) {
    packet.pageError.private = true;
  }
  if (Object.getOwnPropertyNames(packet).includes("private")) {
    packet.private = true;
  }
  return packet;
}

function getWebConsoleUiMock(hud) {
  return {
    emit: () => {},
    emitForTests: () => {},
    hud,
    clearNetworkRequests: () => {},
    clearMessagesCache: () => {},
    inspectObjectActor: () => {},
    toolbox: {
      sessionId: 1,
    },
    watchCssMessages: () => {},
  };
}

function formatErrorTextWithCausedBy(text) {
  // The component text does not append new line character before
  // the "Caused by" label, so add it here to make the assertions
  // look more legible
  return text.replace(/Caused by/g, "\nCaused by");
}

module.exports = {
  clearPrefs,
  clonePacket,
  formatErrorTextWithCausedBy,
  getFiltersPrefs,
  getFirstMessage,
  getLastMessage,
  getMessageAt,
  getPrivatePacket,
  getWebConsoleUiMock,
  prefsService,
  setupActions,
  setupStore,
};
