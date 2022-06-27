/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  getSymbols,
  getLocationSource,
  getSelectedFrame,
  getCurrentThread,
} from "../../selectors";

// a is an ast location with start and end positions (line and column).
// b is a single position (line and column).
// This function tests to see if the b position
// falls within the range given in a.
function inHouseContainsPosition(a, b) {
  const bColumn = b.column || 0;
  const startsBefore =
    a.start.line < b.line ||
    (a.start.line === b.line && a.start.column <= bColumn);
  const endsAfter =
    a.end.line > b.line || (a.end.line === b.line && a.end.column >= bColumn);

  return startsBefore && endsAfter;
}

export function highlightCalls(cx) {
  return async function({ dispatch, getState, parser, client }) {
    if (!cx) {
      return;
    }

    const frame = await getSelectedFrame(
      getState(),
      getCurrentThread(getState())
    );

    if (!frame) {
      return;
    }

    const { thread } = cx;

    const originalAstScopes = await parser.getScopes(frame.location);
    if (!originalAstScopes) {
      return;
    }

    const source = getLocationSource(getState(), frame.location);
    if (!source) {
      return;
    }

    const symbols = getSymbols(getState(), source);

    if (!symbols) {
      return;
    }

    if (!symbols.callExpressions) {
      return;
    }

    const localAstScope = originalAstScopes[0];
    const allFunctionCalls = symbols.callExpressions;

    const highlightedCalls = allFunctionCalls.filter(function(call) {
      const containsStart = inHouseContainsPosition(
        localAstScope,
        call.location.start
      );
      const containsEnd = inHouseContainsPosition(
        localAstScope,
        call.location.end
      );
      return containsStart && containsEnd;
    });

    return dispatch({
      type: "HIGHLIGHT_CALLS",
      thread,
      highlightedCalls,
    });
  };
}

export function unhighlightCalls(cx) {
  return async function({ dispatch, getState, parser, client }) {
    const { thread } = cx;
    return dispatch({
      type: "UNHIGHLIGHT_CALLS",
      thread,
    });
  };
}
