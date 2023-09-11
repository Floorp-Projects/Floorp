/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getInternalSymbols } from "./getSymbols";

function findBestMatchExpression(sourceId, tokenPos) {
  const symbols = getInternalSymbols(sourceId);
  if (!symbols) {
    return null;
  }

  const { line, column } = tokenPos;
  const { memberExpressions, identifiers, literals } = symbols;

  function matchExpression(expression) {
    const { location } = expression;
    const { start, end } = location;
    return start.line == line && start.column <= column && end.column >= column;
  }
  function matchMemberExpression(expression) {
    // For member expressions we ignore "computed" member expressions `foo[bar]`,
    // to only match the one that looks like: `foo.bar`.
    return !expression.computed && matchExpression(expression);
  }
  // Avoid duplicating these arrays and be careful about performance as they can be large
  //
  // Process member expressions first as they can be including identifiers which
  // are subset of the member expression.
  // Ex: `foo.bar` is a member expression made of `foo` and `bar` identifiers.
  return (
    memberExpressions.find(matchMemberExpression) ||
    literals.find(matchExpression) ||
    identifiers.find(matchExpression)
  );
}
export default findBestMatchExpression;
