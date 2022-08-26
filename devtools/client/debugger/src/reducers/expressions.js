/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Expressions reducer
 * @module reducers/expressions
 */

import { prefs } from "../utils/prefs";

export const initialExpressionState = () => ({
  expressions: restoreExpressions(),
  expressionError: false,
  autocompleteMatches: {},
  currentAutocompleteInput: null,
});

function update(state = initialExpressionState(), action) {
  switch (action.type) {
    case "ADD_EXPRESSION":
      if (action.expressionError) {
        return { ...state, expressionError: !!action.expressionError };
      }
      return appendExpressionToList(state, {
        input: action.input,
        value: null,
        updating: true,
      });

    case "UPDATE_EXPRESSION":
      const key = action.expression.input;
      const newState = updateExpressionInList(state, key, {
        input: action.input,
        value: null,
        updating: true,
      });

      return { ...newState, expressionError: !!action.expressionError };

    case "EVALUATE_EXPRESSION":
      return updateExpressionInList(state, action.input, {
        input: action.input,
        value: action.value,
        updating: false,
      });

    case "EVALUATE_EXPRESSIONS":
      const { inputs, results } = action;

      return inputs.reduce(
        (_state, input, index) =>
          updateExpressionInList(_state, input, {
            input,
            value: results[index],
            updating: false,
          }),
        state
      );

    case "DELETE_EXPRESSION":
      return deleteExpression(state, action.input);

    case "CLEAR_EXPRESSION_ERROR":
      return { ...state, expressionError: false };

    case "AUTOCOMPLETE":
      const { matchProp, matches } = action.result;

      return {
        ...state,
        currentAutocompleteInput: matchProp,
        autocompleteMatches: {
          ...state.autocompleteMatches,
          [matchProp]: matches,
        },
      };

    case "CLEAR_AUTOCOMPLETE":
      return {
        ...state,
        autocompleteMatches: {},
        currentAutocompleteInput: "",
      };
  }

  return state;
}

function restoreExpressions() {
  const exprs = prefs.expressions;
  if (!exprs.length) {
    return [];
  }

  return exprs;
}

function storeExpressions({ expressions }) {
  // Return the expressions without the `value` property
  prefs.expressions = expressions.map(({ input, updating }) => ({
    input,
    updating,
  }));
}

function appendExpressionToList(state, value) {
  const newState = { ...state, expressions: [...state.expressions, value] };

  storeExpressions(newState);
  return newState;
}

function updateExpressionInList(state, key, value) {
  const list = [...state.expressions];
  const index = list.findIndex(e => e.input == key);
  list[index] = value;

  const newState = { ...state, expressions: list };
  storeExpressions(newState);
  return newState;
}

function deleteExpression(state, input) {
  const list = [...state.expressions];
  const index = list.findIndex(e => e.input == input);
  list.splice(index, 1);
  const newState = { ...state, expressions: list };
  storeExpressions(newState);
  return newState;
}

export default update;
