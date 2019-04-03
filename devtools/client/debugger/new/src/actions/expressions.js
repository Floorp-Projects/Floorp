/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  getExpression,
  getExpressions,
  getSelectedFrame,
  getSelectedFrameId,
  getSourceFromId,
  getSelectedSource,
  getSelectedScopeMappings,
  getSelectedFrameBindings,
  getCurrentThread,
  getIsPaused
} from "../selectors";
import { PROMISE } from "./utils/middleware/promise";
import { wrapExpression } from "../utils/expressions";
import { features } from "../utils/prefs";
import { isOriginal } from "../utils/source";

import * as parser from "../workers/parser";
import type { Expression } from "../types";
import type { ThunkArgs } from "./types";

/**
 * Add expression for debugger to watch
 *
 * @param {object} expression
 * @param {number} expression.id
 * @memberof actions/pause
 * @static
 */
export function addExpression(input: string) {
  return async ({ dispatch, getState }: ThunkArgs) => {
    if (!input) {
      return;
    }

    const expressionError = await parser.hasSyntaxError(input);

    const expression = getExpression(getState(), input);
    if (expression) {
      return dispatch(evaluateExpression(expression));
    }

    dispatch({ type: "ADD_EXPRESSION", input, expressionError });

    const newExpression = getExpression(getState(), input);
    if (newExpression) {
      return dispatch(evaluateExpression(newExpression));
    }
  };
}

export function autocomplete(input: string, cursor: number) {
  return async ({ dispatch, getState, client }: ThunkArgs) => {
    if (!input) {
      return;
    }
    const thread = getCurrentThread(getState());
    const frameId = getSelectedFrameId(getState(), thread);
    const result = await client.autocomplete(input, cursor, frameId);
    await dispatch({ type: "AUTOCOMPLETE", input, result });
  };
}

export function clearAutocomplete() {
  return { type: "CLEAR_AUTOCOMPLETE" };
}

export function clearExpressionError() {
  return { type: "CLEAR_EXPRESSION_ERROR" };
}

export function updateExpression(input: string, expression: Expression) {
  return async ({ dispatch, getState }: ThunkArgs) => {
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
export function deleteExpression(expression: Expression) {
  return ({ dispatch }: ThunkArgs) => {
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
export function evaluateExpressions() {
  return async function({ dispatch, getState, client }: ThunkArgs) {
    const expressions = getExpressions(getState()).toJS();
    const inputs = expressions.map(({ input }) => input);
    const thread = getCurrentThread(getState());
    const frameId = getSelectedFrameId(getState(), thread);
    const results = await client.evaluateExpressions(inputs, {
      frameId,
      thread
    });
    dispatch({ type: "EVALUATE_EXPRESSIONS", inputs, results });
  };
}

function evaluateExpression(expression: Expression) {
  return async function({ dispatch, getState, client, sourceMaps }: ThunkArgs) {
    if (!expression.input) {
      console.warn("Expressions should not be empty");
      return;
    }

    let input = expression.input;
    const thread = getCurrentThread(getState());
    const frame = getSelectedFrame(getState(), thread);

    if (frame) {
      const { location } = frame;
      const source = getSourceFromId(getState(), location.sourceId);

      const selectedSource = getSelectedSource(getState());

      if (selectedSource && isOriginal(source) && isOriginal(selectedSource)) {
        const mapResult = await dispatch(getMappedExpression(input));
        if (mapResult) {
          input = mapResult.expression;
        }
      }
    }

    const frameId = getSelectedFrameId(getState(), thread);

    return dispatch({
      type: "EVALUATE_EXPRESSION",
      thread,
      input: expression.input,
      [PROMISE]: client.evaluateInFrame(wrapExpression(input), {
        frameId,
        thread
      })
    });
  };
}

/**
 * Gets information about original variable names from the source map
 * and replaces all posible generated names.
 */
export function getMappedExpression(expression: string) {
  return async function({ dispatch, getState, client, sourceMaps }: ThunkArgs) {
    const state = getState();
    const thread = getCurrentThread(getState());
    const mappings = getSelectedScopeMappings(state, thread);
    const bindings = getSelectedFrameBindings(state, thread);

    // We bail early if we do not need to map the expression. This is important
    // because mapping an expression can be slow if the parser worker is
    // busy doing other work.
    //
    // 1. there are no mappings - we do not need to map original expressions
    // 2. does not contain `await` - we do not need to map top level awaits
    // 3. does not contain `=` - we do not need to map assignments
    if (!mappings && !expression.match(/(await|=)/)) {
      return null;
    }

    return parser.mapExpression(
      expression,
      mappings,
      bindings || [],
      features.mapExpressionBindings && getIsPaused(state, thread),
      features.mapAwaitExpression
    );
  };
}
