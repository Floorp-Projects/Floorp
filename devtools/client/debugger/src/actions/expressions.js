/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  getExpression,
  getExpressions,
  getSelectedSource,
  getSelectedScopeMappings,
  getSelectedFrameBindings,
  getIsPaused,
  getSelectedFrame,
  getCurrentThread,
  isMapScopesEnabled,
} from "../selectors/index";
import { PROMISE } from "./utils/middleware/promise";
import { wrapExpression } from "../utils/expressions";
import { features } from "../utils/prefs";

/**
 * Add expression for debugger to watch
 *
 * @param {string} input
 */
export function addExpression(input) {
  return async ({ dispatch, getState, parserWorker }) => {
    if (!input) {
      return null;
    }

    const expressionError = await parserWorker.hasSyntaxError(input);

    // If the expression already exists, only update its evaluation
    let expression = getExpression(getState(), input);
    if (!expression) {
      // This will only display the expression input,
      // evaluateExpression will update its value.
      dispatch({ type: "ADD_EXPRESSION", input, expressionError });

      expression = getExpression(getState(), input);
      // When there is an expression error, we won't store the expression
      if (!expression) {
        return null;
      }
    }

    return dispatch(evaluateExpression(expression));
  };
}

export function autocomplete(input, cursor) {
  return async ({ dispatch, getState, client }) => {
    if (!input) {
      return;
    }
    const thread = getCurrentThread(getState());
    const selectedFrame = getSelectedFrame(getState(), thread);
    const result = await client.autocomplete(input, cursor, selectedFrame?.id);
    // Pass both selectedFrame and thread in case selectedFrame is null
    dispatch({ type: "AUTOCOMPLETE", selectedFrame, thread, input, result });
  };
}

export function clearAutocomplete() {
  return { type: "CLEAR_AUTOCOMPLETE" };
}

export function clearExpressionError() {
  return { type: "CLEAR_EXPRESSION_ERROR" };
}

export function updateExpression(input, expression) {
  return async ({ getState, dispatch, parserWorker }) => {
    if (!input) {
      return;
    }

    const expressionError = await parserWorker.hasSyntaxError(input);
    dispatch({
      type: "UPDATE_EXPRESSION",
      expression,
      input: expressionError ? expression.input : input,
      expressionError,
    });

    await dispatch(evaluateExpressionsForCurrentContext());
  };
}

/**
 *
 * @param {object} expression
 * @param {string} expression.input
 */
export function deleteExpression(expression) {
  return {
    type: "DELETE_EXPRESSION",
    input: expression.input,
  };
}

export function evaluateExpressionsForCurrentContext() {
  return async ({ getState, dispatch }) => {
    const thread = getCurrentThread(getState());
    const selectedFrame = getSelectedFrame(getState(), thread);
    await dispatch(evaluateExpressions(selectedFrame));
  };
}

/**
 * Update all the expressions by querying the server for updated values.
 *
 * @param {object} selectedFrame
 *        If defined, will evaluate the expression against this given frame,
 *        otherwise it will use the global scope of the thread.
 */
export function evaluateExpressions(selectedFrame) {
  return async function ({ dispatch, getState, client }) {
    const expressions = getExpressions(getState());
    const inputs = expressions.map(({ input }) => input);
    // Fallback to global scope of the current thread when selectedFrame is null
    const thread = selectedFrame?.thread || getCurrentThread(getState());
    const results = await client.evaluateExpressions(inputs, {
      // We will only have a specific frame when passing a Selected frame context.
      frameId: selectedFrame?.id,
      threadId: thread,
    });
    // Pass both selectedFrame and thread in case selectedFrame is null
    dispatch({
      type: "EVALUATE_EXPRESSIONS",

      selectedFrame,
      // As `selectedFrame` can be null, pass `thread` to help
      // the reducer know what is the related thread of this action.
      thread,

      inputs,
      results,
    });
  };
}

function evaluateExpression(expression) {
  return async function (thunkArgs) {
    let { input } = expression;
    if (!input) {
      console.warn("Expressions should not be empty");
      return null;
    }

    const { dispatch, getState, client } = thunkArgs;
    const thread = getCurrentThread(getState());
    const selectedFrame = getSelectedFrame(getState(), thread);

    const selectedSource = getSelectedSource(getState());
    // Only map when we are paused and if the currently selected source is original,
    // and the paused location is also original.
    if (
      selectedFrame &&
      selectedSource &&
      selectedFrame.location.source.isOriginal &&
      selectedSource.isOriginal
    ) {
      const mapResult = await getMappedExpression(
        input,
        selectedFrame.thread,
        thunkArgs
      );
      if (mapResult) {
        input = mapResult.expression;
      }
    }

    // Pass both selectedFrame and thread in case selectedFrame is null
    return dispatch({
      type: "EVALUATE_EXPRESSION",
      selectedFrame,
      // When we aren't passing a frame, we have to pass a thread to the pause reducer
      thread: selectedFrame ? null : thread,
      input: expression.input,
      [PROMISE]: client.evaluate(wrapExpression(input), {
        // When evaluating against the global scope (when not paused)
        // frameId will be null here.
        frameId: selectedFrame?.id,
      }),
    });
  };
}

/**
 * Gets information about original variable names from the source map
 * and replaces all possible generated names.
 */
export function getMappedExpression(expression, thread, thunkArgs) {
  const { getState, parserWorker } = thunkArgs;
  const mappings = getSelectedScopeMappings(getState(), thread);
  const bindings = getSelectedFrameBindings(getState(), thread);

  // We bail early if we do not need to map the expression. This is important
  // because mapping an expression can be slow if the parserWorker
  // worker is busy doing other work.
  //
  // 1. there are no mappings - we do not need to map original expressions
  // 2. does not contain `await` - we do not need to map top level awaits
  // 3. does not contain `=` - we do not need to map assignments
  const shouldMapScopes = isMapScopesEnabled(getState()) && mappings;
  if (!shouldMapScopes && !expression.match(/(await|=)/)) {
    return null;
  }

  return parserWorker.mapExpression(
    expression,
    mappings,
    bindings || [],
    features.mapExpressionBindings && getIsPaused(getState(), thread),
    features.mapAwaitExpression
  );
}
