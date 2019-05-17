/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const constants = require("../constants");
const expressionsActions = require("./expressions");
const { generateKey } = require("../utils/utils");

function addInput(input) {
  return ({ dispatch }) => {
    dispatch(expressionsActions.evaluateInput(input));

    dispatch({
      key: generateKey(),
      type: constants.ADD_INPUT,
      value: input,
    });
  };
}

function changeCurrentInput(input) {
  return {
    type: constants.CHANGE_CURRENT_INPUT,
    value: input,
  };
}

function navigateInputHistory(dir) {
  return {
    type: constants.NAVIGATE_INPUT_HISTORY,
    value: dir,
  };
}

module.exports = {
  addInput,
  changeCurrentInput,
  navigateInputHistory,
};
