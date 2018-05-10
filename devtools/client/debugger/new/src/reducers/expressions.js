"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getExpressionError = exports.getAutocompleteMatches = exports.getExpressions = exports.createExpressionState = undefined;
exports.getExpression = getExpression;
exports.getAutocompleteMatchset = getAutocompleteMatchset;

var _makeRecord = require("../utils/makeRecord");

var _makeRecord2 = _interopRequireDefault(_makeRecord);

var _immutable = require("devtools/client/shared/vendor/immutable");

var _lodash = require("devtools/client/shared/vendor/lodash");

var _reselect = require("devtools/client/debugger/new/dist/vendors").vendored["reselect"];

var _prefs = require("../utils/prefs");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Expressions reducer
 * @module reducers/expressions
 */
const createExpressionState = exports.createExpressionState = (0, _makeRecord2.default)({
  expressions: (0, _immutable.List)(restoreExpressions()),
  expressionError: false,
  autocompleteMatches: (0, _immutable.Map)({})
});

function update(state = createExpressionState(), action) {
  switch (action.type) {
    case "ADD_EXPRESSION":
      if (action.expressionError) {
        return state.set("expressionError", !!action.expressionError);
      }

      return appendExpressionToList(state, {
        input: action.input,
        value: null,
        updating: true
      });

    case "UPDATE_EXPRESSION":
      const key = action.expression.input;
      return updateExpressionInList(state, key, {
        input: action.input,
        value: null,
        updating: true
      }).set("expressionError", !!action.expressionError);

    case "EVALUATE_EXPRESSION":
      return updateExpressionInList(state, action.input, {
        input: action.input,
        value: action.value,
        updating: false
      });

    case "EVALUATE_EXPRESSIONS":
      const {
        inputs,
        results
      } = action;
      return (0, _lodash.zip)(inputs, results).reduce((newState, [input, result]) => updateExpressionInList(newState, input, {
        input: input,
        value: result,
        updating: false
      }), state);

    case "DELETE_EXPRESSION":
      return deleteExpression(state, action.input);

    case "CLEAR_EXPRESSION_ERROR":
      return state.set("expressionError", false);
    // respond to time travel

    case "TRAVEL_TO":
      return travelTo(state, action);

    case "AUTOCOMPLETE":
      const {
        matchProp,
        matches
      } = action.result;
      return state.updateIn(["autocompleteMatches", matchProp], list => matches);
  }

  return state;
}

function travelTo(state, action) {
  const {
    expressions
  } = action.data;

  if (!expressions) {
    return state;
  }

  return expressions.reduce((finalState, previousState) => updateExpressionInList(finalState, previousState.input, {
    input: previousState.input,
    value: previousState.value,
    updating: false
  }), state);
}

function restoreExpressions() {
  const exprs = _prefs.prefs.expressions;

  if (exprs.length == 0) {
    return;
  }

  return exprs;
}

function storeExpressions({
  expressions
}) {
  _prefs.prefs.expressions = expressions.map(expression => (0, _lodash.omit)(expression, "value")).toJS();
}

function appendExpressionToList(state, value) {
  const newState = state.update("expressions", () => {
    return state.expressions.push(value);
  });
  storeExpressions(newState);
  return newState;
}

function updateExpressionInList(state, key, value) {
  const newState = state.update("expressions", () => {
    const list = state.expressions;
    const index = list.findIndex(e => e.input == key);
    return list.update(index, () => value);
  });
  storeExpressions(newState);
  return newState;
}

function deleteExpression(state, input) {
  const index = getExpressions({
    expressions: state
  }).findIndex(e => e.input == input);
  const newState = state.deleteIn(["expressions", index]);
  storeExpressions(newState);
  return newState;
}

const getExpressionsWrapper = state => state.expressions;

const getExpressions = exports.getExpressions = (0, _reselect.createSelector)(getExpressionsWrapper, expressions => expressions.expressions);
const getAutocompleteMatches = exports.getAutocompleteMatches = (0, _reselect.createSelector)(getExpressionsWrapper, expressions => expressions.autocompleteMatches);

function getExpression(state, input) {
  return getExpressions(state).find(exp => exp.input == input);
}

function getAutocompleteMatchset(state, input) {
  return getAutocompleteMatches(state).get(input);
}

const getExpressionError = exports.getExpressionError = (0, _reselect.createSelector)(getExpressionsWrapper, expressions => expressions.expressionError);
exports.default = update;