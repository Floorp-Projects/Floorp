/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

module.exports = {
  ADD_EXPRESSION: Symbol("ADD_EXPRESSION"),
  LOAD_PROPERTIES: Symbol("LOAD_PROPERTIES"),
  LOAD_ENTRIES: Symbol("LOAD_ENTRIES"),
  EVALUATE_EXPRESSION: Symbol("EVALUATE_EXPRESSION"),
  CLEAR_EXPRESSIONS: Symbol("CLEAR_EXPRESSIONS"),
  SHOW_RESULT_PACKET: Symbol("SHOW_RESULT_PACKET"),
  HIDE_RESULT_PACKET: Symbol("HIDE_RESULT_PACKET"),
  ADD_INPUT: Symbol("ADD_INPUT"),
  CHANGE_CURRENT_INPUT: Symbol("CHANGE_CURRENT_INPUT"),
  NAVIGATE_INPUT_HISTORY: Symbol("NAVIGATE_INPUT_HISTORY"),
  DIR_FORWARD: Symbol("DIR_FORWARD "),
  DIR_BACKWARD: Symbol("DIR_BACKWARD"),
  LS_EXPRESSIONS_KEY: "LS_EXPRESSIONS_KEY",
};
