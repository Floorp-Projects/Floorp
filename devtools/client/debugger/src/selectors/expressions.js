/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { createSelector } from "devtools/client/shared/vendor/reselect";

const getExpressionsWrapper = state => state.expressions;

export const getExpressions = createSelector(
  getExpressionsWrapper,
  expressions => expressions.expressions
);

const getAutocompleteMatches = createSelector(
  getExpressionsWrapper,
  expressions => expressions.autocompleteMatches
);

export function getExpression(state, input) {
  return getExpressions(state).find(exp => exp.input == input);
}

export function getAutocompleteMatchset(state) {
  const input = state.expressions.currentAutocompleteInput;
  if (!input) {
    return null;
  }
  return getAutocompleteMatches(state)[input];
}

export const getExpressionError = createSelector(
  getExpressionsWrapper,
  expressions => expressions.expressionError
);
