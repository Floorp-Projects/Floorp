/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  getExpression,
  getExpressions,
  getSelectedFrame,
  getSelectedFrameId,
  getLocationSource,
  getSelectedSource,
  getSelectedScopeMappings,
  getSelectedFrameBindings,
  getCurrentThread,
  getIsPaused,
  isMapScopesEnabled,
} from "../selectors";
import { PROMISE } from "./utils/middleware/promise";
import { wrapExpression } from "../utils/expressions";
import { features } from "../utils/prefs";

/**
 * Add expression for debugger to watch
 *
 * @param {object} expression
 * @param {number} expression.id
 * @memberof actions/pause
 * @static
 */
export function addExpression(cx, input) {
  return async ({ dispatch, getState, evaluationsParser }) => {
    if (!input) {
      return null;
    }

    const expressionError = await evaluationsParser.hasSyntaxError(input);

    const expression = getExpression(getState(), input);
    if (expression) {
      return dispatch(evaluateExpression(cx, expression));
    }

    dispatch({ type: "ADD_EXPRESSION", cx, input, expressionError });

    const newExpression = getExpression(getState(), input);
    if (newExpression) {
      return dispatch(evaluateExpression(cx, newExpression));
    }

    return null;
  };
}

export function autocomplete(cx, input, cursor) {
  return async ({ dispatch, getState, client }) => {
    if (!input) {
      return;
    }
    const frameId = getSelectedFrameId(getState(), cx.thread);
    const result = await client.autocomplete(input, cursor, frameId);
    dispatch({ type: "AUTOCOMPLETE", cx, input, result });
  };
}

export function clearAutocomplete() {
  return { type: "CLEAR_AUTOCOMPLETE" };
}

export function clearExpressionError() {
  return { type: "CLEAR_EXPRESSION_ERROR" };
}

export function updateExpression(cx, input, expression) {
  return async ({ dispatch, getState, parser }) => {
    if (!input) {
      return;
    }

    const expressionError = await parser.hasSyntaxError(input);
    dispatch({
      type: "UPDATE_EXPRESSION",
      cx,
      expression,
      input: expressionError ? expression.input : input,
      expressionError,
    });

    dispatch(evaluateExpressions(cx));
  };
}

/**
 *
 * @param {object} expression
 * @param {number} expression.id
 * @memberof actions/pause
 * @static
 */
export function deleteExpression(expression) {
  return ({ dispatch }) => {
    dispatch({
      type: "DELETE_EXPRESSION",
      input: expression.input,
    });
  };
}

/**
 *
 * @memberof actions/pause
 * @param {number} selectedFrameId
 * @static
 */
export function evaluateExpressions(cx) {
  return async function({ dispatch, getState, client }) {
    const expressions = getExpressions(getState());
    const inputs = expressions.map(({ input }) => input);
    const frameId = getSelectedFrameId(getState(), cx.thread);
    const results = await client.evaluateExpressions(inputs, {
      frameId,
      threadId: cx.thread,
    });
    dispatch({ type: "EVALUATE_EXPRESSIONS", cx, inputs, results });
  };
}

function evaluateExpression(cx, expression) {
  return async function({ dispatch, getState, client, sourceMaps }) {
    if (!expression.input) {
      console.warn("Expressions should not be empty");
      return null;
    }

    let { input } = expression;
    const frame = getSelectedFrame(getState(), cx.thread);

    if (frame) {
      const source = getLocationSource(getState(), frame.location);

      const selectedSource = getSelectedSource(getState());

      if (selectedSource && source.isOriginal && selectedSource.isOriginal) {
        const mapResult = await dispatch(getMappedExpression(input));
        if (mapResult) {
          input = mapResult.expression;
        }
      }
    }

    const frameId = getSelectedFrameId(getState(), cx.thread);

    return dispatch({
      type: "EVALUATE_EXPRESSION",
      cx,
      thread: cx.thread,
      input: expression.input,
      [PROMISE]: client.evaluate(wrapExpression(input), {
        frameId,
      }),
    });
  };
}

/**
 * Gets information about original variable names from the source map
 * and replaces all posible generated names.
 */
export function getMappedExpression(expression) {
  return async function({
    dispatch,
    getState,
    client,
    sourceMaps,
    evaluationsParser,
  }) {
    const thread = getCurrentThread(getState());
    const mappings = getSelectedScopeMappings(getState(), thread);
    const bindings = getSelectedFrameBindings(getState(), thread);

    // We bail early if we do not need to map the expression. This is important
    // because mapping an expression can be slow if the evaluationsParser
    // worker is busy doing other work.
    //
    // 1. there are no mappings - we do not need to map original expressions
    // 2. does not contain `await` - we do not need to map top level awaits
    // 3. does not contain `=` - we do not need to map assignments
    const shouldMapScopes = isMapScopesEnabled(getState()) && mappings;
    if (!shouldMapScopes && !expression.match(/(await|=)/)) {
      return null;
    }

    return evaluationsParser.mapExpression(
      expression,
      mappings,
      bindings || [],
      features.mapExpressionBindings && getIsPaused(getState(), thread),
      features.mapAwaitExpression
    );
  };
}
