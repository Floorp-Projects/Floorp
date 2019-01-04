/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import mapOriginalExpression from "./mapOriginalExpression";
import mapExpressionBindings from "./mapBindings";
import mapTopLevelAwait from "./mapAwaitExpression";

export default function mapExpression(
  expression: string,
  mappings: {
    [string]: string | null
  } | null,
  bindings: string[],
  shouldMapBindings: boolean = true,
  shouldMapAwait: boolean = true
): {
  expression: string,
  mapped: {
    await: boolean,
    bindings: boolean,
    originalExpression: boolean
  }
} {
  const mapped = {
    await: false,
    bindings: false,
    originalExpression: false
  };

  try {
    if (mappings) {
      const beforeOriginalExpression = expression;
      expression = mapOriginalExpression(expression, mappings);
      mapped.originalExpression = beforeOriginalExpression !== expression;
    }

    if (shouldMapBindings) {
      const beforeBindings = expression;
      expression = mapExpressionBindings(expression, bindings);
      mapped.bindings = beforeBindings !== expression;
    }

    if (shouldMapAwait) {
      const beforeAwait = expression;
      expression = mapTopLevelAwait(expression);
      mapped.await = beforeAwait !== expression;
    }
  } catch (e) {
    console.warn(`Error when mapping ${expression} expression:`, e);
  }

  return {
    expression,
    mapped
  };
}
