/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const actions = require("devtools/client/webconsole/new-console-output/actions/messages");
const packet = testPackets.get("console.log");
const clearPacket = testPackets.get("console.clear");
const {
  getRepeatId,
  prepareMessage
} = require("devtools/client/webconsole/new-console-output/utils/messages");
const { getAllMessages } = require("devtools/client/webconsole/new-console-output/selectors/messages");

function run_test() {
  run_next_test();
}

/**
 * Test adding a message to the store.
 */
add_task(function* () {
  const { getState, dispatch } = storeFactory();

  dispatch(actions.messageAdd(packet));

  const expectedMessage = prepareMessage(packet);

  const messages = getAllMessages(getState());
  equal(messages.size, 1, "We added exactly one message.")

  const message = messages.first();
  notEqual(message.id, expectedMessage.id, "ID should be unique.");
  // Remove ID for deepEqual comparison.
  deepEqual(message.remove('id'), expectedMessage.remove('id'),
      "MESSAGE_ADD action adds a message");
});

/**
 * Test repeating messages in the store.
 */
add_task(function* () {
  const { getState, dispatch } = storeFactory();

  dispatch(actions.messageAdd(packet));
  dispatch(actions.messageAdd(packet));
  dispatch(actions.messageAdd(packet));

  const messages = getAllMessages(getState());
  equal(messages.size, 1,
    "Repeated messages don't increase message list size");
  equal(messages.first().repeat, 3, "Repeated message is updated as expected");

  let newPacket = Object.assign({}, packet);
  newPacket.message.arguments = ["funny"];
  dispatch(actions.messageAdd(newPacket));

  equal(getAllMessages(getState()).size, 2,
    "Non-repeated messages aren't clobbered");
});

/**
 * Test getRepeatId().
 */
add_task(function* () {
  const message1 = prepareMessage(packet);
  let message2 = prepareMessage(packet);
  equal(getRepeatId(message1), getRepeatId(message2),
    "getRepeatId() returns same repeat id for objects with the same values");

  message2 = message2.set("parameters", ["new args"]);
  notEqual(getRepeatId(message1), getRepeatId(message2),
    "getRepeatId() returns different repeat ids for different values");
});

/**
 * Test adding a console.clear message to the store.
 */
add_task(function*() {
  const { getState, dispatch } = storeFactory();

  dispatch(actions.messageAdd(packet));

  let messages = getAllMessages(getState());
  equal(messages.size, 1,
    "MESSAGE_ADD action adds a message");

  dispatch(actions.messageAdd(clearPacket));

  messages = getAllMessages(getState());
  deepEqual(messages.first().remove('id'), prepareMessage(clearPacket).remove('id'),
    "console.clear clears existing messages and add a new one");
});

/**
 * Test message limit on the store.
 */
add_task(function* () {
  const { getState, dispatch } = storeFactory();
  const logLimit = 1000;
  const messageNumber = logLimit + 1;

  let newPacket = Object.assign({}, packet);
  for (let i = 1; i <= messageNumber; i++) {
    newPacket.message.arguments = [i];
    dispatch(actions.messageAdd(newPacket));
  }

  let messages = getAllMessages(getState());
  equal(messages.count(), logLimit, "Messages are pruned up to the log limit");
  deepEqual(messages.last().parameters, [messageNumber],
    "The last message is the expected one");
});

/**
 * Test message limit on the store with user set prefs.
 */
add_task(function* () {
  const userSetLimit = 10;
  Services.prefs.setIntPref("devtools.hud.loglimit", userSetLimit);

  const { getState, dispatch } = storeFactory();

  let newPacket = Object.assign({}, packet);
  for (let i = 1; i <= userSetLimit + 1; i++) {
    newPacket.message.parameters = [i];
    dispatch(actions.messageAdd(newPacket));
  }

  let messages = getAllMessages(getState());
  equal(messages.count(), userSetLimit,
    "Messages are pruned up to the user set log limit");
});
