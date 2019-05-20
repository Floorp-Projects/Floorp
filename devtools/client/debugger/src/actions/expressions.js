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
  getIsPaused,
  isMapScopesEnabled,
} from "../selectors";
import { PROMISE } from "./utils/middleware/promise";
import { wrapExpression } from "../utils/expressions";
import { features } from "../utils/prefs";
import { isOriginal } from "../utils/source";

import type { Expression, ThreadContext } from "../types";
import type { ThunkArgs } from "./types";

/**
 * Add expression for debugger to watch
 *
 * @param {object} expression
 * @param {number} expression.id
 * @memberof actions/pause
 * @static
 */
export function addExpression(cx: ThreadContext, input: string) {
  return async ({ dispatch, getState, evaluationsParser }: ThunkArgs) => {
    if (!input) {
      return;
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
  };
}

export function autocomplete(cx: ThreadContext, input: string, cursor: number) {
  return async ({ dispatch, getState, client }: ThunkArgs) => {
    if (!input) {
      return;
    }
    const frameId = getSelectedFrameId(getState(), cx.thread);
    const result = await client.autocomplete(input, cursor, frameId);
    await dispatch({ type: "AUTOCOMPLETE", cx, input, result });
  };
}

export function clearAutocomplete() {
  return { type: "CLEAR_AUTOCOMPLETE" };
}

export function clearExpressionError() {
  return { type: "CLEAR_EXPRESSION_ERROR" };
}

export function updateExpression(
  cx: ThreadContext,
  input: string,
  expression: Expression
) {
  return async ({ dispatch, getState, parser }: ThunkArgs) => {
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
export function deleteExpression(expression: Expression) {
  return ({ dispatch }: ThunkArgs) => {
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
export function evaluateExpressions(cx: ThreadContext) {
  return async function({ dispatch, getState, client }: ThunkArgs) {
    const expressions = getExpressions(getState()).toJS();
    const inputs = expressions.map(({ input }) => input);
    const frameId = getSelectedFrameId(getState(), cx.thread);
    const results = await client.evaluateExpressions(inputs, {
      frameId,
      thread: cx.thread,
    });
    dispatch({ type: "EVALUATE_EXPRESSIONS", cx, inputs, results });
  };
}

function evaluateExpression(cx: ThreadContext, expression: Expression) {
  return async function({ dispatch, getState, client, sourceMaps }: ThunkArgs) {
    if (!expression.input) {
      console.warn("Expressions should not be empty");
      return;
    }

    let input = expression.input;
    const frame = getSelectedFrame(getState(), cx.thread);

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

    const frameId = getSelectedFrameId(getState(), cx.thread);

    return dispatch({
      type: "EVALUATE_EXPRESSION",
      cx,
      thread: cx.thread,
      input: expression.input,
      [PROMISE]: client.evaluateInFrame(wrapExpression(input), {
        frameId,
        thread: cx.thread,
      }),
    });
  };
}

/**
 * Gets information about original variable names from the source map
 * and replaces all posible generated names.
 */
export function getMappedExpression(expression: string) {
  return async function({
    dispatch,
    getState,
    client,
    sourceMaps,
    evaluationsParser,
  }: ThunkArgs) {
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
