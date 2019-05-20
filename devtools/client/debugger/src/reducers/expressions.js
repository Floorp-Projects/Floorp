/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/**
 * Expressions reducer
 * @module reducers/expressions
 */

import makeRecord from "../utils/makeRecord";
import { List, Map } from "immutable";
import { omit, zip } from "lodash";

import { createSelector } from "reselect";
import { prefs } from "../utils/prefs";

import type { Expression } from "../types";
import type { Selector, State } from "../reducers/types";
import type { Action } from "../actions/types";
import type { Record } from "../utils/makeRecord";

export type ExpressionState = {
  expressions: List<Expression>,
  expressionError: boolean,
  autocompleteMatches: Map<string, List<string>>,
  currentAutocompleteInput: string | null,
};

export const createExpressionState: () => Record<ExpressionState> = makeRecord({
  expressions: List(restoreExpressions()),
  expressionError: false,
  autocompleteMatches: Map({}),
  currentAutocompleteInput: null,
});

function update(
  state: Record<ExpressionState> = createExpressionState(),
  action: Action
): Record<ExpressionState> {
  switch (action.type) {
    case "ADD_EXPRESSION":
      if (action.expressionError) {
        return state.set("expressionError", !!action.expressionError);
      }
      return appendExpressionToList(state, {
        input: action.input,
        value: null,
        updating: true,
      });

    case "UPDATE_EXPRESSION":
      const key = action.expression.input;
      return updateExpressionInList(state, key, {
        input: action.input,
        value: null,
        updating: true,
      }).set("expressionError", !!action.expressionError);

    case "EVALUATE_EXPRESSION":
      return updateExpressionInList(state, action.input, {
        input: action.input,
        value: action.value,
        updating: false,
      });

    case "EVALUATE_EXPRESSIONS":
      const { inputs, results } = action;

      return zip(inputs, results).reduce(
        (newState, [input, result]) =>
          updateExpressionInList(newState, input, {
            input: input,
            value: result,
            updating: false,
          }),
        state
      );

    case "DELETE_EXPRESSION":
      return deleteExpression(state, action.input);

    case "CLEAR_EXPRESSION_ERROR":
      return state.set("expressionError", false);

    case "AUTOCOMPLETE":
      const { matchProp, matches } = action.result;

      return state
        .updateIn(["autocompleteMatches", matchProp], list => matches)
        .set("currentAutocompleteInput", matchProp);

    case "CLEAR_AUTOCOMPLETE":
      return state
        .updateIn(["autocompleteMatches", ""], list => [])
        .set("currentAutocompleteInput", "");
  }

  return state;
}

function restoreExpressions() {
  const exprs = prefs.expressions;
  if (exprs.length == 0) {
    return;
  }
  return exprs;
}

function storeExpressions({ expressions }) {
  prefs.expressions = expressions
    .map(expression => omit(expression, "value"))
    .toJS();
}

function appendExpressionToList(state: Record<ExpressionState>, value: any) {
  const newState = state.update("expressions", () => {
    return state.expressions.push(value);
  });

  storeExpressions(newState);
  return newState;
}

function updateExpressionInList(
  state: Record<ExpressionState>,
  key: string,
  value: any
) {
  const newState = state.update("expressions", () => {
    const list = state.expressions;
    const index = list.findIndex(e => e.input == key);
    return list.update(index, () => value);
  });

  storeExpressions(newState);
  return newState;
}

function deleteExpression(state: Record<ExpressionState>, input: string) {
  const index = state.expressions.findIndex(e => e.input == input);
  const newState = state.deleteIn(["expressions", index]);
  storeExpressions(newState);
  return newState;
}

const getExpressionsWrapper = state => state.expressions;

export const getExpressions: Selector<List<Expression>> = createSelector(
  getExpressionsWrapper,
  expressions => expressions.expressions
);

export const getAutocompleteMatches: Selector<
  Map<string, List<string>>
> = createSelector(
  getExpressionsWrapper,
  expressions => expressions.autocompleteMatches
);

export function getExpression(state: State, input: string) {
  return getExpressions(state).find(exp => exp.input == input);
}

export function getAutocompleteMatchset(state: State) {
  const input = state.expressions.get("currentAutocompleteInput");
  return getAutocompleteMatches(state).get(input);
}

export const getExpressionError: Selector<boolean> = createSelector(
  getExpressionsWrapper,
  expressions => expressions.expressionError
);

export default update;
