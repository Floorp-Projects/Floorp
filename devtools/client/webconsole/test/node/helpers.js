/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const reduxActions = require("resource://devtools/client/webconsole/actions/index.js");
const {
  configureStore,
} = require("resource://devtools/client/webconsole/store.js");
const {
  IdGenerator,
} = require("resource://devtools/client/webconsole/utils/id-generator.js");
const {
  stubPackets,
} = require("resource://devtools/client/webconsole/test/node/fixtures/stubs/index.js");
const {
  getMutableMessagesById,
} = require("resource://devtools/client/webconsole/selectors/messages.js");
const {
  getPrefsService,
} = require("resource://devtools/client/webconsole/utils/prefs.js");
const prefsService = getPrefsService({});
const { PREFS } = require("resource://devtools/client/webconsole/constants.js");
const Telemetry = require("resource://devtools/client/shared/test-helpers/jest-fixtures/telemetry.js");
const {
  getSerializedPacket,
  parsePacketAndCreateFronts,
} = require("resource://devtools/client/webconsole/test/browser/stub-generator-helpers.js");

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
    thunkArgs: { toolbox: {} },
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
    toolbox: {},
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
