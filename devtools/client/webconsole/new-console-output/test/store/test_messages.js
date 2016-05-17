/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const actions = require("devtools/client/webconsole/new-console-output/actions/messages");
const packet = testPackets.get("console.log");
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

  deepEqual(getAllMessages(getState()), [expectedMessage],
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

  const expectedMessage = prepareMessage(packet);
  expectedMessage.repeat = 3;

  deepEqual(getAllMessages(getState()), [expectedMessage],
    "Adding same message to the store twice results in repeated message");
});

/**
 * Test getRepeatId().
 */
add_task(function* () {
  const message1 = prepareMessage(packet);
  const message2 = prepareMessage(packet);
  equal(getRepeatId(message1), getRepeatId(message2),
    "getRepeatId() returns same repeat id for objects with the same values");

  message2.data.arguments = ["new args"];
  notEqual(getRepeatId(message1), getRepeatId(message2),
    "getRepeatId() returns different repeat ids for different values");
});
