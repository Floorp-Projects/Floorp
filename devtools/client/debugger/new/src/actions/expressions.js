"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.addExpression = addExpression;
exports.autocomplete = autocomplete;
exports.clearAutocomplete = clearAutocomplete;
exports.clearExpressionError = clearExpressionError;
exports.updateExpression = updateExpression;
exports.deleteExpression = deleteExpression;
exports.evaluateExpressions = evaluateExpressions;
exports.getMappedExpression = getMappedExpression;

var _selectors = require("../selectors/index");

var _promise = require("./utils/middleware/promise");

var _devtoolsSourceMap = require("devtools/client/shared/source-map/index.js");

var _expressions = require("../utils/expressions");

var _parser = require("../workers/parser/index");

var parser = _interopRequireWildcard(_parser);

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Add expression for debugger to watch
 *
 * @param {object} expression
 * @param {number} expression.id
 * @memberof actions/pause
 * @static
 */
function addExpression(input) {
  return async ({
    dispatch,
    getState
  }) => {
    if (!input) {
      return;
    }

    const expressionError = await parser.hasSyntaxError(input);
    const expression = (0, _selectors.getExpression)(getState(), input);

    if (expression) {
      return dispatch(evaluateExpression(expression));
    }

    dispatch({
      type: "ADD_EXPRESSION",
      input,
      expressionError
    });
    const newExpression = (0, _selectors.getExpression)(getState(), input);

    if (newExpression) {
      return dispatch(evaluateExpression(newExpression));
    }
  };
}

function autocomplete(input, cursor) {
  return async ({
    dispatch,
    getState,
    client
  }) => {
    if (!input) {
      return;
    }

    const frameId = (0, _selectors.getSelectedFrameId)(getState());
    const result = await client.autocomplete(input, cursor, frameId);
    await dispatch({
      type: "AUTOCOMPLETE",
      input,
      result
    });
  };
}

function clearAutocomplete() {
  return {
    type: "CLEAR_AUTOCOMPLETE"
  };
}

function clearExpressionError() {
  return {
    type: "CLEAR_EXPRESSION_ERROR"
  };
}

function updateExpression(input, expression) {
  return async ({
    dispatch,
    getState
  }) => {
    if (!input) {
      return;
    }

    const expressionError = await parser.hasSyntaxError(input);
    dispatch({
      type: "UPDATE_EXPRESSION",
      expression,
      input: expressionError ? expression.input : input,
      expressionError
    });
    dispatch(evaluateExpressions());
  };
}
/**
 *
 * @param {object} expression
 * @param {number} expression.id
 * @memberof actions/pause
 * @static
 */


function deleteExpression(expression) {
  return ({
    dispatch
  }) => {
    dispatch({
      type: "DELETE_EXPRESSION",
      input: expression.input
    });
  };
}
/**
 *
 * @memberof actions/pause
 * @param {number} selectedFrameId
 * @static
 */


function evaluateExpressions() {
  return async function ({
    dispatch,
    getState,
    client
  }) {
    const expressions = (0, _selectors.getExpressions)(getState()).toJS();
    const inputs = expressions.map(({
      input
    }) => input);
    const frameId = (0, _selectors.getSelectedFrameId)(getState());
    const results = await client.evaluateExpressions(inputs, frameId);
    dispatch({
      type: "EVALUATE_EXPRESSIONS",
      inputs,
      results
    });
  };
}

function evaluateExpression(expression) {
  return async function ({
    dispatch,
    getState,
    client,
    sourceMaps
  }) {
    if (!expression.input) {
      console.warn("Expressions should not be empty");
      return;
    }

    let input = expression.input;
    const frame = (0, _selectors.getSelectedFrame)(getState());

    if (frame) {
      const {
        location
      } = frame;
      const source = (0, _selectors.getSourceFromId)(getState(), location.sourceId);
      const sourceId = source.id;
      const selectedSource = (0, _selectors.getSelectedSource)(getState());

      if (selectedSource && !(0, _devtoolsSourceMap.isGeneratedId)(sourceId) && !(0, _devtoolsSourceMap.isGeneratedId)(selectedSource.id)) {
        input = await dispatch(getMappedExpression(input));
      }
    }

    const frameId = (0, _selectors.getSelectedFrameId)(getState());
    return dispatch({
      type: "EVALUATE_EXPRESSION",
      input: expression.input,
      [_promise.PROMISE]: client.evaluateInFrame((0, _expressions.wrapExpression)(input), frameId)
    });
  };
}
/**
 * Gets information about original variable names from the source map
 * and replaces all posible generated names.
 */


function getMappedExpression(expression) {
  return async function ({
    dispatch,
    getState,
    client,
    sourceMaps
  }) {
    const mappings = (0, _selectors.getSelectedScopeMappings)(getState());

    if (!mappings) {
      return expression;
    }

    return parser.mapOriginalExpression(expression, mappings);
  };
}