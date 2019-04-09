/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const constants = require("../constants");
const Immutable = require("immutable");

const initialState = Immutable.Map();

function update(state = initialState, action) {
  const { type, value, key } = action;

  switch (type) {
    case constants.ADD_EXPRESSION:
      const newState = state.set(key, Immutable.Map(value));
      window.localStorage.setItem(
        constants.LS_EXPRESSIONS_KEY,
        JSON.stringify(newState.toJS())
      );
      return newState;

    case constants.CLEAR_EXPRESSIONS:
      window.localStorage.removeItem(constants.LS_EXPRESSIONS_KEY);
      return state.clear();

    case constants.SHOW_RESULT_PACKET:
      return state.mergeIn([key], { showPacket: true });

    case constants.HIDE_RESULT_PACKET:
      return state.mergeIn([key], { showPacket: false });
  }

  return state;
}

module.exports = update;
