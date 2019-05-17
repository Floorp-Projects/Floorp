/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const constants = require("../constants");
const { generateKey } = require("../utils/utils");

function evaluateInput(input) {
  return async function({ dispatch, client }) {
    try {
      const packet = await client.clientCommands.evaluate(input);
      dispatch(addExpression(input, packet));
    } catch (err) {
      console.warn("Error when evaluating expression", err);
    }
  };
}

function addExpression(input, packet) {
  return {
    key: generateKey(),
    type: constants.ADD_EXPRESSION,
    value: {
      input,
      packet,
    },
  };
}

function clearExpressions() {
  return {
    type: constants.CLEAR_EXPRESSIONS,
  };
}

function showResultPacket(key) {
  return {
    key,
    type: constants.SHOW_RESULT_PACKET,
  };
}

function hideResultPacket(key) {
  return {
    key,
    type: constants.HIDE_RESULT_PACKET,
  };
}

function createObjectClient(grip) {
  return function({ dispatch, client }) {
    return client.getObjectClient(grip);
  };
}

function createLongStringClient(grip) {
  return function({ dispatch, client }) {
    return client.getLongStringClient(grip);
  };
}

function releaseActor(actor) {
  return function({ dispatch, client }) {
    client.releaseActor(actor);
  };
}
module.exports = {
  addExpression,
  clearExpressions,
  evaluateInput,
  showResultPacket,
  hideResultPacket,
  createObjectClient,
  createLongStringClient,
  releaseActor,
};
